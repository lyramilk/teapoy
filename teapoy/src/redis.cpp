#include "redis.h"
#include "config.h"
#include <hiredis/hiredis.h>
#include <stdarg.h>
#include <iostream>
#include <vector>
#include <sys/poll.h>
#include <libmilk/log.h>
#include <libmilk/multilanguage.h>
#include <libmilk/netio.h>

#include <sys/socket.h>
#include "stringutil.h"
#include <string.h>
#include <errno.h>
#include <stdlib.h>

namespace lyramilk{ namespace teapoy{ namespace redis{

	exception::exception()throw()
	{
	}

	exception::exception(lyramilk::data::string msg) throw()
	{
		str = msg;
	}

	exception::~exception() throw()
	{
	}

	const char* exception::what() const throw()
	{
		return str.c_str();
	}

	
	redis_param_iterator redis_param_iterator::eof;

	redis_param_iterator::redis_param_iterator(const redis_param* parent):i(0)
	{
		if(parent && parent->type() == redis_param::t_array){
			this->parent = const_cast<redis_param*>(parent);
		}
	}

	redis_param_iterator::redis_param_iterator()
	{
		parent = NULL;
	}

	redis_param_iterator::redis_param_iterator(const redis_param_iterator &o)
	{
		parent = o.parent;
		i = o.i;
	}

	redis_param_iterator::~redis_param_iterator()
	{
	}

	redis_param_iterator& redis_param_iterator::operator =(const redis_param_iterator &o)
	{
		parent = o.parent;
		i = o.i;
		return *this;
	}

	bool redis_param_iterator::operator ==(const redis_param_iterator &o)
	{
		if(parent){
			return parent == o.parent && i == o.i;
		}
		return o.parent == NULL;
	}

	bool redis_param_iterator::operator !=(const redis_param_iterator &o)
	{
		if(parent){
			return parent != o.parent || i != o.i;
		}
		return o.parent != NULL;
	}

	redis_param_iterator& redis_param_iterator::operator ++()
	{
		if(!parent) return eof;
		i++;

		if(i < parent->size()){
			return *this;
		}
		*this = eof;
		return *this;
	}

	redis_param_iterator redis_param_iterator::operator ++(int)
	{
		redis_param_iterator tmp(*this);
		i++;
		if(i >= parent->size()){
			*this = eof;
		}
		return tmp;
	}

	redis_param redis_param_iterator::operator*()
	{
		return parent->at(i);
	}

	redis_param redis_param_iterator::operator->()
	{
		return parent->at(i);
	}

	redis_param::redis_param(void* param)
	{
		pnative = param;
	}

	redis_param::~redis_param()
	{
	}

	redis_param::param_type redis_param::type() const
	{
		if(pnative){
			redisReply* reply = (redisReply*)pnative;
			switch(reply->type){
			  case REDIS_REPLY_STRING:
				return t_string;
			  case REDIS_REPLY_ARRAY:
				return t_array;
			  case REDIS_REPLY_INTEGER:
				return t_integer;
			  case REDIS_REPLY_NIL:
				return t_nil;
			  case REDIS_REPLY_STATUS:
				return t_status;
			  case REDIS_REPLY_ERROR:
				return t_error;
			}
		}
		return t_unknow;
	}

	lyramilk::data::string redis_param::str() const throw(exception_typenotmatch,exception_invalid)
	{
		if(!pnative){
			return "null";
		}
		redisReply* reply = (redisReply*)pnative;
		if(reply->type != REDIS_REPLY_STRING && reply->type != REDIS_REPLY_ERROR && reply->type != REDIS_REPLY_STATUS) THROW(exception_typenotmatch());
		return lyramilk::data::string(reply->str,reply->len);
	}

	const char* redis_param::c_str() const throw(exception_typenotmatch,exception_invalid)
	{
		if(!pnative){
			return "null";
		}
		redisReply* reply = (redisReply*)pnative;
		if(reply->type != REDIS_REPLY_STRING && reply->type != REDIS_REPLY_ERROR && reply->type != REDIS_REPLY_STATUS) THROW(exception_typenotmatch());
		return reply->str;
	}

	redis_param::operator lyramilk::data::string () const throw(exception_typenotmatch,exception_invalid)
	{
		if(!pnative){
			return "null";
		}
		redisReply* reply = (redisReply*)pnative;
		if(reply->type != REDIS_REPLY_STRING && reply->type != REDIS_REPLY_ERROR && reply->type != REDIS_REPLY_STATUS) THROW(exception_typenotmatch());
		return lyramilk::data::string(reply->str,reply->len);
	}

	long long redis_param::toint() const throw(exception_typenotmatch,exception_invalid)
	{
		if(!pnative) THROW(exception_invalid());
		redisReply* reply = (redisReply*)pnative;
		if(reply->type != REDIS_REPLY_INTEGER) THROW(exception_typenotmatch());
		return reply->integer;
	}

