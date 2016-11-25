#include "script.h"
#include "env.h"
#include "stringutil.h"
#include <libmilk/var.h>
#include <iostream>
#include <libmilk/log.h>
#include <libmilk/multilanguage.h>
#include <libmilk/factory.hpp>
#include "redisX.h"
#include <queue>
#include <sys/socket.h>
#include <stdlib.h>

namespace lyramilk{ namespace teapoy{ namespace native {
	using namespace lyramilk::teapoy::redisX;

	class redis_clients:public lyramilk::threading::exclusive::list<lyramilk::teapoy::redisX::redis_client>
	{
		lyramilk::data::string host;
		lyramilk::data::uint16 port;
		lyramilk::data::string pwd;
	  public:
		redis_clients(lyramilk::data::string host,lyramilk::data::uint16 port,lyramilk::data::string pwd)
		{
			this->host = host;
			this->port = port;
			this->pwd = pwd;
		}

		virtual ~redis_clients()
		{
		}

		virtual lyramilk::teapoy::redisX::redis_client* underflow()
		{
			lyramilk::teapoy::redisX::redis_client* p = new lyramilk::teapoy::redisX::redis_client();
			if(!p) return nullptr;
			p->open(host.c_str(),port);
			p->auth(pwd);
			return p;
		}
	};

	class redis_clients_multiton:public lyramilk::util::multiton_factory<redis_clients>
	{
		lyramilk::threading::mutex_spin lock;
	  public:
		static redis_clients_multiton* instance()
		{
			static redis_clients_multiton _mm;
			return &_mm;
		}

		static lyramilk::data::string makekey(lyramilk::data::string host,lyramilk::data::uint16 port)
		{
			host.append((const char*)&port,sizeof(port));
			return host;
		}

		virtual redis_clients::ptr getobj(lyramilk::data::string host,lyramilk::data::uint16 port,lyramilk::data::string pwd)
		{
			lyramilk::data::string token = makekey(host,port);
			redis_clients* c = get(token);
			if(c == nullptr){
				lyramilk::threading::mutex_sync _(lock);
				define(token,new redis_clients(host,port,pwd));
				c = get(token);
			}

			if(c == nullptr) return nullptr;
			return c->get();
		}
	};


	class redis
	{
	  public:
		lyramilk::log::logss log;
		redis_clients::ptr c;
		bool isreadonly;
		static void* ctr(lyramilk::data::var::array args)
		{
			redis_clients::ptr c;
			if(args.size() == 2){
				c = redis_clients_multiton::instance()->getobj(args[0],args[1],"");
			}else if(args.size() == 3){
				c = redis_clients_multiton::instance()->getobj(args[0],args[1],args[2]);
			}else if(args.size() == 1){
				lyramilk::data::var& v = args[0];
				lyramilk::data::string host = v["host"];
				lyramilk::data::uint16 port = v["port"];
				lyramilk::data::string pwd = v["password"];
				c = redis_clients_multiton::instance()->getobj(host,port,pwd);
			}
			if(c){
				return new redis(c);
			}
			return nullptr;
		}
		static void dtr(void* p)
		{
			delete (redis*)p;
		}

		redis(redis_clients::ptr p):log(lyramilk::klog,"teapoy.native.redis")
		{
			c = p;
			isreadonly = false;
		}

		~redis()
		{
		}

		bool is_ssdb()
		{
			return false;
		}

		bool is_readonly()
		{
			return isreadonly;
		}

