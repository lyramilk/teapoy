#include "redis.h"
#include <pthread.h>
#include <stdio.h>
#include <libmilk/log.h>
#include <libmilk/dict.h>
#include <libmilk/exception.h>
#include <string.h>
#include <errno.h>


namespace lyramilk{ namespace teapoy{ namespace redis{
	////////////////
	void redis_client::reconnect()
	{
		close();
		if(open(host,port)){
			auth(pwd);
		}
	}

	redis_client::redis_client()
	{
		listener = nullptr;
		type = t_unknow;
	}

	redis_client::~redis_client()
	{
		close();
	}

	bool redis_client::open(const lyramilk::data::string& host,lyramilk::data::uint16 port)
	{
		this->host = host;
		this->port = port;

		if(client::open(this->host,this->port)){
			lyramilk::data::stringstream ss;
			lyramilk::netio::netaddress naddr = dest();
			ss << naddr.ip_str() << ":" << naddr.port;
			addr = ss.str();
			return true;
		}
		throw lyramilk::exception(D("redis(%s:%d)网络错误：%s\n",this->host.c_str(),this->port,strerror(errno)) );
		return false;
	}

	bool redis_client::close()
	{
		if(client::close()){
			addr.clear();
			return true;
		}
		return false;
	}

	void redis_client::set_listener(redis_client_listener lst)
	{
		listener = lst;
	}

	bool redis_client::auth(const lyramilk::data::string& password)
	{
		pwd = password;
		lyramilk::data::array ar;
		ar.push_back("auth");
		ar.push_back(pwd);
		lyramilk::data::var v;
		if(exec(ar,v)){
			return true;
		}
		return false;
	}

	bool redis_client::parse(lyramilk::data::istream& is,lyramilk::data::var& v)
	{
		char c;
		for(is.get(c);is && (c == '\r' || c == '\n');is.get(c));
		if(is){
			switch(c){
			  case '*':{
					lyramilk::data::string slen;
					slen.reserve(256);
					while(is.get(c)){
						if(c == '\r') continue;
						if(c == '\n') break;
						slen.push_back(c);
					}

					lyramilk::data::var len = slen;
					int ilen = len;
					v.type(lyramilk::data::var::t_array);

					lyramilk::data::array& ar = v;
					ar.resize(ilen);

					lyramilk::data::var* e = ar.data();
					for(int i=0;i<ilen;++i,++e){
						parse(is,*e);
					}
					return true;
				}
				break;
			  case '$':{
					lyramilk::data::string slen;
					slen.reserve(256);
					while(is.get(c)){
						if(c == '\r') continue;
						if(c == '\n') break;
						slen.push_back(c);
					}
					if(slen == "-1") return true;
					lyramilk::data::var len = slen;
					int ilen = len;
					lyramilk::data::string buf;
					buf.resize(ilen);
					is.read((char*)buf.c_str(),ilen);
					buf.erase(buf.begin() + is.gcount(),buf.end());
					v = buf;
					char buff[2];
					is.read(buff,2);
					return true;
				}
				break;
			  case '+':{
					lyramilk::data::string str;
					str.reserve(4096);
					while(is.get(c)){
						if(c == '\r') continue;
						if(c == '\n') break;
						str.push_back(c);
					}
					v = str;
					return true;
				}
				break;
			  case '-':{
					lyramilk::data::string str;
					str.reserve(4096);
					while(is.get(c)){
						if(c == '\r') continue;
						if(c == '\n') break;
						str.push_back(c);
					}
					v = str;
					return false;
				}
				break;
			  case ':':{
					lyramilk::data::string str;
					str.reserve(4096);
					while(is.get(c)){
						if(c == '\r') continue;
						if(c == '\n') break;
						str.push_back(c);
					}

					/* 断言这个数字恒为非负整数，如果不是需要修改代码。 */
					v = str;
					v.type(lyramilk::data::var::t_int);
					return true;
				}
				break;
			  default:
				throw lyramilk::exception(D("redis(%s)错误：响应格式错误",addr.c_str()));
			}
		}
		throw lyramilk::exception(D("redis(%s)错误：协议错误",addr.c_str()));
	}

	bool redis_client::exec(const lyramilk::data::array& cmd,lyramilk::data::var& ret)
	{
		if(!isalive()){
			lyramilk::klog(lyramilk::log::error,"redis_client::exec") << D("redis(%s:%d)断线重连",host.c_str(),port) << std::endl;
			reconnect();
		}
		lyramilk::data::array::const_iterator it = cmd.begin();
		lyramilk::netio::socket_ostream ss(this);
		ss << "*" << cmd.size() << "\r\n";
		for(;it!=cmd.end();++it){
			lyramilk::data::string str = it->str();
			ss << "$" << str.size() << "\r\n";
			ss << str << "\r\n";
		}
		ss.flush();
		lyramilk::netio::socket_istream iss(this);
		bool suc = false;
		
		try{
			suc = parse(iss,ret);
		}catch(std::exception& e){
			close();
			throw e;
		}
		if(!suc){
			if(ret.type() == lyramilk::data::var::t_str){
				lyramilk::data::string str = ret.str();
				if(str.find("NOAUTH") != str.npos){
					close();
					throw lyramilk::exception(D("redis(%s:%d)错误：%s",host.c_str(),port,str.c_str()));
					return false;
				}
			}
		}
		if(listener)listener(addr,cmd,suc,ret);
		return suc;
	}