	redis_param::operator long long () const throw(exception_typenotmatch,exception_invalid)
	{
		if(!pnative) THROW(exception_invalid());
		redisReply* reply = (redisReply*)pnative;
		if(reply->type != REDIS_REPLY_INTEGER) THROW(exception_typenotmatch());
		return reply->integer;
	}

	redis_param redis_param::at(unsigned long long index) const throw(exception_typenotmatch,exception_invalid,exception_outofbound)
	{
		if(!pnative) THROW(exception_invalid());
		redisReply* reply = (redisReply*)pnative;
		if(reply->type == REDIS_REPLY_ARRAY){
			if(index < reply->elements){
				return redis_param(reply->element[index]);
			}
			THROW(exception_outofbound());
		}else{
			THROW(exception_typenotmatch());
		}
	}

	unsigned long long redis_param::size() const throw(exception_typenotmatch,exception_invalid)
	{
		if(!pnative) THROW(exception_invalid());
		redisReply* reply = (redisReply*)pnative;
		if(reply->type == REDIS_REPLY_ARRAY){
			return reply->elements;
		}
		THROW(exception_typenotmatch());
	}

	redis_param::iterator redis_param::begin() const
	{
		if(!pnative) THROW(exception_invalid());
		redisReply* reply = (redisReply*)pnative;
		if(reply->elements == 0) return redis_param::iterator::eof;
		return iterator(this);
	}

	redis_param::iterator redis_param::end() const
	{
		return redis_param::iterator::eof;
	}

	redis_param* redis_param::operator->()
	{
		return this;
	}

	redis_reply::redis_reply(void* param):redis_param(param)
	{
	}

	redis_reply::redis_reply(const redis_reply& o):redis_param(NULL)
	{
		*this = o;
	}

	redis_reply& redis_reply::operator =(const redis_reply& o)
	{
		if(pnative){
			redisReply* reply = (redisReply*)pnative;
			freeReplyObject(reply);
		}
		pnative = o.pnative;
		const_cast<redis_reply&>(o).pnative = NULL;
		return *this;
	}

	redis_reply::~redis_reply()
	{
		if(pnative){
			redisReply* reply = (redisReply*)pnative;
			freeReplyObject(reply);
		}
	}

	bool redis_reply::good() const
	{
		if(!pnative) return false;
		redisReply* reply = (redisReply*)pnative;
		return reply->type != REDIS_REPLY_ERROR;
	}

	redis_client::redis_client():master(false)
	{
		ctx = NULL;
		listener = NULL;
		isssdb = false;
	}

	redis_client::redis_client(const redis_client& o):master(false)
	{
		*this = o;
	}

	redis_client& redis_client::operator =(const redis_client& o)
	{
		ctx = o.ctx;
		pwd = o.pwd;
		listener = o.listener;
		isssdb = o.isssdb;
		return *this;
	}

	redis_client::~redis_client()
	{
		close();
	}

	bool redis_client::open(const char* host,unsigned short port,lyramilk::data::string pwd)
	{
		this->host = host;
		this->port = port;
		this->pwd = pwd;
		struct timeval tv = {0};
		tv.tv_sec = 3;

		errno = 0;
		redisContext* c = redisConnectWithTimeout(host,port,tv);
		if(c){
			if(c->err){
				lyramilk::klog(lyramilk::log::error,"teapoy.redis.open") << D("连接失败：%s(%s:%d)",c->errstr,this->host.c_str(),this->port) << std::endl;
				redisFree(c);
				return false;
			}else{
				ctx = c;
				if(!pwd.empty()){
					if(!redis_client::execute("auth %s",pwd.c_str()).good()){
						return false;
					}
				}

				redis_reply ret = redis_client::execute("info");
				if(lyramilk::teapoy::redis::redis_reply::t_array == ret.type()){
					isssdb = true;
				}
				/*
				if(lyramilk::teapoy::redis::redis_reply::t_string == ret.type()){
					// redis
					isssdb = false;
				}*/
				lyramilk::klog(lyramilk::log::debug,"teapoy.redis.open") << D("建立连接：%s:%d",this->host.c_str(),this->port) << std::endl;
				struct timeval timeout = {3,0};	//3s
				redisSetTimeout(c,timeout);
				master = true;
				return true;
			}
		}
		return false;
	}

	void redis_client::close()
	{
		if(master && ctx){
			redisContext* c = (redisContext*)ctx;
			ctx = NULL;
			redisFree(c);
			master = false;
			lyramilk::klog(lyramilk::log::debug,"teapoy.redis.open") << D("关闭连接：%s:%d",this->host.c_str(),this->port) << std::endl;
		}
	}

