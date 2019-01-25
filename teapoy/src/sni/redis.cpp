#include "script.h"
#include "env.h"
#include "stringutil.h"
#include <cassert>
#include <libmilk/var.h>
#include <iostream>
#include <libmilk/log.h>
#include <libmilk/dict.h>
#include <libmilk/factory.h>
#include "redis.h"
#include <queue>
#include <sys/socket.h>
#include <stdlib.h>

#pragma push_macro("D")
#undef D
#include "dbconnpool.h"
#pragma pop_macro("D")

namespace lyramilk{ namespace teapoy{ namespace native {
	using namespace lyramilk::teapoy::redis;
#ifdef __GNUC__
	static inline long long __rdtsc()
	{
		long long x = 0;
		__asm__ volatile ("rdtsc" : "=A" (x));
		return x;
	}
#endif

	class redis:public lyramilk::script::sclass
	{
	  public:
		lyramilk::log::logss log;
		redis_client* c;
		bool isreadonly;
		redis_clients::ptr pp;

		static lyramilk::script::sclass* ctr(const lyramilk::data::array& args)
		{
			bool readonly = false;
			redis_client* c = nullptr;
			if(args.size() == 2){
				if(args[0].type() == lyramilk::data::var::t_user){
					const void* p = args[0].userdata("__redis_client");
					if(p){
						redis_clients::ptr* pp = (redis_clients::ptr*)p;
						redis_clients::ptr ppr = *pp;
						if(ppr){
							return new redis(ppr,args[1]);
						}
					}
					throw lyramilk::exception(D("redis错误： 无法从池中获取到命名对象。"));
					return nullptr;
				}

				c = new lyramilk::teapoy::redis::redis_client();
				c->open(args[0].str(),args[1]);
			}else if(args.size() == 3){
				c = new lyramilk::teapoy::redis::redis_client();
				c->open(args[0].str(),args[1]);
				c->auth(args[2]);
			}else if(args.size() == 4){
				readonly = args[3];
				c = new lyramilk::teapoy::redis::redis_client();
				c->open(args[0].str(),args[1]);
				c->auth(args[2]);
			}else if(args.size() == 5){
				readonly = args[3];
				c = new lyramilk::teapoy::redis::redis_client();
				c->open(args[0].str(),args[1]);
				c->auth(args[2]);

				if(args[4]){
					c->set_listener(lyramilk::teapoy::redis::redis_client::default_listener);
				}
			}else if(args.size() == 1){
				const lyramilk::data::var& v = args[0];
				lyramilk::data::string host = v["host"];
				lyramilk::data::uint16 port = v["port"];
				lyramilk::data::string pwd = v["password"];
				lyramilk::data::var vrdonly = v["readonly"];

				c = new lyramilk::teapoy::redis::redis_client();
				c->open(host.c_str(),port);
				c->auth(pwd);

				bool withlistener = v["listener"].type_like(lyramilk::data::var::t_str);
				if(vrdonly.type_like(lyramilk::data::var::t_bool)){
					readonly = vrdonly;
				}
				if(withlistener){
					c->set_listener(lyramilk::teapoy::redis::redis_client::default_listener);
				}
			}
			if(c){
				if(!c->isalive()){
					delete c;
					lyramilk::data::var v(args);
					throw lyramilk::exception(D("redis错误：网络连接失败。(%s)",v.str().c_str()));
				}
				redis* p = new redis(c);
				p->readonly(readonly);
				return p;
			}
			return nullptr;
		}
		static void dtr(lyramilk::script::sclass* p)
		{
			delete (redis*)p;
		}

		redis(redis_client* p):log(lyramilk::klog,"teapoy.native.redis")
		{
			c = p;
			isreadonly = false;
		}

		redis(redis_clients::ptr ptr,bool rdonly):log(lyramilk::klog,"teapoy.native.redis")
		{
			pp = ptr;
			c = pp;
			isreadonly = rdonly;
		}

		~redis()
		{
			if(!pp){
				delete c;
			}
		}

		bool is_ssdb()
		{
			return c && c->is_ssdb();
		}

		bool readonly()
		{
			return isreadonly;
		}

		void readonly(bool b)
		{
			isreadonly = b;
		}

		lyramilk::data::var exec(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			lyramilk::data::var vret;
			if(!c->exec(args,vret)){
				//return lyramilk::data::var::nil;
				throw lyramilk::exception(D("redis.%s执行命令%s错误：%s",__FUNCTION__,lyramilk::data::var(args).str().c_str(),vret.str().c_str()));
			}
			return vret;
		}