	struct thread_redis_client_task_args
	{
		redis_client* p;
		redis_client_watch_handler h;
		void* args;
	};

	static void thread_redis_client_task(redis_client* p,redis_client_watch_handler h,void* args)
	{
		lyramilk::netio::socket_istream ss(p);
		while(true){
			lyramilk::data::var v;
			if(p->parse(ss,v)){
				if(h && !h(true,v,args)) break;
			}else{
				if(h && !h(false,v,args)) break;
			}
		}
	}

	static void* thread_redis_client_task2(void* param)
	{
		thread_redis_client_task_args* args = (thread_redis_client_task_args*)param;
		redis_client* p = args->p;
		redis_client_watch_handler h = args->h;
		void* a = args->args;
		delete args;
		thread_redis_client_task(p,h,a);
		return nullptr;
	}

	bool redis_client::async_exec(const lyramilk::data::array& cmd,redis_client_watch_handler h,void* param,bool thread_join)
	{
		{
			lyramilk::data::array::const_iterator it = cmd.begin();
			lyramilk::netio::socket_ostream ss(this);
			ss << "*" << cmd.size() << "\r\n";
			for(;it!=cmd.end();++it){
				lyramilk::data::string str = it->str();
				ss << "$" << str.size() << "\r\n";
				ss << str << "\r\n";
			}
			ss.flush();
		}
		if(thread_join){
			thread_redis_client_task(this,h,param);
			return true;
		}else{
			pthread_t thread;
			thread_redis_client_task_args* args = new thread_redis_client_task_args();
			args->p = this;
			args->h = h;
			args->args = param;
			if(pthread_create(&thread,NULL,(void* (*)(void*))thread_redis_client_task2,args) == 0){
				pthread_detach(thread);
				return true;
			}
			return false;
		}
	}

	bool redis_client::is_ssdb()
	{
		if(t_unknow == type){
			lyramilk::data::array ar;
			ar.push_back("info");
			lyramilk::data::var v;
			if(exec(ar,v)){
				if(v.type() == lyramilk::data::var::t_array && v[0] == "ssdb-server"){
					type = t_ssdb;
				}
				if(v.type() == lyramilk::data::var::t_str){
					lyramilk::data::string str = v.str();
					if(str.find("redis_version") != str.npos){
						type = t_redis;
					}
				}
			}
		}
		return t_ssdb == type;
	}

	bool redis_client::testcmd(const lyramilk::data::string& cmd)
	{
		std::map<lyramilk::data::string,bool>::iterator it = support_cmds.find(cmd);
		if(it != support_cmds.end()){
			return it->second;
		}

		lyramilk::data::array ar;
		ar.push_back(cmd);
		lyramilk::data::var v;
		if(exec(ar,v)){
			support_cmds[cmd] = true;
			return true;
		}

		lyramilk::data::string str = v.str();
		if(str.find("ERR unknown command") == str.npos && str.find("Unknown Command") == str.npos){
			support_cmds[cmd] = true;
			return true;
		}

		support_cmds[cmd] = false;
		return false;
	}

	lyramilk::data::string redis_client::version()
	{
		if(!ver.empty()) return ver;
		lyramilk::data::array ar;
		ar.push_back("info");
		//ar.push_back("server");
		lyramilk::data::var v;
		if(exec(ar,v)){
			if(v.type() == lyramilk::data::var::t_array && v[0] == "ssdb-server"){
				type = t_ssdb;
			}else if(v.type() == lyramilk::data::var::t_str){
				lyramilk::data::string str = v.str();
				std::size_t pos_redis_version = str.find("redis_version:");
				if(pos_redis_version != str.npos){
					type = t_redis;
					pos_redis_version += sizeof("redis_version:") - 1;
					std::size_t pos_redis_version_eof = str.find_first_not_of("1234567890.",pos_redis_version);
					ver = str.substr(pos_redis_version,pos_redis_version_eof-pos_redis_version);
				}
			}
		}
		return ver;
	}

	void redis_client::default_listener(const lyramilk::data::string& addr,const lyramilk::data::array& cmd,bool success,const lyramilk::data::var& ret)
	{
		lyramilk::data::stringstream ss;

		lyramilk::data::string logcmd;
		lyramilk::data::array::const_iterator it = cmd.begin();
		for(;it!=cmd.end();++it){
			logcmd += it->str() + ' ';
		}

		ss << addr << "执行" << logcmd << (success?"成功：":"失败，原因：") << "返回值" << ret;
		if(success){
			lyramilk::klog(lyramilk::log::debug,"teapoy.redis.log") << ss.str() << std::endl;
		}else{
			lyramilk::klog(lyramilk::log::error,"teapoy.redis.log") << ss.str() << std::endl;
		}
	}

}}}