	redis_reply redis_client::execute(const char* fmt,...)
	{
		va_list va;
		va_start(va, fmt);
		return execute(fmt,va);
	}

	redis_reply redis_client::execute(const char* fmt,va_list vaa)
	{
		if(!alive()){
			lyramilk::data::string errstr = D("在redis(%s,%d)上执行命令%s失败，原因：网络链接失败",host.c_str(),port,fmt);
			throw lyramilk::teapoy::redis::exception(errstr);
		}
		va_list va;
		va_copy(va, vaa);
		redisContext* c = (redisContext*)ctx;
		redisReply* reply = (redisReply*)redisvCommand(c,fmt,va);
		if(c->err != 0){
			lyramilk::data::string errstr = D("在redis(%s,%d)上执行命令%s失败，原因：%s",host.c_str(),port,fmt,c->errstr);
			close();
			throw lyramilk::teapoy::redis::exception(errstr);
		}
		redis_reply r(reply);
		while(listener){
			int cnt = 0;
			va_list va;
			{
				char buff[256];
				va_copy(va, vaa);

				cnt = vsnprintf(buff,256,fmt,va);
				va_end(va);
				if(cnt < 256){
					listener->notify(buff,cnt,r);
					break;
				}
			}
			std::vector<char> buf(cnt + 1);
			va_copy(va, vaa);
			cnt = vsnprintf(buf.data(),cnt,fmt,va);
			va_end(va);
			listener->notify(buf.data(),cnt,r);
			break;
		}
		return r;
	}

	redis_client_listener* redis_client::set_listener(redis_client_listener* lst)
	{
		redis_client_listener* old = listener;
		listener = lst;
		return old;
	}

	bool redis_client::alive()
	{
		if(this == NULL) return false;

		if(ctx != NULL){
			redisContext* c = (redisContext*)ctx;
			pollfd pfd;
			pfd.fd = c->fd;
			pfd.events = POLLOUT | POLLERR | POLLHUP;
			pfd.revents = 0;
			if(::poll(&pfd,1,1000) != 1){
				close();
				return ctx == NULL && open(host.c_str(),port,pwd);
			}
			if(pfd.revents != POLLOUT){
				close();
				return ctx == NULL && open(host.c_str(),port,pwd);
			}
			return true;
		}
		return open(host.c_str(),port,pwd);
	}

	int redis_client::getfd()
	{
		redisContext* c = (redisContext*)ctx;
		if(c && c->fd) return c->fd;
		return 0;
	}

	static lyramilk::data::var::array ssdb_exec2(lyramilk::netio::client& c,const lyramilk::data::var::array& ar)
	{
		lyramilk::data::var::array ret;
		lyramilk::data::stringstream ss;
		for(lyramilk::data::var::array::const_iterator it = ar.begin();it!=ar.end();++it){
			lyramilk::data::string str = it->str();
			ss << str.size() << "\n" << str << "\n";
		}
		ss << "\n";
		lyramilk::data::string str = ss.str();
		int r = c.write(str.c_str(),str.size());
		if(r < 1){
			return ret;
		}
		char buff[65535];
		r = c.read(buff,sizeof(buff));
		if(r>0){
			const char* ptr = buff;
			int len = r;
			while(len>0){
				const char* data = (const char*)memchr(ptr,'\n',r);
				if(data == nullptr) break;
				++data;
				int num = data - ptr;
				int size = strtol(ptr,nullptr,10);
				if(size > 0){
					ret.push_back(lyramilk::data::string(data,size));
				}

				len -= num + size;
				ptr += num + size;

				if(len >= 1 && ptr[0] == '\n'){
					len -= 1;
					ptr += 1;
				}else if(len >= 2 && ptr[0] == '\r' && ptr[1] == '\n'){
					len -= 2;
					ptr += 2;
				}else{
					break;
				}
			}
		}
		return ret;
	}

	lyramilk::data::var::array redis_client::ssdb_exec(const lyramilk::data::var::array& ar)
	{
		lyramilk::netio::client client;
		if(!client.open(host,port)) return lyramilk::data::var::array();
		lyramilk::data::var::array args_auth;
		args_auth.push_back("auth");
		args_auth.push_back(pwd);
		lyramilk::data::var::array ret = ssdb_exec2(client,args_auth);
		if(ret.size() < 2) return lyramilk::data::var::array();
		if(ret[0] != "ok"){
			lyramilk::data::string errstr = D("在redis(%s,%d)上执行命令失败，原因：%s",host.c_str(),port,ret[1].str().c_str());
			throw teapoy::redis::exception(errstr);
		}

		return ssdb_exec2(client,ar);
	}

	bool redis_client::is_ssdb()
	{
		return isssdb;
	}

}}}
