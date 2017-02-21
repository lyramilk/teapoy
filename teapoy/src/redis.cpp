#include "redis.h"
#include <pthread.h>
#include <stdio.h>
#include <libmilk/log.h>
#include <libmilk/multilanguage.h>
#include <libmilk/exception.h>


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
	}

	bool redis_client::open(lyramilk::data::string host,lyramilk::data::uint16 port)
	{
		this->host = host;
		this->port = port;
		if(client::open(host,port)){
			lyramilk::data::stringstream ss;
			lyramilk::netio::netaddress naddr = dest();
			ss << naddr.ip_str() << ":" << naddr.port;
			addr = ss.str();
			return true;
		}
		throw lyramilk::exception(D("redis(%s:%d)错误：网络错误",host.c_str(),port));
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

	bool redis_client::auth(lyramilk::data::string password)
	{
		pwd = password;
		lyramilk::data::var::array ar;
		ar.push_back("auth");
		ar.push_back(pwd);
		lyramilk::data::var v;
		if(exec(ar,v)){
			return true;
		}
		return false;
	}

	bool inline is_hex_digit(char c) {
		return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
	}

	char inline hex_digit_to_int(char c){
		switch(c) {
		  case '0': return 0;
		  case '1': return 1;
		  case '2': return 2;
		  case '3': return 3;
		  case '4': return 4;
		  case '5': return 5;
		  case '6': return 6;
		  case '7': return 7;
		  case '8': return 8;
		  case '9': return 9;
		  case 'a': case 'A': return 10;
		  case 'b': case 'B': return 11;
		  case 'c': case 'C': return 12;
		  case 'd': case 'D': return 13;
		  case 'e': case 'E': return 14;
		  case 'f': case 'F': return 15;
		  default: return 0;
		}
	}

	lyramilk::data::var::array uconv_split(lyramilk::data::string str)
	{
		lyramilk::data::var::array ret;
		const char *p = str.c_str();
		lyramilk::data::string current;
		current.reserve(ret.size());

		while(1) {
			while(*p && isspace(*p)) p++;
			if (*p) {
				int inq=0;
				int insq=0;
				int done=0;

				while(!done) {
					if (inq) {
						if (*p == '\\' && *(p+1) == 'x' && is_hex_digit(*(p+2)) && is_hex_digit(*(p+3))){
							unsigned char byte;
							byte = (hex_digit_to_int(*(p+2))*16) + hex_digit_to_int(*(p+3));
							current.push_back(byte);
							p += 3;
						} else if (*p == '\\' && *(p+1)) {
							char c;
							p++;
							switch(*p) {
							  case 'n': c = '\n'; break;
							  case 'r': c = '\r'; break;
							  case 't': c = '\t'; break;
							  case 'b': c = '\b'; break;
							  case 'a': c = '\a'; break;
							  default: c = *p; break;
							}
							current.push_back(c);
						} else if (*p == '"') {
						if (*(p+1) && !isspace(*(p+1))) throw lyramilk::exception(D("redis错误：响应格式错误"));
							done=1;
						} else if (!*p) {
							throw lyramilk::exception(D("redis错误：响应格式错误"));
						} else {
							current.push_back(*p);
						}
					} else if (insq) {
						if (*p == '\\' && *(p+1) == '\'') {
							p++;
							current.push_back('\'');
						} else if (*p == '\'') {
						if (*(p+1) && !isspace(*(p+1))) throw lyramilk::exception(D("redis错误：响应格式错误"));
							done=1;
						} else if (!*p) {
							throw lyramilk::exception(D("redis错误：响应格式错误"));
						} else {
							current.push_back(*p);
						}
					} else {
						switch(*p) {
						  case ' ':
						  case '\n':
						  case '\r':
						  case '\t':
						  case '\0':
							done=1;
							break;
						  case '"':
							inq=1;
							break;
						  case '\'':
							insq=1;
							break;
						  default:
							current.push_back(*p);
							break;
						}
					}
					if (*p) p++;
				}
				ret.push_back(current);
				current.clear();
			} else {
				return ret;
			}
		}
	}

	lyramilk::data::string uconv(lyramilk::data::string str)
	{
		const char *p = str.c_str();
		std::size_t sz = str.size();
		const char *e = p + sz + 1;

		lyramilk::data::string current;
		current.reserve(sz);

		enum {
			s_0,
			s_str1,
			s_str2,
		}s = s_0;

		for(;*p && p < e;++p){
			switch(s){
			  case s_0:{
				switch(*p) {
				  case '"':
					s = s_str1;
					break;
				  case '\'':
					s = s_str2;
					break;
				}
				current.push_back(*p);
			  }break;
			  case s_str1:{
				if(*p == '\\' && *(p+1) == 'x' && is_hex_digit(*(p+2)) && is_hex_digit(*(p+3))){
					unsigned char byte;
					byte = (hex_digit_to_int(*(p+2))*16) + hex_digit_to_int(*(p+3));
					current.push_back(byte);
					p+=3;
				}else if (*p == '\\' && *(p+1)) {
					char c;
					p++;
					switch(*p) {
					  case 'n': c = '\n'; break;
					  case 'r': c = '\r'; break;
					  case 't': c = '\t'; break;
					  case 'b': c = '\b'; break;
					  case 'a': c = '\a'; break;
					  default: c = *p; break;
					}
					current.push_back(c);
				} else {
					current.push_back(*p);
					if(*p == '"') s = s_0;
				}
			  }break;
			  case s_str2:{
				if (*p == '\\' && *(p+1) == '\''){
					p++;
					current.push_back('\'');
				}else{
					current.push_back(*p);
					if(*p == '\'') s = s_0;
				}
			  }break;
			}
		}
		return current;
	}

	bool redis_client::parse(lyramilk::data::stringstream& is,lyramilk::data::var& v)
	{
		char c;
		if(is.get(c)){
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

					lyramilk::data::var::array& ar = v;
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
					v = uconv(str);
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
					v = uconv(str);
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
		throw lyramilk::exception(D("redis(%s)错误：网络错误",addr.c_str()));
	}

	bool redis_client::exec(const lyramilk::data::var::array& cmd,lyramilk::data::var& ret)
	{
		if(!isalive()){
			reconnect();
		}
		lyramilk::data::var::array::const_iterator it = cmd.begin();
		lyramilk::netio::socket_stream ss(*this);
		ss << "*" << cmd.size() << "\r\n";
		for(;it!=cmd.end();++it){
			lyramilk::data::string str = it->str();
			ss << "$" << str.size() << "\r\n";
			ss << str << "\r\n";
		}
		ss.flush();
		bool suc = parse(ss,ret);
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
		lyramilk::netio::socket_stream ss(*p);
		while(true){
			lyramilk::data::var v;
			if(p->parse(ss,v)){
				if(h && !h(true,v,args)) break;
			}else{
				if(h && !h(false,v,args)) break;
			}
		}
		COUT << "被break了" << std::endl;
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

	bool redis_client::async_exec(const lyramilk::data::var::array& cmd,redis_client_watch_handler h,void* param,bool thread_join)
	{
		{
			lyramilk::data::var::array::const_iterator it = cmd.begin();
			lyramilk::netio::socket_stream ss(*this);
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
			lyramilk::data::var::array ar;
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

	void redis_client::default_listener(const lyramilk::data::string& addr,const lyramilk::data::var::array& cmd,bool success,const lyramilk::data::var& ret)
	{
		lyramilk::data::stringstream ss;

		lyramilk::data::string logcmd;
		lyramilk::data::var::array::const_iterator it = cmd.begin();
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
