#include "redisX.h"
#include <pthread.h>
#include <stdio.h>
/*
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
#include <stdlib.h>*/


#include <libmilk/log.h>
#include <libmilk/multilanguage.h>
#include <libmilk/exception.h>


namespace lyramilk{ namespace teapoy{ namespace redisX{
	////////////////
	redis_client::redis_client()
	{
	}

	redis_client::~redis_client()
	{
	}

	bool redis_client::auth(lyramilk::data::string password)
	{
		lyramilk::data::var::array ar;
		ar.push_back("auth");
		lyramilk::data::var v;
		exec(ar,v);
		return true;
	}

	char inline hex(char a){
		if(a >= '0' && a <= '9') return a - '0';
		if(a >= 'a' && a <= 'f') return a - 'a' + 10;
		if(a >= 'A' && a <= 'F') return a - 'A' + 10;
		return 0;
	}

	lyramilk::data::string uconv(lyramilk::data::string str)
	{
		lyramilk::data::string ret;
		lyramilk::data::string::iterator it = str.begin();
		for(;it!=str.end();++it){
			if(*it != '\\'){
				ret.push_back(*it);
			}else{
				++it;
				if(*it == 'r'){
					ret.push_back('\r');
				}else if(*it == 'n'){
					ret.push_back('\n');
				}else if(*it == 'x'){
					++it;
					char ca = *it;
					++it;
					char cb = *it;
					ret.push_back(hex(cb) | (hex(ca) << 4));
				}
			}
		}
		return ret;
	}

	inline bool parse(lyramilk::data::stringstream& is,lyramilk::data::var& v)
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
					for(int i=0;i<ilen;++i){
						parse(is,ar[i]);
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

					v = uconv(str);
					v.type(lyramilk::data::var::t_int64);
					return true;
				}
				break;
			  default:
				throw lyramilk::exception(D("redis错误：响应格式错误"));
			}
		}
		throw lyramilk::exception(D("redis错误：网络错误"));
	}

	bool redis_client::exec(const lyramilk::data::var::array& cmd,lyramilk::data::var& ret)
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
		return parse(ss,ret);
	}

	struct thread_redis_client_task_args
	{
		redis_client* p;
		redis_client_watch_handler h;
	};

	static void thread_redis_client_task(redis_client* p,redis_client_watch_handler h)
	{
		lyramilk::netio::socket_stream ss(*p);
		while(true){
			lyramilk::data::var v;
			if(parse(ss,v)){
				if(h && !h(true,v)) break;
			}else{
				if(h && !h(false,v)) break;
			}
		}
		COUT << "被break了" << std::endl;
	}

	static void* thread_redis_client_task2(void* param)
	{
		thread_redis_client_task_args* args = (thread_redis_client_task_args*)param;
		redis_client* p = args->p;
		redis_client_watch_handler h = args->h;
		delete args;
		thread_redis_client_task(p,h);
		return nullptr;
	}

	bool redis_client::async_exec(const lyramilk::data::var::array& cmd,redis_client_watch_handler h,bool thread_join)
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
			thread_redis_client_task(this,h);
			return true;
		}else{
			pthread_t thread;
			thread_redis_client_task_args* args = new thread_redis_client_task_args();
			args->p = this;
			args->h = h;
			if(pthread_create(&thread,NULL,(void* (*)(void*))thread_redis_client_task2,args) == 0){
				pthread_detach(thread);
				return true;
			}
			return false;
		}
	}
}}}