		lyramilk::data::var exec(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			if(is_readonly()) throw lyramilk::exception(D("redis.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_array);
			lyramilk::data::var vret;
			if(!c->exec(args[0],vret)) return lyramilk::data::var::nil;
			return vret;
		}

		lyramilk::data::var clear(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			if(is_readonly()) throw lyramilk::exception(D("redis.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			lyramilk::data::var::array cmd;
			cmd.push_back("flushdb");
			lyramilk::data::var vret;
			if(!c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		lyramilk::data::var readonly(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			if(args.size() > 0){
				MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_bool);
				isreadonly = args[0];
			}
			return isreadonly;
		}

		lyramilk::data::var newid(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			if(is_readonly()) throw lyramilk::exception(D("redis.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			if(args.size() == 0){
				lyramilk::data::var::array cmd;
				cmd.reserve(2);
				cmd.push_back("incr");
				cmd.push_back("teapoy:auto_increment::");
				lyramilk::data::var vret;
				if(!c->exec(cmd,vret)) return lyramilk::data::var::nil;
				return vret;
			}
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string skey = args[0];
			lyramilk::data::var::array cmd;
			cmd.reserve(2);
			cmd.push_back("incr");
			cmd.push_back("teapoy:auto_increment:" + skey + ":");
			lyramilk::data::var vret;
			if(!c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		lyramilk::data::var ok(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			TODO();
		}

		lyramilk::data::var kv(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string key = args[0];
			lyramilk::script::engine* e = (lyramilk::script::engine*)env.find(lyramilk::script::engine::s_env_engine())->second.userdata(lyramilk::script::engine::s_user_engineptr());
			
			lyramilk::data::var::array ar;
			ar.reserve(2);
			ar.push_back(lyramilk::data::var("redis",this));
			ar.push_back(key);
			return e->createobject("redis.kv",ar);
		}

		lyramilk::data::var hashmap(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string key = args[0];
			lyramilk::script::engine* e = (lyramilk::script::engine*)env.find(lyramilk::script::engine::s_env_engine())->second.userdata(lyramilk::script::engine::s_user_engineptr());
			
			lyramilk::data::var::array ar;
			ar.reserve(2);
			ar.push_back(lyramilk::data::var("redis",this));
			ar.push_back(key);
			return e->createobject("redis.hashmap",ar);
		}

		lyramilk::data::var zset(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string key = args[0];
			lyramilk::script::engine* e = (lyramilk::script::engine*)env.find(lyramilk::script::engine::s_env_engine())->second.userdata(lyramilk::script::engine::s_user_engineptr());
			
			lyramilk::data::var::array ar;
			ar.reserve(2);
			ar.push_back(lyramilk::data::var("redis",this));
			ar.push_back(key);
			return e->createobject("redis.zset",ar);
		}

		lyramilk::data::var list(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string key = args[0];
			lyramilk::script::engine* e = (lyramilk::script::engine*)env.find(lyramilk::script::engine::s_env_engine())->second.userdata(lyramilk::script::engine::s_user_engineptr());
			
			lyramilk::data::var::array ar;
			ar.reserve(2);
			ar.push_back(lyramilk::data::var("redis",this));
			ar.push_back(key);
			return e->createobject("redis.list",ar);
		}

		static int define(bool permanent,lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["readonly"] = lyramilk::script::engine::functional<redis,&redis::readonly>;
			fn["exec"] = lyramilk::script::engine::functional<redis,&redis::exec>;
			fn["clear"] = lyramilk::script::engine::functional<redis,&redis::clear>;
			fn["newid"] = lyramilk::script::engine::functional<redis,&redis::newid>;
			fn["ok"] = lyramilk::script::engine::functional<redis,&redis::ok>;
			fn["kv"] = lyramilk::script::engine::functional<redis,&redis::kv>;
			fn["hashmap"] = lyramilk::script::engine::functional<redis,&redis::hashmap>;
			fn["zset"] = lyramilk::script::engine::functional<redis,&redis::zset>;
			fn["list"] = lyramilk::script::engine::functional<redis,&redis::list>;
			p->define(permanent,"Redis",fn,redis::ctr,redis::dtr);
			return 1;
		}
	};

	class redis_zset_iterator
	{
		lyramilk::log::logss log;
		redis* predis;

		lyramilk::data::uint64 start;
		lyramilk::data::uint64 cursor;

		lyramilk::data::string key;
		lyramilk::data::string zset_item_key;
		lyramilk::data::var zset_item_score;
		bool isok;
		bool reverse;
		std::queue<std::pair<lyramilk::data::string,lyramilk::data::var> > dq;
	  public:
		static void* ctr(lyramilk::data::var::array args)
		{
			return new redis_zset_iterator((redis*)args[0].userdata("redis"),args[1],args[2],args[3]);
		}
		static void dtr(void* p)
		{
			delete (redis_zset_iterator*)p;
		}

		redis_zset_iterator(redis* pc,lyramilk::data::string redis_key,bool reverse,lyramilk::data::uint64 start):log(lyramilk::klog,"teapoy.native.redis.sorted_set.iterator")
		{
			this->reverse = reverse;
			predis = pc;
			this->key = redis_key;
			isok = false;
			this->start = start;
			this->cursor = start;
		}

		virtual ~redis_zset_iterator()
		{
		}

		bool sync()
		{
			if(!dq.empty()) return true;
			lyramilk::data::var::array cmd;
			lyramilk::data::var::array cmd2;
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
			cmd.push_back("20");
			lyramilk::data::var vret;
			if(predis->c->exec(cmd,vret)){
				lyramilk::data::var::array& ret = vret;
				for(std::size_t i=0;i<ret.size();i+=2){
					lyramilk::data::string k = ret[i];
					lyramilk::data::string s = ret[i+1];
					lyramilk::data::var v = s;
					if(s.find('.') == s.npos){
						v.type(lyramilk::data::var::t_int64);
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

		lyramilk::data::var ok(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			if(cursor == this->start){
				sync();
			}
			return isok;
		}

		lyramilk::data::var value(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			return zset_item_key;
		}

		lyramilk::data::var score(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			return zset_item_score;
		}


		lyramilk::data::var next(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
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

		static int define(bool permanent,lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["value"] = lyramilk::script::engine::functional<redis_zset_iterator,&redis_zset_iterator::value>;
			fn["score"] = lyramilk::script::engine::functional<redis_zset_iterator,&redis_zset_iterator::score>;
			fn["ok"] = lyramilk::script::engine::functional<redis_zset_iterator,&redis_zset_iterator::ok>;
			fn["next"] = lyramilk::script::engine::functional<redis_zset_iterator,&redis_zset_iterator::next>;
			p->define(permanent,"redis.zset.iterator",fn,redis_zset_iterator::ctr,redis_zset_iterator::dtr);
			return 1;
		}
	};


	class redis_zset
	{
		lyramilk::log::logss log;
		lyramilk::data::string key;
		redis* predis;
	  public:
		static void* ctr(lyramilk::data::var::array args)
		{
			assert(args.size() == 2);
			return new redis_zset((redis*)args[0].userdata("redis"),args[1]);
		}
		static void dtr(void* p)
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

		lyramilk::data::var getkey(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			return key;
		}

		lyramilk::data::var scan(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			lyramilk::data::uint64 startof = 0;
			if(args.size() > 0){
				const lyramilk::data::var& v = args.at(0);
				if(!v.type_like(lyramilk::data::var::t_uint64)){
					throw lyramilk::exception(D("参数%d类型不兼容:%s，期待%s",1,v.type_name().c_str(),lyramilk::data::var::type_name(lyramilk::data::var::t_uint64).c_str()));
				}
				startof = v;
			}
			lyramilk::script::engine* e = (lyramilk::script::engine*)env.find(lyramilk::script::engine::s_env_engine())->second.userdata(lyramilk::script::engine::s_user_engineptr());
			
			lyramilk::data::var::array ar;
			ar.reserve(4);
			ar.push_back(lyramilk::data::var("redis",predis));
			ar.push_back(key);
			ar.push_back(false);
			ar.push_back(startof);
			return e->createobject("redis.zset.iterator",ar);
		}

		lyramilk::data::var rscan(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			lyramilk::data::uint64 startof = 0;
			if(args.size() > 0){
				const lyramilk::data::var& v = args.at(0);
				if(!v.type_like(lyramilk::data::var::t_uint64)){
					throw lyramilk::exception(D("参数%d类型不兼容:%s，期待%s",1,v.type_name().c_str(),lyramilk::data::var::type_name(lyramilk::data::var::t_uint64).c_str()));
				}
				startof = v;
			}
			lyramilk::script::engine* e = (lyramilk::script::engine*)env.find(lyramilk::script::engine::s_env_engine())->second.userdata(lyramilk::script::engine::s_user_engineptr());
			
			lyramilk::data::var::array ar;
			ar.reserve(4);
			ar.push_back(lyramilk::data::var("redis",predis));
			ar.push_back(key);
			ar.push_back(true);
			ar.push_back(startof);
			return e->createobject("redis.zset.iterator",ar);
		}

		lyramilk::data::var add(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			if(predis->is_readonly()) throw lyramilk::exception(D("redis.zset.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_uint64);
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_str);
			lyramilk::data::string str = args[1];

			if(predis->is_ssdb() && str.size() > 200){
				throw lyramilk::exception(D("redis.zset.%s：SSDB禁止向zset插入超长键%d:%s",str.size(),str.c_str()));
			}

			lyramilk::data::var::array cmd;
			cmd.reserve(4);
			cmd.push_back("zadd");
			cmd.push_back(key);
			cmd.push_back(args[0]);
			cmd.push_back(str);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		lyramilk::data::var remove(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			if(predis->is_readonly()) throw lyramilk::exception(D("redis.zset.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string str = args[0];
			lyramilk::data::var::array cmd;
			cmd.reserve(3);
			cmd.push_back("zrem");
			cmd.push_back(key);
			cmd.push_back(args[0]);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		lyramilk::data::var size(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			lyramilk::data::var::array cmd;
			cmd.reserve(2);
			cmd.push_back("zcard");
			cmd.push_back(key);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		lyramilk::data::var count(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			if(args.size() == 0){
				lyramilk::data::var::array cmd;
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
				lyramilk::data::var::array cmd;
				cmd.reserve(4);
				cmd.push_back("zcount");
				cmd.push_back(key);
				cmd.push_back(args[0]);
				cmd.push_back("+inf");
				lyramilk::data::var vret;
				if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
				return vret;
			}
			lyramilk::data::var::array cmd;
			cmd.reserve(4);
			cmd.push_back("zcount");
			cmd.push_back(key);
			cmd.push_back(args[0]);
			cmd.push_back(args[1]);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		lyramilk::data::var clear(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			if(predis->is_readonly()) throw lyramilk::exception(D("redis.zset.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			if(predis->is_ssdb()){
				lyramilk::data::var::array cmd;
				cmd.reserve(2);
				cmd.push_back("zclear");
				cmd.push_back(key);
				lyramilk::data::var vret;
				if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
				TODO();
				return vret == 1;
			}
			lyramilk::data::var::array cmd;
			cmd.reserve(2);
			cmd.push_back("del");
			cmd.push_back(key);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret == 1;
		}

		lyramilk::data::var score(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::var::array cmd;
			cmd.reserve(3);
			cmd.push_back("zscore");
			cmd.push_back(key);
			cmd.push_back(args[0]);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			if(vret.type() == lyramilk::data::var::t_str){
				lyramilk::data::string s = vret.str();
				if(s.find('.') == s.npos){
					vret.type(lyramilk::data::var::t_int64);
				}else{
					vret.type(lyramilk::data::var::t_double);
				}
			}
			return vret;
		}

		lyramilk::data::var at(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_int64);
			lyramilk::data::int64 i = args[0];
			lyramilk::data::var::array cmd;
			cmd.reserve(4);
			cmd.push_back("zrange");
			cmd.push_back(key);
			cmd.push_back(i);
			cmd.push_back(i);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret[0];
		}

		lyramilk::data::var random(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_uint64);
			lyramilk::data::uint64 n = args[0];
			lyramilk::data::uint64 max = size(args,env);

			srand(time(0));

			std::map<lyramilk::data::uint64,bool> m;
			lyramilk::data::var::array ret;
			if(max == 0) return ret;
			ret.reserve(n);
			lyramilk::data::int64 hp = 10;
			lyramilk::script::engine* e = nullptr;
			if (args.size() > 1 && args[1].type() == lyramilk::data::var::t_user){
				e = (lyramilk::script::engine*)env.find(lyramilk::script::engine::s_env_engine())->second.userdata(lyramilk::script::engine::s_user_engineptr());
			}

			while(ret.size() < n && hp > 0){
				lyramilk::data::uint64 index = rand() % max;
				std::pair<std::map<lyramilk::data::uint64,bool>::iterator,bool> checkr = m.insert(std::pair<lyramilk::data::uint64,bool>(index,true));
				if(checkr.second){
					lyramilk::data::var::array ar;
					ar.push_back(index);
					lyramilk::data::var v = at(ar,env);
					if(v.type() == lyramilk::data::var::t_str){
						lyramilk::data::string str = v.str();

						if(e){
							//转换函数
							lyramilk::data::var::array ar;
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

		static int define(bool permanent,lyramilk::script::engine* p)
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
			fn["random"] = lyramilk::script::engine::functional<redis_zset,&redis_zset::random>;
			p->define(permanent,"redis.zset",fn,redis_zset::ctr,redis_zset::dtr);
			return 1;
		}
	};

	class redis_hmap_iterator
	{
		lyramilk::log::logss log;
		redis* predis;
		lyramilk::data::string key;
		lyramilk::data::string sitem_value;
		lyramilk::data::var::array keys;

		bool isok;
		bool reverse;
		lyramilk::data::uint64 cursor;
		lyramilk::data::uint64 start;
	  public:
		static void* ctr(lyramilk::data::var::array args)
		{
			return new redis_hmap_iterator((redis*)args[0].userdata("redis"),args[1],args[2],args[3]);
		}
		static void dtr(void* p)
		{
			delete (redis_hmap_iterator*)p;
		}

		redis_hmap_iterator(redis* pc,lyramilk::data::string redis_key,bool reverse,lyramilk::data::uint64 start):log(lyramilk::klog,"teapoy.native.redis.sorted_set.iterator")
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
				lyramilk::data::var::array cmd;
				cmd.push_back("hkeys");
				cmd.push_back(key);
				lyramilk::data::var vret;
				if(!predis->c->exec(cmd,vret)) return false;
				keys = vret;
				isok = true;
			}
			return true;
		}

		lyramilk::data::var ok(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
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

		lyramilk::data::var next(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
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

		lyramilk::data::var item_key(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			return keys[reverse?(keys.size() - cursor - 1):cursor];
		}

		lyramilk::data::var item_value(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			if(sitem_value.empty()){
				lyramilk::data::var::array cmd;
				cmd.push_back("hget");
				cmd.push_back(key);
				cmd.push_back(keys[cursor]);
				lyramilk::data::var vret;
				if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
				sitem_value = vret.str();
			}
			return sitem_value;
		}

		static int define(bool permanent,lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["ok"] = lyramilk::script::engine::functional<redis_hmap_iterator,&redis_hmap_iterator::ok>;
			fn["next"] = lyramilk::script::engine::functional<redis_hmap_iterator,&redis_hmap_iterator::next>;
			fn["key"] = lyramilk::script::engine::functional<redis_hmap_iterator,&redis_hmap_iterator::item_key>;
			fn["value"] = lyramilk::script::engine::functional<redis_hmap_iterator,&redis_hmap_iterator::item_value>;
			p->define(permanent,"redis.hashmap.iterator",fn,redis_hmap_iterator::ctr,redis_hmap_iterator::dtr);
			return 1;
		}
	};

	class redis_hmap
	{
		lyramilk::log::logss log;
		lyramilk::data::string key;
		redis* predis;
	  public:
		static void* ctr(lyramilk::data::var::array args)
		{
			return new redis_hmap((redis*)args[0].userdata("redis"),args[1]);
		}
		static void dtr(void* p)
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

		lyramilk::data::var getkey(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			return key;
		}

		lyramilk::data::var scan(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			lyramilk::data::uint64 startof = 0;
			if(args.size() > 0){
				const lyramilk::data::var& v = args.at(0);
				if(!v.type_like(lyramilk::data::var::t_uint64)){
					throw lyramilk::exception(D("参数%d类型不兼容:%s，期待%s",1,v.type_name().c_str(),lyramilk::data::var::type_name(lyramilk::data::var::t_uint64).c_str()));
				}
				startof = v;
			}
			lyramilk::script::engine* e = (lyramilk::script::engine*)env.find(lyramilk::script::engine::s_env_engine())->second.userdata(lyramilk::script::engine::s_user_engineptr());
			
			lyramilk::data::var::array ar;
			ar.reserve(4);
			ar.push_back(lyramilk::data::var("redis",predis));
			ar.push_back(key);
			ar.push_back(false);
			ar.push_back(startof);
			return e->createobject("redis.hashmap.iterator",ar);
		}

		lyramilk::data::var rscan(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			lyramilk::data::uint64 startof = 0;
			if(args.size() > 0){
				const lyramilk::data::var& v = args.at(0);
				if(!v.type_like(lyramilk::data::var::t_uint64)){
					throw lyramilk::exception(D("参数%d类型不兼容:%s，期待%s",1,v.type_name().c_str(),lyramilk::data::var::type_name(lyramilk::data::var::t_uint64).c_str()));
				}
				startof = v;
			}
			lyramilk::script::engine* e = (lyramilk::script::engine*)env.find(lyramilk::script::engine::s_env_engine())->second.userdata(lyramilk::script::engine::s_user_engineptr());
			
			lyramilk::data::var::array ar;
			ar.reserve(4);
			ar.push_back(lyramilk::data::var("redis",predis));
			ar.push_back(key);
			ar.push_back(true);
			ar.push_back(startof);
			return e->createobject("redis.hashmap.iterator",ar);
		}

		lyramilk::data::var get(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::var::array cmd;
			cmd.reserve(3);
			cmd.push_back("hget");
			cmd.push_back(key);
			cmd.push_back(args[0]);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		lyramilk::data::var set(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			if(predis->is_readonly()) throw lyramilk::exception(D("redis.hashmap.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_str);
			lyramilk::data::var::array cmd;
			cmd.reserve(4);
			cmd.push_back("hset");
			cmd.push_back(key);
			cmd.push_back(args[0]);
			cmd.push_back(args[1]);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		lyramilk::data::var size(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			lyramilk::data::var::array cmd;
			cmd.reserve(2);
			cmd.push_back("hlen");
			cmd.push_back(key);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		lyramilk::data::var exist(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::var::array cmd;
			cmd.reserve(3);
			cmd.push_back("hexists");
			cmd.push_back(key);
			cmd.push_back(args[0]);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		lyramilk::data::var clear(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			if(predis->is_readonly()) throw lyramilk::exception(D("redis.hashmap.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			if(predis->is_ssdb()){
				lyramilk::data::var::array cmd;
				cmd.reserve(2);
				cmd.push_back("hclear");
				cmd.push_back(key);
				lyramilk::data::var vret;
				if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
				TODO();
				return vret;
			}
			lyramilk::data::var::array cmd;
			cmd.reserve(2);
			cmd.push_back("del");
			cmd.push_back(key);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		static int define(bool permanent,lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["getkey"] = lyramilk::script::engine::functional<redis_hmap,&redis_hmap::getkey>;
			fn["scan"] = lyramilk::script::engine::functional<redis_hmap,&redis_hmap::scan>;
			fn["rscan"] = lyramilk::script::engine::functional<redis_hmap,&redis_hmap::rscan>;
			fn["get"] = lyramilk::script::engine::functional<redis_hmap,&redis_hmap::get>;
			fn["set"] = lyramilk::script::engine::functional<redis_hmap,&redis_hmap::set>;
			fn["exist"] = lyramilk::script::engine::functional<redis_hmap,&redis_hmap::exist>;
			fn["clear"] = lyramilk::script::engine::functional<redis_hmap,&redis_hmap::clear>;
			p->define(permanent,"redis.hashmap",fn,redis_hmap::ctr,redis_hmap::dtr);
			return 1;
		}
	};

	///////////////////////////////

	class redis_list_iterator
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
		static void* ctr(lyramilk::data::var::array args)
		{
			return new redis_list_iterator((redis*)args[0].userdata("redis"),args[1],args[2],args[3]);
		}
		static void dtr(void* p)
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
			lyramilk::data::var::array cmd;
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
				lyramilk::data::var::array& ret = vret;
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

		lyramilk::data::var ok(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			if(cursor == start){
				sync();
			}
			return isok;

		}

		lyramilk::data::var value(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			return list_item_value;
		}

		lyramilk::data::var next(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
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

		static int define(bool permanent,lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["ok"] = lyramilk::script::engine::functional<redis_list_iterator,&redis_list_iterator::ok>;
			fn["value"] = lyramilk::script::engine::functional<redis_list_iterator,&redis_list_iterator::value>;
			fn["next"] = lyramilk::script::engine::functional<redis_list_iterator,&redis_list_iterator::next>;
			p->define(permanent,"redis.list.iterator",fn,redis_list_iterator::ctr,redis_list_iterator::dtr);
			return 1;
		}
	};

	class redis_list
	{
		lyramilk::log::logss log;
		lyramilk::data::string key;
		redis* predis;
	  public:
		static void* ctr(lyramilk::data::var::array args)
		{
			return new redis_list((redis*)args[0].userdata("redis"),args[1]);
		}
		static void dtr(void* p)
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

		lyramilk::data::var getkey(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			return key;
		}

		lyramilk::data::var push_back(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			if(predis->is_readonly()) throw lyramilk::exception(D("redis.list.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::var::array cmd;
			cmd.reserve(3);
			cmd.push_back("rpush");
			cmd.push_back(key);
			cmd.push_back(args[0]);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		lyramilk::data::var push_front(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			if(predis->is_readonly()) throw lyramilk::exception(D("redis.list.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::var::array cmd;
			cmd.reserve(3);
			cmd.push_back("lpush");
			cmd.push_back(key);
			cmd.push_back(args[0]);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		lyramilk::data::var pop_back(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			if(predis->is_readonly()) throw lyramilk::exception(D("redis.list.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			lyramilk::data::var::array cmd;
			cmd.reserve(2);
			cmd.push_back("rpop");
			cmd.push_back(key);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		lyramilk::data::var pop_front(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			if(predis->is_readonly()) throw lyramilk::exception(D("redis.list.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			lyramilk::data::var::array cmd;
			cmd.reserve(2);
			cmd.push_back("lpop");
			cmd.push_back(key);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		lyramilk::data::var front(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			lyramilk::data::var::array cmd;
			cmd.reserve(3);
			cmd.push_back("lindex");
			cmd.push_back(key);
			cmd.push_back(0);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		lyramilk::data::var back(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			lyramilk::data::var::array cmd;
			cmd.reserve(3);
			cmd.push_back("lindex");
			cmd.push_back(key);
			cmd.push_back(-1);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		lyramilk::data::var get(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_int64);
			lyramilk::data::var::array cmd;
			cmd.reserve(3);
			cmd.push_back("lindex");
			cmd.push_back(key);
			cmd.push_back(args[0]);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		lyramilk::data::var size(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			lyramilk::data::var::array cmd;
			cmd.reserve(2);
			cmd.push_back("llen");
			cmd.push_back(key);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		lyramilk::data::var scan(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			lyramilk::data::uint64 startof = 0;
			if(args.size() > 0){
				const lyramilk::data::var& v = args.at(0);
				if(!v.type_like(lyramilk::data::var::t_uint64)){
					throw lyramilk::exception(D("参数%d类型不兼容:%s，期待%s",1,v.type_name().c_str(),lyramilk::data::var::type_name(lyramilk::data::var::t_uint64).c_str()));
				}
				startof = v;
			}
			lyramilk::script::engine* e = (lyramilk::script::engine*)env.find(lyramilk::script::engine::s_env_engine())->second.userdata(lyramilk::script::engine::s_user_engineptr());
			
			lyramilk::data::var::array ar;
			ar.reserve(4);
			ar.push_back(lyramilk::data::var("redis",predis));
			ar.push_back(key);
			ar.push_back(false);
			ar.push_back(startof);
			return e->createobject("redis.list.iterator",ar);
		}

		lyramilk::data::var rscan(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			lyramilk::data::uint64 startof = 0;
			if(args.size() > 0){
				const lyramilk::data::var& v = args.at(0);
				if(!v.type_like(lyramilk::data::var::t_uint64)){
					throw lyramilk::exception(D("参数%d类型不兼容:%s，期待%s",1,v.type_name().c_str(),lyramilk::data::var::type_name(lyramilk::data::var::t_uint64).c_str()));
				}
				startof = v;
			}
			lyramilk::script::engine* e = (lyramilk::script::engine*)env.find(lyramilk::script::engine::s_env_engine())->second.userdata(lyramilk::script::engine::s_user_engineptr());
			
			lyramilk::data::var::array ar;
			ar.reserve(4);
			ar.push_back(lyramilk::data::var("redis",predis));
			ar.push_back(key);
			ar.push_back(true);
			ar.push_back(startof);
			return e->createobject("redis.list.iterator",ar);
		}

		lyramilk::data::var clear(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			if(predis->is_readonly()) throw lyramilk::exception(D("redis.list.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			if(predis->is_ssdb()){
				lyramilk::data::var::array cmd;
				cmd.reserve(2);
				cmd.push_back("qclear");
				cmd.push_back(key);
				lyramilk::data::var vret;
				if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
				TODO();
			}

			lyramilk::data::var::array cmd;
			cmd.reserve(2);
			cmd.push_back("del");
			cmd.push_back(key);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret != 0;
		}

		lyramilk::data::var random(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_uint64);
			lyramilk::data::uint64 n = args[0];
			lyramilk::data::uint64 max = size(args,env);

			srand(time(0));

			std::map<lyramilk::data::uint64,bool> m;
			lyramilk::data::var::array ret;
			if(max == 0) return ret;
			ret.reserve(n);
			lyramilk::data::int64 hp = 10;
			lyramilk::script::engine* e = nullptr;
			if (args.size() > 1 && args[1].type() == lyramilk::data::var::t_user){
				e = (lyramilk::script::engine*)env.find(lyramilk::script::engine::s_env_engine())->second.userdata(lyramilk::script::engine::s_user_engineptr());
			}

			while(ret.size() < n && hp > 0){
				lyramilk::data::uint64 index = rand() % max;
				std::pair<std::map<lyramilk::data::uint64,bool>::iterator,bool> checkr = m.insert(std::pair<lyramilk::data::uint64,bool>(index,true));
				if(checkr.second){
					lyramilk::data::var::array ar;
					ar.push_back(index);
					lyramilk::data::var v = get(ar,env);
					if(v.type() == lyramilk::data::var::t_str){
						lyramilk::data::string str = v.str();

						if(e){
							//转换函数
							lyramilk::data::var::array ar;
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

		static int define(bool permanent,lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["getkey"] = lyramilk::script::engine::functional<redis_list,&redis_list::getkey>;
			fn["push_back"] = lyramilk::script::engine::functional<redis_list,&redis_list::push_back>;
			fn["push_front"] = lyramilk::script::engine::functional<redis_list,&redis_list::push_front>;
			fn["pop_back"] = lyramilk::script::engine::functional<redis_list,&redis_list::pop_back>;
			fn["pop_front"] = lyramilk::script::engine::functional<redis_list,&redis_list::pop_front>;
			fn["front"] = lyramilk::script::engine::functional<redis_list,&redis_list::front>;
			fn["back"] = lyramilk::script::engine::functional<redis_list,&redis_list::back>;
			fn["get"] = lyramilk::script::engine::functional<redis_list,&redis_list::get>;
			fn["size"] = lyramilk::script::engine::functional<redis_list,&redis_list::size>;
			fn["scan"] = lyramilk::script::engine::functional<redis_list,&redis_list::scan>;
			fn["rscan"] = lyramilk::script::engine::functional<redis_list,&redis_list::rscan>;
			fn["clear"] = lyramilk::script::engine::functional<redis_list,&redis_list::clear>;
			fn["random"] = lyramilk::script::engine::functional<redis_list,&redis_list::random>;
			p->define(permanent,"redis.list",fn,redis_list::ctr,redis_list::dtr);
			return 1;
		}
	};

	class redis_kv
	{
		lyramilk::log::logss log;
		lyramilk::data::string key;
		redis* predis;
	  public:
		static void* ctr(lyramilk::data::var::array args)
		{
			return new redis_kv((redis*)args[0].userdata("redis"),args[1]);
		}
		static void dtr(void* p)
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

		lyramilk::data::var getkey(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			return key;
		}

		lyramilk::data::var get(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			lyramilk::data::var::array cmd;
			cmd.reserve(2);
			cmd.push_back("get");
			cmd.push_back(key);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		lyramilk::data::var set(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			if(predis->is_readonly()) throw lyramilk::exception(D("redis.kv.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::var::array cmd;
			cmd.reserve(3);
			cmd.push_back("set");
			cmd.push_back(key);
			cmd.push_back(args[0]);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		lyramilk::data::var incr(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			if(predis->is_readonly()) throw lyramilk::exception(D("redis.kv.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			lyramilk::data::var::array cmd;
			cmd.reserve(2);
			cmd.push_back("incr");
			cmd.push_back(key);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		lyramilk::data::var decr(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			if(predis->is_readonly()) throw lyramilk::exception(D("redis.kv.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			lyramilk::data::var::array cmd;
			cmd.reserve(2);
			cmd.push_back("decr");
			cmd.push_back(key);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		lyramilk::data::var clear(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			if(predis->is_readonly()) throw lyramilk::exception(D("redis.kv.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			lyramilk::data::var::array cmd;
			cmd.reserve(2);
			cmd.push_back("del");
			cmd.push_back(key);
			lyramilk::data::var vret;
			if(!predis->c->exec(cmd,vret)) return lyramilk::data::var::nil;
			return vret;
		}

		static int define(bool permanent,lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["getkey"] = lyramilk::script::engine::functional<redis_kv,&redis_kv::getkey>;
			fn["get"] = lyramilk::script::engine::functional<redis_kv,&redis_kv::get>;
			fn["set"] = lyramilk::script::engine::functional<redis_kv,&redis_kv::set>;
			fn["incr"] = lyramilk::script::engine::functional<redis_kv,&redis_kv::incr>;
			fn["decr"] = lyramilk::script::engine::functional<redis_kv,&redis_kv::decr>;
			fn["clear"] = lyramilk::script::engine::functional<redis_kv,&redis_kv::clear>;
			p->define(permanent,"redis.kv",fn,redis_kv::ctr,redis_kv::dtr);
			return 1;
		}
	};


	static int define(bool permanent,lyramilk::script::engine* p)
	{
		int i = 0;
		i+= redis::define(permanent,p);
		i+= redis_zset::define(permanent,p);
		i+= redis_zset_iterator::define(permanent,p);
		i+= redis_hmap::define(permanent,p);
		i+= redis_hmap_iterator::define(permanent,p);
		i+= redis_list::define(permanent,p);
		i+= redis_list_iterator::define(permanent,p);
		i+= redis_kv::define(permanent,p);
		return i;
	}

	static __attribute__ ((constructor)) void __init()
	{
		lyramilk::teapoy::script2native::instance()->regist("redis",define);
	}
}}}