		lyramilk::data::var clear(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			if(readonly()) throw lyramilk::exception(D("redis.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			lyramilk::data::array cmd;
			cmd.push_back("flushdb");
			lyramilk::data::var vret;
			if(!c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		lyramilk::data::var readonly(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			if(args.size() > 0){
				MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_bool);
				isreadonly = args[0];
			}
			return isreadonly;
		}

		lyramilk::data::var newid(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			if(readonly()) throw lyramilk::exception(D("redis.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			if(args.size() == 0){
				lyramilk::data::array cmd;
				cmd.reserve(2);
				cmd.push_back("incr");
				cmd.push_back("teapoy:auto_increment::");
				lyramilk::data::var vret;
				if(!c->exec(cmd,vret)) return lyramilk::data::var::nil;
				return vret;
			}
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string skey = args[0];
			lyramilk::data::array cmd;
			cmd.reserve(2);
			cmd.push_back("incr");
			cmd.push_back("teapoy:auto_increment:" + skey);
			lyramilk::data::var vret;
			if(!c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		lyramilk::data::var ok(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			TODO();
		}

		lyramilk::data::var kvget(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::array cmd;
			cmd.reserve(2);
			cmd.push_back("get");
			cmd.push_back(args[0]);
			lyramilk::data::var vret;
			if(!c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		lyramilk::data::var kvset(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			if(readonly()) throw lyramilk::exception(D("redis.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_str);
			lyramilk::data::array cmd;
			cmd.reserve(3);
			cmd.push_back("set");
			cmd.push_back(args[0]);
			cmd.push_back(args[1]);
			lyramilk::data::var vret;
			if(!c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		lyramilk::data::var kv(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string key = args[0];
			lyramilk::script::engine* e = (lyramilk::script::engine*)env.find(lyramilk::script::engine::s_env_engine())->second.userdata(lyramilk::script::engine::s_env_engine());
			
			lyramilk::data::array ar;
			ar.reserve(2);
			ar.push_back(lyramilk::data::var("redis",this));
			ar.push_back(key);
			return e->createobject("redis.kv",ar);
		}

		lyramilk::data::var hashmap(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string key = args[0];
			lyramilk::script::engine* e = (lyramilk::script::engine*)env.find(lyramilk::script::engine::s_env_engine())->second.userdata(lyramilk::script::engine::s_env_engine());
			
			lyramilk::data::array ar;
			ar.reserve(2);
			ar.push_back(lyramilk::data::var("redis",this));
			ar.push_back(key);
			return e->createobject("redis.hashmap",ar);
		}

		lyramilk::data::var zset(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string key = args[0];
			lyramilk::script::engine* e = (lyramilk::script::engine*)env.find(lyramilk::script::engine::s_env_engine())->second.userdata(lyramilk::script::engine::s_env_engine());
			
			lyramilk::data::array ar;
			ar.reserve(2);
			ar.push_back(lyramilk::data::var("redis",this));
			ar.push_back(key);
			return e->createobject("redis.zset",ar);
		}

		lyramilk::data::var list(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string key = args[0];
			lyramilk::script::engine* e = (lyramilk::script::engine*)env.find(lyramilk::script::engine::s_env_engine())->second.userdata(lyramilk::script::engine::s_env_engine());
			
			lyramilk::data::array ar;
			ar.reserve(2);
			ar.push_back(lyramilk::data::var("redis",this));
			ar.push_back(key);
			return e->createobject("redis.list",ar);
		}


		struct redis_args
		{
			const lyramilk::data::array& param;
			lyramilk::script::engine* eng;
		};


		lyramilk::data::strings static split_redis_string(lyramilk::data::string str)
		{
			const char *p = str.c_str();
			std::size_t sz = str.size();
			const char *e = p + sz + 1;

			lyramilk::data::strings ret;
			lyramilk::data::string current;
			current.reserve(sz);
			enum {
				s_0,
				s_str1,
				s_str2,
			}s = s_0;
			while(*p && isspace(*p) && p < e) p++;
			for(;*p && p < e;++p){
				switch(s){
				  case s_0:{
					switch(*p) {
					  case ' ':
					  case '\n':
					  case '\r':
					  case '\t':
					  case '\0':
						if(!current.empty()) ret.push_back(current);
						current.clear();
						for(++p;*p && isspace(*p) && p < e;++p);
						--p;
						continue;
					  case '"':
						s = s_str1;
						current.clear();
						break;
					  case '\'':
						s = s_str2;
						current.clear();
						break;
					  default:
						current.push_back(*p);
					}
				  }break;
				  case s_str1:{
					if(*p == '"'){
						s = s_0;
						if(!current.empty()) ret.push_back(current);
						current.clear();
						for(++p;*p && isspace(*p) && p < e;++p);
						--p;
					}else{
						current.push_back(*p);
					}
				  }break;
				  case s_str2:{
					if(*p == '\''){
						s = s_0;
						if(!current.empty()) ret.push_back(current);
						current.clear();
						for(++p;*p && isspace(*p) && p < e;++p);
						--p;
					}else{
						current.push_back(*p);
					}
				  }break;
				}
			}
			if(!current.empty()) ret.push_back(current);
			return ret;
		}

		static bool callback_monitor(bool success,const lyramilk::data::var& v,void* param)
		{
			if(v.type() != lyramilk::data::var::t_str){
				throw lyramilk::exception(D("监听错误"));
			}
			lyramilk::data::strings rets = split_redis_string(v.str());

			if(rets.size() < 4) return true;

			lyramilk::data::string time = rets[0];
			lyramilk::data::string db = rets[1].substr(1);
			rets[2].erase(rets[2].end() - 1);
			lyramilk::data::string addr = rets[2];
			lyramilk::data::string cmd = lyramilk::teapoy::lowercase(rets[3]);
			redis_args* p = (redis_args*)param;
			lyramilk::data::array ar;
			ar.reserve(4);
			lyramilk::data::array cmds;
			cmds.reserve(rets.size() - 3);
			cmds.push_back(cmd);
			for(lyramilk::data::strings::iterator it = rets.begin()+4;it!=rets.end();++it){
				cmds.push_back(*it);
			}
			ar.push_back(time);
			ar.push_back(db);
			ar.push_back(addr);
			ar.push_back(cmds);
			p->eng->call(p->param[0],ar);
			return true;
		}

		lyramilk::data::var monitor(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			if(c->is_ssdb()){
				throw lyramilk::exception(D("ssdb不支持monitor操作"));
			}

			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_user);
			lyramilk::script::engine* e = (lyramilk::script::engine*)env.find(lyramilk::script::engine::s_env_engine())->second.userdata(lyramilk::script::engine::s_env_engine());
			lyramilk::data::array ar;
			ar.push_back("monitor");
			redis_args rparam = {param:args,eng:e};
			c->async_exec(ar,callback_monitor,&rparam,true);
			throw lyramilk::exception(D("不可能执行的分支"));
		}

		static bool callback_subscribe(bool success,const lyramilk::data::var& v,void* param)
		{
			redis_args* p = (redis_args*)param;
			lyramilk::data::array ar;
			ar.push_back(v);
			p->eng->call(p->param[1],ar);
			return true;
		}

		lyramilk::data::var subscribe(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			if(c->is_ssdb()){
				throw lyramilk::exception(D("ssdb不支持subscribe操作"));
			}

			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_array);
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_user);
			lyramilk::script::engine* e = (lyramilk::script::engine*)env.find(lyramilk::script::engine::s_env_engine())->second.userdata(lyramilk::script::engine::s_env_engine());
			const lyramilk::data::array& args0 = args[0];
			lyramilk::data::array ar;
			ar.push_back("subscribe");
			ar.insert(ar.end(),args0.begin(),args0.end());
			redis_args rparam = {param:args,eng:e};
			c->async_exec(ar,callback_subscribe,&rparam,true);
			throw lyramilk::exception(D("不可能执行的分支"));
		}

		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["readonly"] = lyramilk::script::engine::functional<redis,&redis::readonly>;
			fn["exec"] = lyramilk::script::engine::functional<redis,&redis::exec>;
			fn["clear"] = lyramilk::script::engine::functional<redis,&redis::clear>;
			fn["newid"] = lyramilk::script::engine::functional<redis,&redis::newid>;
			fn["ok"] = lyramilk::script::engine::functional<redis,&redis::ok>;
			fn["kvget"] = lyramilk::script::engine::functional<redis,&redis::kvget>;
			fn["kvset"] = lyramilk::script::engine::functional<redis,&redis::kvset>;
			fn["kv"] = lyramilk::script::engine::functional<redis,&redis::kv>;
			fn["hashmap"] = lyramilk::script::engine::functional<redis,&redis::hashmap>;
			fn["zset"] = lyramilk::script::engine::functional<redis,&redis::zset>;
			fn["list"] = lyramilk::script::engine::functional<redis,&redis::list>;
			fn["monitor"] = lyramilk::script::engine::functional<redis,&redis::monitor>;
			fn["subscribe"] = lyramilk::script::engine::functional<redis,&redis::subscribe>;
			p->define("Redis",fn,redis::ctr,redis::dtr);
			return 1;
		}
	};

	class redis_zset_iterator:public lyramilk::script::sclass
	{
		lyramilk::log::logss log;
		redis* predis;

		lyramilk::data::uint64 start;
		lyramilk::data::uint64 cursor;
		lyramilk::data::uint64 unit;

		lyramilk::data::string key;
		lyramilk::data::string zset_item_key;
		lyramilk::data::var zset_item_score;
		bool isok;
		bool reverse;
		std::queue<std::pair<lyramilk::data::string,lyramilk::data::var> > dq;
	  public:
		static lyramilk::script::sclass* ctr(const lyramilk::data::array& args)
		{
			return new redis_zset_iterator((redis*)args[0].userdata("redis"),args[1],args[2],args[3],args[4]);
		}
		static void dtr(lyramilk::script::sclass* p)
		{
			delete (redis_zset_iterator*)p;
		}

		redis_zset_iterator(redis* pc,lyramilk::data::string redis_key,bool reverse,lyramilk::data::uint64 start,lyramilk::data::uint64 unit):log(lyramilk::klog,"teapoy.native.redis.sorted_set.iterator")
		{
			this->reverse = reverse;
			predis = pc;
			this->key = redis_key;
			isok = false;
			this->start = start;
			this->cursor = start;
			this->unit = unit;
		}

		virtual ~redis_zset_iterator()
		{
		}

		bool sync()
		{
			if(!dq.empty()) return true;
			lyramilk::data::array cmd;
			lyramilk::data::array cmd2;
			cmd.reserve(8);
			cmd2.reserve(3);
			if(this->reverse){
				cmd.push_back("zrevrangebyscore");
				cmd2.push_back("zrevrank");
				cmd.push_back(key);
				cmd.push_back("+inf");
				cmd.push_back("-inf");
			}else{
				cmd.push_back("zrangebyscore");
				cmd2.push_back("zrank");
				cmd.push_back(key);
				cmd.push_back("-inf");
				cmd.push_back("+inf");
			}
			cmd.push_back("WITHSCORES");
			cmd.push_back("limit");
			cmd.push_back(cursor);
			cmd.push_back(unit);
			lyramilk::data::var vret;
			if(predis->c->exec(cmd,vret)){
				lyramilk::data::array& ret = vret;
				for(std::size_t i=0;i<ret.size();i+=2){
					lyramilk::data::string k = ret[i];
					lyramilk::data::string s = ret[i+1];
					lyramilk::data::var v = s;
					if(s.find('.') == s.npos){
						v.type(lyramilk::data::var::t_int);
					}else{
						v.type(lyramilk::data::var::t_double);
					}
					dq.push(std::pair<lyramilk::data::string,lyramilk::data::string>(k,s));
				}
				if(dq.size() == 0) return false;
				{
					lyramilk::data::string pageeof = ret.at(ret.size()-2);

					cmd2.push_back(key);
					cmd2.push_back(pageeof);

					lyramilk::data::var vrank;
					if(!predis->c->exec(cmd2,vrank)){
						return false;
					}
					cursor = vrank;
					++cursor;
				}

				if(zset_item_key.empty()){
					std::pair<lyramilk::data::string,lyramilk::data::var>& pr = dq.front();
					zset_item_key = pr.first;
					zset_item_score = pr.second;
					isok = true;
					dq.pop();
				}
				return true;
			}
			return false;
		}

		lyramilk::data::var ok(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			if(cursor == this->start){
				sync();
			}
			return isok;
		}

		lyramilk::data::var value(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			return zset_item_key;
		}

		lyramilk::data::var score(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			return zset_item_score;
		}


		lyramilk::data::var next(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			isok = false;
			if(dq.size() == 0 && !sync()) return false;

			std::pair<lyramilk::data::string,lyramilk::data::var>& pr = dq.front();
			zset_item_key = pr.first;
			zset_item_score = pr.second;
			isok = true;
			dq.pop();
			return true;
		}

		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["value"] = lyramilk::script::engine::functional<redis_zset_iterator,&redis_zset_iterator::value>;
			fn["score"] = lyramilk::script::engine::functional<redis_zset_iterator,&redis_zset_iterator::score>;
			fn["ok"] = lyramilk::script::engine::functional<redis_zset_iterator,&redis_zset_iterator::ok>;
			fn["next"] = lyramilk::script::engine::functional<redis_zset_iterator,&redis_zset_iterator::next>;
			p->define("redis.zset.iterator",fn,redis_zset_iterator::ctr,redis_zset_iterator::dtr);
			return 1;
		}
	};


	class redis_zset:public lyramilk::script::sclass
	{
		lyramilk::log::logss log;
		lyramilk::data::string key;
		redis* predis;
	  public:
		static lyramilk::script::sclass* ctr(const lyramilk::data::array& args)
		{
			assert(args.size() == 2);
			return new redis_zset((redis*)args[0].userdata("redis"),args[1]);
		}
		static void dtr(lyramilk::script::sclass* p)
		{
			delete (redis_zset*)p;
		}

		redis_zset(redis* pc,lyramilk::data::string key):log(lyramilk::klog,"teapoy.native.redis.sorted_set")
		{
			predis = pc;
			this->key = key;
		}

		~redis_zset()
		{
		}

		lyramilk::data::var getkey(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			return key;
		}

		lyramilk::data::var scan(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			lyramilk::data::uint64 startof = 0;
			lyramilk::data::uint64 unit = 20;
			if(args.size() > 0){
				const lyramilk::data::var& v = args.at(0);
				if(!v.type_like(lyramilk::data::var::t_uint)){
					throw lyramilk::exception(D("参数%d类型不兼容:%s，期待%s",1,v.type_name().c_str(),lyramilk::data::var::type_name(lyramilk::data::var::t_uint).c_str()));
				}
				startof = v;
			}
			if(args.size() > 1){
				const lyramilk::data::var& v = args.at(1);
				if(!v.type_like(lyramilk::data::var::t_uint)){
					throw lyramilk::exception(D("参数%d类型不兼容:%s，期待%s",2,v.type_name().c_str(),lyramilk::data::var::type_name(lyramilk::data::var::t_uint).c_str()));
				}
				unit = v;
			}
			lyramilk::script::engine* e = (lyramilk::script::engine*)env.find(lyramilk::script::engine::s_env_engine())->second.userdata(lyramilk::script::engine::s_env_engine());
			
			lyramilk::data::array ar;
			ar.reserve(5);
			ar.push_back(lyramilk::data::var("redis",predis));
			ar.push_back(key);
			ar.push_back(false);
			ar.push_back(startof);
			ar.push_back(unit);
			return e->createobject("redis.zset.iterator",ar);
		}

		lyramilk::data::var rscan(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			lyramilk::data::uint64 startof = 0;
			lyramilk::data::uint64 unit = 20;
			if(args.size() > 0){
				const lyramilk::data::var& v = args.at(0);
				if(!v.type_like(lyramilk::data::var::t_uint)){
					throw lyramilk::exception(D("参数%d类型不兼容:%s，期待%s",1,v.type_name().c_str(),lyramilk::data::var::type_name(lyramilk::data::var::t_uint).c_str()));
				}
				startof = v;
			}
			if(args.size() > 1){
				const lyramilk::data::var& v = args.at(1);
				if(!v.type_like(lyramilk::data::var::t_uint)){
					throw lyramilk::exception(D("参数%d类型不兼容:%s，期待%s",2,v.type_name().c_str(),lyramilk::data::var::type_name(lyramilk::data::var::t_uint).c_str()));
				}
				unit = v;
			}
			lyramilk::script::engine* e = (lyramilk::script::engine*)env.find(lyramilk::script::engine::s_env_engine())->second.userdata(lyramilk::script::engine::s_env_engine());
			lyramilk::data::array ar;
			ar.reserve(5);
			ar.push_back(lyramilk::data::var("redis",predis));
			ar.push_back(key);
			ar.push_back(true);
			ar.push_back(startof);
			ar.push_back(unit);
			return e->createobject("redis.zset.iterator",ar);
		}

		lyramilk::data::var add(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			if(predis->readonly()) throw lyramilk::exception(D("redis.zset.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_uint);
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_str);
			lyramilk::data::string str = args[1];
			if(predis->is_ssdb()){
				if(str.size() > 200){
					throw lyramilk::exception(D("redis.zset.%s：SSDB禁止向zset插入超长键%d:%s",__FUNCTION__,str.size(),str.c_str()));
				}
				if(args[0].type() == lyramilk::data::var::t_double){
					throw lyramilk::exception(D("redis.zset.%s：SSDB禁止使用浮点数作为zset的score，请检查参数1",__FUNCTION__));
				}
			}

			lyramilk::data::array cmd;
			cmd.reserve(4);
			cmd.push_back("zadd");
			cmd.push_back(key);
			cmd.push_back(args[0]);
			cmd.push_back(str);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		lyramilk::data::var remove(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			if(predis->readonly()) throw lyramilk::exception(D("redis.zset.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string str = args[0];
			lyramilk::data::array cmd;
			cmd.reserve(3);
			cmd.push_back("zrem");
			cmd.push_back(key);
			cmd.push_back(args[0]);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		lyramilk::data::var size(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			lyramilk::data::array cmd;
			cmd.reserve(2);
			cmd.push_back("zcard");
			cmd.push_back(key);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		lyramilk::data::var count(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			if(args.size() == 0){
				lyramilk::data::array cmd;
				cmd.reserve(4);
				cmd.push_back("zcount");
				cmd.push_back(key);
				cmd.push_back("-inf");
				cmd.push_back("+inf");
				lyramilk::data::var vret;
				if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
				return vret;
			}
			if(args.size() == 1){
				lyramilk::data::array cmd;
				cmd.reserve(4);
				cmd.push_back("zcount");
				cmd.push_back(key);
				cmd.push_back(args[0]);
				cmd.push_back("+inf");
				lyramilk::data::var vret;
				if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
				return vret;
			}
			lyramilk::data::array cmd;
			cmd.reserve(4);
			cmd.push_back("zcount");
			cmd.push_back(key);
			cmd.push_back(args[0]);
			cmd.push_back(args[1]);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		lyramilk::data::var clear(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			if(predis->readonly()) throw lyramilk::exception(D("redis.zset.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			if(predis->is_ssdb()){
				lyramilk::data::array cmd;
				cmd.reserve(2);
				cmd.push_back("zclear");
				cmd.push_back(key);
				lyramilk::data::var vret;
				if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
				return vret.size() > 0 && vret[0] > 0;
			}
			lyramilk::data::array cmd;
			cmd.reserve(2);
			cmd.push_back("del");
			cmd.push_back(key);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret > 0;
		}

		lyramilk::data::var score(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::array cmd;
			cmd.reserve(3);
			cmd.push_back("zscore");
			cmd.push_back(key);
			cmd.push_back(args[0]);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			if(vret.type() == lyramilk::data::var::t_str){
				lyramilk::data::string s = vret.str();
				if(s.find('.') == s.npos){
					vret.type(lyramilk::data::var::t_int);
				}else{
					vret.type(lyramilk::data::var::t_double);
				}
			}
			return vret;
		}

		lyramilk::data::var at(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_int);
			lyramilk::data::int64 i = args[0];
			lyramilk::data::array cmd;
			cmd.reserve(4);
			cmd.push_back("zrange");
			cmd.push_back(key);
			cmd.push_back(i);
			cmd.push_back(i);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			if(vret.size() > 0){
				return vret[0];
			}
			return lyramilk::data::var::nil;
		}

		lyramilk::data::var rank(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::array cmd;
			cmd.reserve(3);
			cmd.push_back("zrank");
			cmd.push_back(key);
			cmd.push_back(args[0]);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		lyramilk::data::var random(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_uint);
			lyramilk::data::uint64 n = args[0];
			lyramilk::data::uint64 max = size(args,env);

			srand(__rdtsc());

			std::map<lyramilk::data::uint64,bool> m;
			lyramilk::data::array ret;
			if(max == 0) return ret;
			ret.reserve(n);
			lyramilk::data::int64 hp = 10;
			lyramilk::script::engine* e = nullptr;
			if (args.size() > 1 && args[1].type() == lyramilk::data::var::t_user){
				e = (lyramilk::script::engine*)env.find(lyramilk::script::engine::s_env_engine())->second.userdata(lyramilk::script::engine::s_env_engine());
			}

			while(ret.size() < n && hp > 0){
				lyramilk::data::uint64 index = rand() % max;
				std::pair<std::map<lyramilk::data::uint64,bool>::iterator,bool> checkr = m.insert(std::pair<lyramilk::data::uint64,bool>(index,true));
				if(checkr.second){
					lyramilk::data::array ar;
					ar.push_back(index);
					lyramilk::data::var v = at(ar,env);
					if(v.type() == lyramilk::data::var::t_str){
						lyramilk::data::string str = v.str();

						if(e){
							//转换函数
							lyramilk::data::array ar;
							ar.push_back(str);
							lyramilk::data::var v = e->call(args[1],ar);
							lyramilk::data::var::vt t = v.type();
							if(t != lyramilk::data::var::t_user && t != lyramilk::data::var::t_invalid){
								ret.push_back(v);
								continue;
							}
						}else{
							ret.push_back(str);
							continue;
						}
					}
				}
				--hp;
			}
			return ret;
		}

		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["getkey"] = lyramilk::script::engine::functional<redis_zset,&redis_zset::getkey>;
			fn["scan"] = lyramilk::script::engine::functional<redis_zset,&redis_zset::scan>;
			fn["rscan"] = lyramilk::script::engine::functional<redis_zset,&redis_zset::rscan>;
			fn["add"] = lyramilk::script::engine::functional<redis_zset,&redis_zset::add>;
			fn["remove"] = lyramilk::script::engine::functional<redis_zset,&redis_zset::remove>;
			fn["size"] = lyramilk::script::engine::functional<redis_zset,&redis_zset::size>;
			fn["count"] = lyramilk::script::engine::functional<redis_zset,&redis_zset::count>;
			fn["clear"] = lyramilk::script::engine::functional<redis_zset,&redis_zset::clear>;
			fn["score"] = lyramilk::script::engine::functional<redis_zset,&redis_zset::score>;
			fn["at"] = lyramilk::script::engine::functional<redis_zset,&redis_zset::at>;
			fn["rank"] = lyramilk::script::engine::functional<redis_zset,&redis_zset::rank>;
			fn["random"] = lyramilk::script::engine::functional<redis_zset,&redis_zset::random>;
			p->define("redis.zset",fn,redis_zset::ctr,redis_zset::dtr);
			return 1;
		}
	};

	class redis_hmap_iterator:public lyramilk::script::sclass
	{
		lyramilk::log::logss log;
		redis* predis;
		lyramilk::data::string key;
		lyramilk::data::string sitem_value;
		lyramilk::data::array keys;

		bool isok;
		bool reverse;
		lyramilk::data::uint64 cursor;
		lyramilk::data::uint64 start;
	  public:
		static lyramilk::script::sclass* ctr(const lyramilk::data::array& args)
		{
			return new redis_hmap_iterator((redis*)args[0].userdata("redis"),args[1],args[2],args[3]);
		}
		static void dtr(lyramilk::script::sclass* p)
		{
			delete (redis_hmap_iterator*)p;
		}

		redis_hmap_iterator(redis* pc,lyramilk::data::string redis_key,bool reverse,lyramilk::data::uint64 start):log(lyramilk::klog,"teapoy.native.redis.hashmap.iterator")
		{
			predis = pc;
			this->key = redis_key;
			cursor = start;
			this->start = start;
			this->reverse = reverse;
			this->isok = false;
		}

		~redis_hmap_iterator()
		{
		}

		bool sync()
		{
			if(!isok){
				lyramilk::data::array cmd;
				cmd.push_back("hkeys");
				cmd.push_back(key);
				lyramilk::data::var vret;
				if(!predis->c->exec(cmd,vret)) return false;
				keys = vret;
				isok = true;
			}
			return true;
		}

		lyramilk::data::var ok(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			if(cursor == this->start && keys.empty()){
				sync();
			}
			if(!isok) return false;
			if(cursor < keys.size()){
				return true;
			}
			isok = false;
			return false;
		}

		lyramilk::data::var next(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			sitem_value.clear();
			if(cursor < keys.size()){
				++cursor;
			}
			if(cursor < keys.size()){
				return true;
			}
			isok = false;
			return false;
		}

		lyramilk::data::var item_key(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			return keys[reverse?(keys.size() - cursor - 1):cursor];
		}

		lyramilk::data::var item_value(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			if(sitem_value.empty()){
				lyramilk::data::array cmd;
				cmd.push_back("hget");
				cmd.push_back(key);
				cmd.push_back(keys[cursor]);
				lyramilk::data::var vret;
				if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
				sitem_value = vret.str();
			}
			return sitem_value;
		}

		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["ok"] = lyramilk::script::engine::functional<redis_hmap_iterator,&redis_hmap_iterator::ok>;
			fn["next"] = lyramilk::script::engine::functional<redis_hmap_iterator,&redis_hmap_iterator::next>;
			fn["key"] = lyramilk::script::engine::functional<redis_hmap_iterator,&redis_hmap_iterator::item_key>;
			fn["value"] = lyramilk::script::engine::functional<redis_hmap_iterator,&redis_hmap_iterator::item_value>;
			p->define("redis.hashmap.iterator",fn,redis_hmap_iterator::ctr,redis_hmap_iterator::dtr);
			return 1;
		}
	};

	class redis_hmap_hscan_iterator:public lyramilk::script::sclass
	{
		lyramilk::log::logss log;
		redis* predis;
		lyramilk::data::string key;
		lyramilk::data::uint64 skips;
		lyramilk::data::uint64 cursor;
		std::queue<std::pair<lyramilk::data::string,lyramilk::data::string> > smap;
		bool iscompleted;
	  public:
		static lyramilk::script::sclass* ctr(const lyramilk::data::array& args)
		{
			return new redis_hmap_hscan_iterator((redis*)args[0].userdata("redis"),args[1],args[2]);
		}
		static void dtr(lyramilk::script::sclass* p)
		{
			delete (redis_hmap_hscan_iterator*)p;
		}

		redis_hmap_hscan_iterator(redis* pc,lyramilk::data::string redis_key,lyramilk::data::uint64 start):log(lyramilk::klog,"teapoy.native.redis.hashmap.iterator")
		{
			predis = pc;
			key = redis_key;
			cursor = 0;
			iscompleted = false;
			skips = start;
		}

		~redis_hmap_hscan_iterator()
		{
		}

		bool sync()
		{
			do{
				if(iscompleted) return !smap.empty();
				if(smap.empty()){
					lyramilk::data::array cmd;
					cmd.push_back("hscan");
					cmd.push_back(key);
					cmd.push_back(cursor);
					lyramilk::data::var vret;
					if(!predis->c->exec(cmd,vret)) return false;
					if(vret.type() != lyramilk::data::var::t_array) return false;
					lyramilk::data::array& ar = vret;
					cursor = ar[0];
					if(cursor == 0){
						iscompleted = true;
					}
					lyramilk::data::array& data = ar[1];

					if(data.size() % 2 != 0) return false;

					for(std::size_t i=0;i<data.size();i+=2){
						if(skips == 0){
							smap.push(std::pair<lyramilk::data::string,lyramilk::data::string>(data[i].str(),data[i+1].str()));
						}else{
							--skips;
						}
					}
				}
				if(!smap.empty()) return true;
			}while(skips > 0 || (!iscompleted));
			return false;
		}

		lyramilk::data::var ok(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			return sync();
		}

		lyramilk::data::var next(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			if(sync()){
				smap.pop();
			}
			return sync();
		}

		lyramilk::data::var item_key(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			if(!sync()) return lyramilk::data::var::nil;
			return smap.front().first;
		}

		lyramilk::data::var item_value(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			if(!sync()) return lyramilk::data::var::nil;
			return smap.front().second;
		}

		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["ok"] = lyramilk::script::engine::functional<redis_hmap_hscan_iterator,&redis_hmap_hscan_iterator::ok>;
			fn["next"] = lyramilk::script::engine::functional<redis_hmap_hscan_iterator,&redis_hmap_hscan_iterator::next>;
			fn["key"] = lyramilk::script::engine::functional<redis_hmap_hscan_iterator,&redis_hmap_hscan_iterator::item_key>;
			fn["value"] = lyramilk::script::engine::functional<redis_hmap_hscan_iterator,&redis_hmap_hscan_iterator::item_value>;
			p->define("redis.hashmap.hscan_iterator",fn,redis_hmap_hscan_iterator::ctr,redis_hmap_hscan_iterator::dtr);
			return 1;
		}
	};

	class redis_hmap:public lyramilk::script::sclass
	{
		lyramilk::log::logss log;
		lyramilk::data::string key;
		redis* predis;
	  public:
		static lyramilk::script::sclass* ctr(const lyramilk::data::array& args)
		{
			return new redis_hmap((redis*)args[0].userdata("redis"),args[1]);
		}
		static void dtr(lyramilk::script::sclass* p)
		{
			delete (redis_hmap*)p;
		}

		redis_hmap(redis* pc,lyramilk::data::string redis_key):log(lyramilk::klog,"teapoy.native.redis.hashmap")
		{
			predis = pc;
			this->key = redis_key;
		}

		~redis_hmap()
		{
		}

		lyramilk::data::var getkey(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			return key;
		}

		lyramilk::data::var scan(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			lyramilk::data::uint64 startof = 0;
			if(args.size() > 0){
				const lyramilk::data::var& v = args.at(0);
				if(!v.type_like(lyramilk::data::var::t_uint)){
					throw lyramilk::exception(D("参数%d类型不兼容:%s，期待%s",1,v.type_name().c_str(),lyramilk::data::var::type_name(lyramilk::data::var::t_uint).c_str()));
				}
				startof = v;
			}
			lyramilk::script::engine* e = (lyramilk::script::engine*)env.find(lyramilk::script::engine::s_env_engine())->second.userdata(lyramilk::script::engine::s_env_engine());
			//if(startof == 0 && !predis->is_ssdb() && predis->c->version() >= "2.8.0"){
			if(startof == 0 && !predis->is_ssdb() && predis->c->version() >= "2.8.0" && predis->c->testcmd("hscan")){
				lyramilk::data::array ar;
				ar.reserve(4);
				ar.push_back(lyramilk::data::var("redis",predis));
				ar.push_back(key);
				ar.push_back(startof);
				return e->createobject("redis.hashmap.hscan_iterator",ar);
			}

			lyramilk::data::array ar;
			ar.reserve(4);
			ar.push_back(lyramilk::data::var("redis",predis));
			ar.push_back(key);
			ar.push_back(false);
			ar.push_back(startof);
			return e->createobject("redis.hashmap.iterator",ar);
		}

		lyramilk::data::var get(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::array cmd;
			cmd.reserve(3);
			cmd.push_back("hget");
			cmd.push_back(key);
			cmd.push_back(args[0]);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		lyramilk::data::var gets(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::array cmd;
			cmd.reserve(2 + args.size());
			cmd.push_back("hmget");
			cmd.push_back(key);
			for(lyramilk::data::uint64 i=0;i<args.size();++i){
				cmd.push_back(args[i]);
			}
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		lyramilk::data::var set(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			if(predis->readonly()) throw lyramilk::exception(D("redis.hashmap.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_str);
			lyramilk::data::array cmd;
			cmd.reserve(4);
			cmd.push_back("hset");
			cmd.push_back(key);
			cmd.push_back(args[0]);
			cmd.push_back(args[1]);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		lyramilk::data::var remove(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			if(predis->readonly()) throw lyramilk::exception(D("redis.hashmap.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::array cmd;
			cmd.reserve(3);
			cmd.push_back("hdel");
			cmd.push_back(key);
			cmd.push_back(args[0]);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret == 1;
		}

		lyramilk::data::var size(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			lyramilk::data::array cmd;
			cmd.reserve(2);
			cmd.push_back("hlen");
			cmd.push_back(key);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		lyramilk::data::var exist(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::array cmd;
			cmd.reserve(3);
			cmd.push_back("hexists");
			cmd.push_back(key);
			cmd.push_back(args[0]);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		lyramilk::data::var incr(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_int);
			lyramilk::data::array cmd;
			cmd.reserve(4);
			cmd.push_back("hincrby");
			cmd.push_back(key);
			cmd.push_back(args[0]);
			cmd.push_back(args[1]);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		lyramilk::data::var decr(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_int);
			lyramilk::data::int64 v = args[1];
			lyramilk::data::array cmd;
			cmd.reserve(4);
			cmd.push_back("hincrby");
			cmd.push_back(key);
			cmd.push_back(args[0]);
			cmd.push_back(0 - v);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		lyramilk::data::var clear(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			if(predis->readonly()) throw lyramilk::exception(D("redis.hashmap.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			if(predis->is_ssdb()){
				lyramilk::data::array cmd;
				cmd.reserve(2);
				cmd.push_back("hclear");
				cmd.push_back(key);
				lyramilk::data::var vret;
				if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
				return vret.size() > 0 && vret[0] > 0;
			}
			lyramilk::data::array cmd;
			cmd.reserve(2);
			cmd.push_back("del");
			cmd.push_back(key);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret > 0;
		}

		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["getkey"] = lyramilk::script::engine::functional<redis_hmap,&redis_hmap::getkey>;
			fn["scan"] = lyramilk::script::engine::functional<redis_hmap,&redis_hmap::scan>;
			fn["gets"] = lyramilk::script::engine::functional<redis_hmap,&redis_hmap::gets>;
			fn["get"] = lyramilk::script::engine::functional<redis_hmap,&redis_hmap::get>;
			fn["set"] = lyramilk::script::engine::functional<redis_hmap,&redis_hmap::set>;
			fn["remove"] = lyramilk::script::engine::functional<redis_hmap,&redis_hmap::remove>;
			fn["size"] = lyramilk::script::engine::functional<redis_hmap,&redis_hmap::size>;
			fn["exist"] = lyramilk::script::engine::functional<redis_hmap,&redis_hmap::exist>;
			fn["incr"] = lyramilk::script::engine::functional<redis_hmap,&redis_hmap::incr>;
			fn["decr"] = lyramilk::script::engine::functional<redis_hmap,&redis_hmap::decr>;
			fn["clear"] = lyramilk::script::engine::functional<redis_hmap,&redis_hmap::clear>;
			p->define("redis.hashmap",fn,redis_hmap::ctr,redis_hmap::dtr);
			return 1;
		}
	};

	///////////////////////////////

	class redis_list_iterator:public lyramilk::script::sclass
	{
		lyramilk::log::logss log;
		lyramilk::data::string key;
		redis* predis;

		bool isok;
		bool reverse;
		std::deque<lyramilk::data::string> dq;
		lyramilk::data::int64 cursor;
		lyramilk::data::int64 start;
		lyramilk::data::string list_item_value;
	  public:
		static lyramilk::script::sclass* ctr(const lyramilk::data::array& args)
		{
			return new redis_list_iterator((redis*)args[0].userdata("redis"),args[1],args[2],args[3]);
		}
		static void dtr(lyramilk::script::sclass* p)
		{
			delete (redis_list_iterator*)p;
		}

		redis_list_iterator(redis* pc,lyramilk::data::string redis_key,bool reverse,lyramilk::data::uint64 start):log(lyramilk::klog,"teapoy.native.redis.list.iterator")
		{
			predis = pc;
			this->key = redis_key;
			this->reverse = reverse;
			cursor = start;
			this->start = start;
			isok = false;
		}

		~redis_list_iterator()
		{
		}

		bool sync()
		{
			if(!dq.empty()) return true;
			lyramilk::data::array cmd;
			cmd.reserve(4);
			if(this->reverse){
				cmd.push_back("lrange");
				cmd.push_back(key);
				cmd.push_back(0 - (cursor + 20));
				cmd.push_back(0 - cursor - 1);
			}else{
				cmd.push_back("lrange");
				cmd.push_back(key);
				cmd.push_back(cursor);
				cmd.push_back(cursor + 20 - 1);
			}
			lyramilk::data::var vret;
			if(predis->c->exec(cmd,vret)){
				lyramilk::data::array& ret = vret;
				for(std::size_t i=0;i<ret.size();++i){
					lyramilk::data::string v = ret[i];
					dq.push_back(v);
				}
				if(dq.empty()) return false;
				if(cursor == start){
					isok = true;
					if(this->reverse){
						list_item_value = dq.back();
						dq.pop_back();
					}else{
						list_item_value = dq.front();
						dq.pop_front();
					}
					cursor += dq.size() + 1;
				}else{
					cursor += dq.size();
				}
				return true;
			}
			return false;
		}

		lyramilk::data::var ok(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			if(cursor == start){
				sync();
			}
			return isok;

		}

		lyramilk::data::var value(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			return list_item_value;
		}

		lyramilk::data::var next(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			isok = false;
			if(dq.size() == 0 && !sync()) return false;
			if(this->reverse){
				list_item_value = dq.back();
				dq.pop_back();
			}else{
				list_item_value = dq.front();
				dq.pop_front();
			}
			isok = true;
			return true;
		}

		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["ok"] = lyramilk::script::engine::functional<redis_list_iterator,&redis_list_iterator::ok>;
			fn["value"] = lyramilk::script::engine::functional<redis_list_iterator,&redis_list_iterator::value>;
			fn["next"] = lyramilk::script::engine::functional<redis_list_iterator,&redis_list_iterator::next>;
			p->define("redis.list.iterator",fn,redis_list_iterator::ctr,redis_list_iterator::dtr);
			return 1;
		}
	};

	class redis_list:public lyramilk::script::sclass
	{
		lyramilk::log::logss log;
		lyramilk::data::string key;
		redis* predis;
	  public:
		static lyramilk::script::sclass* ctr(const lyramilk::data::array& args)
		{
			return new redis_list((redis*)args[0].userdata("redis"),args[1]);
		}
		static void dtr(lyramilk::script::sclass* p)
		{
			delete (redis_list*)p;
		}

		redis_list(redis* pc,lyramilk::data::string redis_key):log(lyramilk::klog,"teapoy.native.redis.list")
		{
			predis = pc;
			this->key = redis_key;
		}

		~redis_list()
		{
		}

		lyramilk::data::var getkey(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			return key;
		}

		lyramilk::data::var push_back(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			if(predis->readonly()) throw lyramilk::exception(D("redis.list.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::array cmd;
			cmd.reserve(3);
			cmd.push_back("rpush");
			cmd.push_back(key);
			cmd.push_back(args[0]);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		lyramilk::data::var push_front(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			if(predis->readonly()) throw lyramilk::exception(D("redis.list.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::array cmd;
			cmd.reserve(3);
			cmd.push_back("lpush");
			cmd.push_back(key);
			cmd.push_back(args[0]);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		lyramilk::data::var pop_back(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			if(predis->readonly()) throw lyramilk::exception(D("redis.list.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			lyramilk::data::array cmd;
			cmd.reserve(2);
			cmd.push_back("rpop");
			cmd.push_back(key);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		lyramilk::data::var pop_front(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			if(predis->readonly()) throw lyramilk::exception(D("redis.list.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			lyramilk::data::array cmd;
			cmd.reserve(2);
			cmd.push_back("lpop");
			cmd.push_back(key);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		lyramilk::data::var front(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			lyramilk::data::array cmd;
			cmd.reserve(3);
			cmd.push_back("lindex");
			cmd.push_back(key);
			cmd.push_back(0);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		lyramilk::data::var back(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			lyramilk::data::array cmd;
			cmd.reserve(3);
			cmd.push_back("lindex");
			cmd.push_back(key);
			cmd.push_back(-1);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		lyramilk::data::var at(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_int);
			lyramilk::data::array cmd;
			cmd.reserve(3);
			cmd.push_back("lindex");
			cmd.push_back(key);
			cmd.push_back(args[0]);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		lyramilk::data::var size(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			lyramilk::data::array cmd;
			cmd.reserve(2);
			cmd.push_back("llen");
			cmd.push_back(key);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		lyramilk::data::var scan(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			lyramilk::data::uint64 startof = 0;
			if(args.size() > 0){
				const lyramilk::data::var& v = args.at(0);
				if(!v.type_like(lyramilk::data::var::t_uint)){
					throw lyramilk::exception(D("参数%d类型不兼容:%s，期待%s",1,v.type_name().c_str(),lyramilk::data::var::type_name(lyramilk::data::var::t_uint).c_str()));
				}
				startof = v;
			}
			lyramilk::script::engine* e = (lyramilk::script::engine*)env.find(lyramilk::script::engine::s_env_engine())->second.userdata(lyramilk::script::engine::s_env_engine());
			
			lyramilk::data::array ar;
			ar.reserve(4);
			ar.push_back(lyramilk::data::var("redis",predis));
			ar.push_back(key);
			ar.push_back(false);
			ar.push_back(startof);
			return e->createobject("redis.list.iterator",ar);
		}

		lyramilk::data::var rscan(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			lyramilk::data::uint64 startof = 0;
			if(args.size() > 0){
				const lyramilk::data::var& v = args.at(0);
				if(!v.type_like(lyramilk::data::var::t_uint)){
					throw lyramilk::exception(D("参数%d类型不兼容:%s，期待%s",1,v.type_name().c_str(),lyramilk::data::var::type_name(lyramilk::data::var::t_uint).c_str()));
				}
				startof = v;
			}
			lyramilk::script::engine* e = (lyramilk::script::engine*)env.find(lyramilk::script::engine::s_env_engine())->second.userdata(lyramilk::script::engine::s_env_engine());
			
			lyramilk::data::array ar;
			ar.reserve(4);
			ar.push_back(lyramilk::data::var("redis",predis));
			ar.push_back(key);
			ar.push_back(true);
			ar.push_back(startof);
			return e->createobject("redis.list.iterator",ar);
		}

		lyramilk::data::var clear(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			if(predis->readonly()) throw lyramilk::exception(D("redis.list.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			if(predis->is_ssdb()){
				lyramilk::data::array cmd;
				cmd.reserve(2);
				cmd.push_back("qclear");
				cmd.push_back(key);
				lyramilk::data::var vret;
				if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
				return vret.size() > 0 && vret[0] > 0;
			}

			lyramilk::data::array cmd;
			cmd.reserve(2);
			cmd.push_back("del");
			cmd.push_back(key);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret > 0;
		}

		lyramilk::data::var random(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_uint);
			lyramilk::data::uint64 n = args[0];
			lyramilk::data::uint64 max = size(args,env);

			srand(__rdtsc());

			std::map<lyramilk::data::uint64,bool> m;
			lyramilk::data::array ret;
			if(max == 0) return ret;
			ret.reserve(n);
			lyramilk::data::int64 hp = 10;
			lyramilk::script::engine* e = nullptr;
			if (args.size() > 1 && args[1].type() == lyramilk::data::var::t_user){
				e = (lyramilk::script::engine*)env.find(lyramilk::script::engine::s_env_engine())->second.userdata(lyramilk::script::engine::s_env_engine());
			}

			while(ret.size() < n && hp > 0){
				lyramilk::data::uint64 index = rand() % max;
				std::pair<std::map<lyramilk::data::uint64,bool>::iterator,bool> checkr = m.insert(std::pair<lyramilk::data::uint64,bool>(index,true));
				if(checkr.second){
					lyramilk::data::array ar;
					ar.push_back(index);
					lyramilk::data::var v = at(ar,env);
					if(v.type() == lyramilk::data::var::t_str){
						lyramilk::data::string str = v.str();

						if(e){
							//转换函数
							lyramilk::data::array ar;
							ar.push_back(str);
							lyramilk::data::var v = e->call(args[1],ar);
							lyramilk::data::var::vt t = v.type();
							if(t != lyramilk::data::var::t_user && t != lyramilk::data::var::t_invalid){
								ret.push_back(v);
								continue;
							}
						}else{
							ret.push_back(str);
							continue;
						}
					}
				}
				--hp;
			}
			return ret;
		}

		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["getkey"] = lyramilk::script::engine::functional<redis_list,&redis_list::getkey>;
			fn["push_back"] = lyramilk::script::engine::functional<redis_list,&redis_list::push_back>;
			fn["push_front"] = lyramilk::script::engine::functional<redis_list,&redis_list::push_front>;
			fn["pop_back"] = lyramilk::script::engine::functional<redis_list,&redis_list::pop_back>;
			fn["pop_front"] = lyramilk::script::engine::functional<redis_list,&redis_list::pop_front>;
			fn["front"] = lyramilk::script::engine::functional<redis_list,&redis_list::front>;
			fn["back"] = lyramilk::script::engine::functional<redis_list,&redis_list::back>;
			//fn["get"] = lyramilk::script::engine::functional<redis_list,&redis_list::get>;
			fn["at"] = lyramilk::script::engine::functional<redis_list,&redis_list::at>;
			fn["size"] = lyramilk::script::engine::functional<redis_list,&redis_list::size>;
			fn["scan"] = lyramilk::script::engine::functional<redis_list,&redis_list::scan>;
			fn["rscan"] = lyramilk::script::engine::functional<redis_list,&redis_list::rscan>;
			fn["clear"] = lyramilk::script::engine::functional<redis_list,&redis_list::clear>;
			fn["random"] = lyramilk::script::engine::functional<redis_list,&redis_list::random>;
			p->define("redis.list",fn,redis_list::ctr,redis_list::dtr);
			return 1;
		}
	};

	class redis_kv:public lyramilk::script::sclass
	{
		lyramilk::log::logss log;
		lyramilk::data::string key;
		redis* predis;
	  public:
		static lyramilk::script::sclass* ctr(const lyramilk::data::array& args)
		{
			return new redis_kv((redis*)args[0].userdata("redis"),args[1]);
		}
		static void dtr(lyramilk::script::sclass* p)
		{
			delete (redis_kv*)p;
		}

		redis_kv(redis* pc,lyramilk::data::string redis_key):log(lyramilk::klog,"teapoy.native.redis.kv")
		{
			predis = pc;
			this->key = redis_key;
		}

		~redis_kv()
		{
		}

		lyramilk::data::var getkey(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			return key;
		}

		lyramilk::data::var get(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			lyramilk::data::array cmd;
			cmd.reserve(2);
			cmd.push_back("get");
			cmd.push_back(key);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		lyramilk::data::var set(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			if(predis->readonly()) throw lyramilk::exception(D("redis.kv.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::array cmd;
			cmd.reserve(3);
			cmd.push_back("set");
			cmd.push_back(key);
			cmd.push_back(args[0]);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		lyramilk::data::var incr(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			if(predis->readonly()) throw lyramilk::exception(D("redis.kv.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			lyramilk::data::array cmd;
			cmd.reserve(2);
			cmd.push_back("incr");
			cmd.push_back(key);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		lyramilk::data::var decr(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			if(predis->readonly()) throw lyramilk::exception(D("redis.kv.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			lyramilk::data::array cmd;
			cmd.reserve(2);
			cmd.push_back("decr");
			cmd.push_back(key);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		lyramilk::data::var clear(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			if(predis->readonly()) throw lyramilk::exception(D("redis.kv.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			lyramilk::data::array cmd;
			cmd.reserve(2);
			cmd.push_back("del");
			cmd.push_back(key);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret > 0;
		}

		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["getkey"] = lyramilk::script::engine::functional<redis_kv,&redis_kv::getkey>;
			fn["get"] = lyramilk::script::engine::functional<redis_kv,&redis_kv::get>;
			fn["set"] = lyramilk::script::engine::functional<redis_kv,&redis_kv::set>;
			fn["incr"] = lyramilk::script::engine::functional<redis_kv,&redis_kv::incr>;
			fn["decr"] = lyramilk::script::engine::functional<redis_kv,&redis_kv::decr>;
			fn["clear"] = lyramilk::script::engine::functional<redis_kv,&redis_kv::clear>;
			p->define("redis.kv",fn,redis_kv::ctr,redis_kv::dtr);
			return 1;
		}
	};


	static int define(lyramilk::script::engine* p)
	{
		int i = 0;
		i+= redis::define(p);
		i+= redis_zset::define(p);
		i+= redis_zset_iterator::define(p);
		i+= redis_hmap::define(p);
		i+= redis_hmap_iterator::define(p);
		i+= redis_hmap_hscan_iterator::define(p);
		i+= redis_list::define(p);
		i+= redis_list_iterator::define(p);
		i+= redis_kv::define(p);
		return i;
	}

	static __attribute__ ((constructor)) void __init()
	{
		lyramilk::teapoy::script2native::instance()->regist("redis",define);
	}
}}}