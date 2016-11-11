#include "script.h"
#include "env.h"
#include "stringutil.h"
#include <libmilk/var.h>
#include <libmilk/log.h>
#include <libmilk/multilanguage.h>
#include <libmilk/factory.hpp>
#include "redis.h"
#include <queue>
#include <sys/socket.h>
#include <stdlib.h>

namespace lyramilk{ namespace teapoy{ namespace native {
	using namespace lyramilk::teapoy::redis;

	class redislog:public lyramilk::teapoy::redis::redis_client_listener
	{
		virtual void notify(const char* str,int len,lyramilk::teapoy::redis::redis_reply& reply)
		{
			if(reply.good()){
				lyramilk::data::string replystatus = "?unknow";
				switch(reply.type()){
				  case lyramilk::teapoy::redis::redis_reply::t_unknow:
					replystatus = "Unknow";
					break;
				  case lyramilk::teapoy::redis::redis_reply::t_string:
					replystatus = "String";
					{
						lyramilk::data::string msg = D("执行命令%s成功，reply类型%s(%d)，值=%s",str,replystatus.c_str(),reply.type(),reply.c_str());
						lyramilk::klog(lyramilk::log::debug,"hiredis.log") << msg << std::endl;
					}
					return;
				  case lyramilk::teapoy::redis::redis_reply::t_array:
					replystatus = "Array";
					break;
				  case lyramilk::teapoy::redis::redis_reply::t_integer:
					replystatus = "Integer";
					{
						lyramilk::data::string msg = D("执行命令%s成功，reply类型%s(%d)，值=%lld",str,replystatus.c_str(),reply.type(),reply.toint());
						lyramilk::klog(lyramilk::log::debug,"hiredis.log") << msg << std::endl;
					}
					return;
				  case lyramilk::teapoy::redis::redis_reply::t_nil:
					replystatus = "Nil";
					break;
				  case lyramilk::teapoy::redis::redis_reply::t_status:
					replystatus = "Status";
					{
						lyramilk::data::string msg = D("执行命令%s成功，reply类型%s(%d)",str,replystatus.c_str(),reply.type());
						lyramilk::klog(lyramilk::log::debug,"hiredis.log") << msg << std::endl;
					}
					return;
				  case lyramilk::teapoy::redis::redis_reply::t_error:
					replystatus = "Error";
					break;
				}
				lyramilk::data::string msg = D("执行命令%s成功，reply类型%s(%d)",str,replystatus.c_str(),reply.type());
				lyramilk::klog(lyramilk::log::debug,"hiredis.log") << msg << std::endl;
			}else{
				lyramilk::data::string msg = D("执行命令%s失败:%s",str,reply.str().c_str());
				lyramilk::klog(lyramilk::log::error,"hiredis.log") << msg << std::endl;
			}
		}
	};



	class redis_clients:public lyramilk::threading::exclusive::list<lyramilk::teapoy::redis::redis_client>
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

		virtual lyramilk::teapoy::redis::redis_client* underflow()
		{
			lyramilk::teapoy::redis::redis_client* p = new lyramilk::teapoy::redis::redis_client();
			if(!p) return nullptr;
			if(!pwd.empty()){
				p->open(host.c_str(),port,pwd);
			}else{
				p->open(host.c_str(),port);
			}
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


	class hiredis
	{
	  public:
		lyramilk::log::logss log;
		redislog rlog;
		redis_clients::ptr c;
		bool isreadonly;

		static void* ctr(lyramilk::data::var::array args)
		{
			hiredis* r = nullptr;
			if(args.size() == 2){
				r = new hiredis();
				if(r && r->open(args[0],args[1])){
				}else{
					delete r;
					throw lyramilk::exception(D("连接失败"));
				};
			}else if(args.size() == 3){
				r = new hiredis();
				if(r && r->open(args[0],args[1],args[2])){
				}else{
					delete r;
					throw lyramilk::exception(D("连接失败"));
				}
			}else if(args.size() == 1){
				lyramilk::data::var& v = args[0];
				lyramilk::data::string host = v["host"];
				lyramilk::data::uint16 port = v["port"];
				lyramilk::data::string pwd = v["password"];
				r = new hiredis();
				if(r && r->open(host,port,pwd)){
				}else{
					delete r;
					throw lyramilk::exception(D("连接失败"));
				}
			}
			if(r != nullptr){
				if(env::is_debug()){
					//r->c->set_listener(&r->rlog);
				}
			}
			return r;
		}
		static void dtr(void* p)
		{
			delete (hiredis*)p;
		}

		hiredis():log(lyramilk::klog,"teapoy.native.hiredis")
		{
			isreadonly = false;
		}

		bool open(lyramilk::data::string host,lyramilk::data::uint16 port,lyramilk::data::string pwd = "")
		{
			c = redis_clients_multiton::instance()->getobj(host,port,pwd);
			if(!c) return false;
			return true;
		}

		~hiredis()
		{
		}

		lyramilk::data::var redis_param_2_var(redis_param* v)
		{
			switch(v->type()){
				//t_unknow = 256,t_string,t_array,t_integer,t_nil,t_status,t_error
			  case redis_param::t_string:
				return v->str();
				break;
			  case redis_param::t_array:{
					lyramilk::data::var::array ar;
					redis_param::iterator it = v->begin();
					for(;it!=v->end();++it){
						redis_param rp = *it;
						ar.push_back(redis_param_2_var(&rp));
					}
					return ar;
				}
				break;
			  case redis_param::t_integer:
				return v->toint();
				break;
			  default:
				return lyramilk::data::var::nil;
			}
			return "";
		}

		lyramilk::data::var exec(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);

			switch(args.size()){
			  case 1:{
					redis_reply ret = c->execute(args[0].str().c_str());
					if(!ret.good()) return lyramilk::data::var::nil;
					return redis_param_2_var(&ret);
				}break;
			  case 2:{
					redis_reply ret = c->execute(args[0].str().c_str(),args[1].str().c_str());
					if(!ret.good()) return lyramilk::data::var::nil;
					return redis_param_2_var(&ret);
				}break;
			  case 3:{
					redis_reply ret = c->execute(args[0].str().c_str(),args[1].str().c_str(),args[2].str().c_str());
					if(!ret.good()) return lyramilk::data::var::nil;
					return redis_param_2_var(&ret);
				}break;
			}
			log(lyramilk::log::error,__FUNCTION__) << D("参数过多%d\n",args.size()) << std::endl;
			return lyramilk::data::var::nil;
		}

		lyramilk::data::var clear(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			if(isreadonly) throw lyramilk::exception(D("hiredis.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			redis_reply ret = c->execute("flushdb");
			if(!ret.good()) return lyramilk::data::var::nil;
			return redis_param_2_var(&ret);
		}

		lyramilk::data::var readonly(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			if(args.size() > 0){
				MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_bool);
				isreadonly = args[0];
			}
			return isreadonly;
		}

		lyramilk::data::var newid(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			if(isreadonly) throw lyramilk::exception(D("hiredis.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			if(args.size() == 0){
				redis_reply ret = c->execute("incr ___global_autoincrease_id");
				if(!ret.good()) return lyramilk::data::var::nil;
				return ret.toint();
			}else{
				MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
				lyramilk::data::string skey = args[0];
				skey += ":_lastid_";
				redis_reply ret = c->execute("incr %s",skey.c_str());
				if(!ret.good()) return lyramilk::data::var::nil;
				return ret.toint();
			}
		}

		lyramilk::data::var get(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string key = args[0];

			redis_reply ret = c->execute("get %s",key.c_str());
			if(!ret.good()) return lyramilk::data::var::nil;
			if(ret.type() != redis_reply::t_string) return lyramilk::data::var::nil;
			return ret.str();
		}

		lyramilk::data::var set(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			if(isreadonly) throw lyramilk::exception(D("hiredis.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_str);
			lyramilk::data::string key = args[0];
			lyramilk::data::string value = args[1];

			redis_reply ret = c->execute("set %s %s",key.c_str(),value.c_str());
			if(!ret.good()) return lyramilk::data::var::nil;
			return ret.type() != redis_reply::t_status;
		}

		lyramilk::data::var ok(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			return c->alive();
		}

		lyramilk::data::var getkv(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string key = args[0];
			lyramilk::script::engine* e = (lyramilk::script::engine*)env[lyramilk::script::engine::s_env_engine()].userdata(lyramilk::script::engine::s_user_engineptr());
			
			lyramilk::data::var::array ar;
			ar.push_back(lyramilk::data::var("hiredis",this));
			ar.push_back(key);
			return e->createobject("hiredis.kv",ar);
		}

		lyramilk::data::var gethmap(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string key = args[0];
			lyramilk::script::engine* e = (lyramilk::script::engine*)env[lyramilk::script::engine::s_env_engine()].userdata(lyramilk::script::engine::s_user_engineptr());
			
			lyramilk::data::var::array ar;
			ar.push_back(lyramilk::data::var("hiredis",this));
			ar.push_back(key);
			return e->createobject("hiredis.hmap",ar);
		}

		lyramilk::data::var getzset(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string key = args[0];
			lyramilk::script::engine* e = (lyramilk::script::engine*)env[lyramilk::script::engine::s_env_engine()].userdata(lyramilk::script::engine::s_user_engineptr());
			
			lyramilk::data::var::array ar;
			ar.push_back(lyramilk::data::var("hiredis",this));
			ar.push_back(key);
			return e->createobject("hiredis.zset",ar);
		}

		lyramilk::data::var getlist(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string key = args[0];
			lyramilk::script::engine* e = (lyramilk::script::engine*)env[lyramilk::script::engine::s_env_engine()].userdata(lyramilk::script::engine::s_user_engineptr());
			
			lyramilk::data::var::array ar;
			ar.push_back(lyramilk::data::var("hiredis",this));
			ar.push_back(key);
			return e->createobject("hiredis.list",ar);
		}

		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["readonly"] = lyramilk::script::engine::functional<hiredis,&hiredis::readonly>;
			fn["exec"] = lyramilk::script::engine::functional<hiredis,&hiredis::exec>;
			fn["clear"] = lyramilk::script::engine::functional<hiredis,&hiredis::clear>;
			fn["newid"] = lyramilk::script::engine::functional<hiredis,&hiredis::newid>;
			fn["ok"] = lyramilk::script::engine::functional<hiredis,&hiredis::ok>;
			fn["get"] = lyramilk::script::engine::functional<hiredis,&hiredis::get>;
			fn["set"] = lyramilk::script::engine::functional<hiredis,&hiredis::set>;
			fn["getkv"] = lyramilk::script::engine::functional<hiredis,&hiredis::getkv>;
			fn["gethmap"] = lyramilk::script::engine::functional<hiredis,&hiredis::gethmap>;
			fn["getzset"] = lyramilk::script::engine::functional<hiredis,&hiredis::getzset>;
			fn["getlist"] = lyramilk::script::engine::functional<hiredis,&hiredis::getlist>;
			p->define("HiRedis",fn,hiredis::ctr,hiredis::dtr);
			return 1;
		}
	};

	class hiredis_zset_iterator
	{
		lyramilk::log::logss log;
		hiredis* pmaster;
		lyramilk::data::string hiredis_key;
		lyramilk::data::uint64 cursor;

		lyramilk::data::string zset_key;
		lyramilk::data::string zset_value;
		std::queue<std::pair<lyramilk::data::string,lyramilk::data::string> > dq;

		bool status;
		bool reverse;
		lyramilk::data::uint64 start;
	  public:
		static void* ctr(lyramilk::data::var::array args)
		{
			assert(args.size() > 1);
			if(args.size() == 2) return new hiredis_zset_iterator((hiredis*)args[0].userdata("hiredis"),args[1],false);
			if(args.size() == 4) return new hiredis_zset_iterator((hiredis*)args[0].userdata("hiredis"),args[1],args[2],args[3]);
			return new hiredis_zset_iterator((hiredis*)args[0].userdata("hiredis"),args[1],args[2]);
		}
		static void dtr(void* p)
		{
			delete (hiredis_zset_iterator*)p;
		}

		hiredis_zset_iterator(hiredis* pc,lyramilk::data::string hiredis_key,bool reverse,lyramilk::data::uint64 start = 0):log(lyramilk::klog,"teapoy.native.hiredis.sorted_set.iterator")
		{
			this->reverse = reverse;
			pmaster = pc;
			this->hiredis_key = hiredis_key;
			status = false;
			this->start = start;
			this->cursor = start;
		}

		virtual ~hiredis_zset_iterator()
		{
		}

		bool flush()
		{
			if(!dq.empty()) return true;
			lyramilk::data::string cmd;
			lyramilk::data::string cmd2;
			if(this->reverse){
				cmd = "zrevrangebyscore %s +inf -inf WITHSCORES limit %llu 20";
				cmd2 = "zrevrank %s %s";
			}else{
				cmd = "zrangebyscore %s -inf +inf WITHSCORES limit %llu 20";
				cmd2 = "zrank %s %s";
			}
			redis_reply ret = pmaster->c->execute(cmd.c_str(),hiredis_key.c_str(),cursor);
			if(!ret.good()) return false;
			if(ret.type() != redis_reply::t_array) return false;

			for(unsigned long long i=0;i<ret.size();i+=2){
				lyramilk::data::string k = ret.at(i).str();
				lyramilk::data::string s = ret.at(i+1).str();
				std::pair<lyramilk::data::string,lyramilk::data::string> pr(k,s);
				dq.push(pr);
			}
			if(dq.size() == 0) return false;
			{
				lyramilk::data::string pageeof = ret.at(ret.size()-2).str();
				redis_reply ret = pmaster->c->execute(cmd2.c_str(),hiredis_key.c_str(),pageeof.c_str());

				if(!ret.good()) return false;
				if(ret.type() != redis_reply::t_integer) return false;

				cursor = ret.toint() + 1;
			}

			if(zset_key.empty() && zset_value.empty()){
				std::pair<lyramilk::data::string,lyramilk::data::string>& pr = dq.front();
				zset_key = pr.first;
				zset_value = pr.second;
				status = true;
				dq.pop();
			}
			return true;
		}

		lyramilk::data::var ok(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			if(cursor == this->start){
				flush();
			}
			return status;
		}

		lyramilk::data::var value(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			return zset_key;
		}

		lyramilk::data::var score(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			if(zset_value.find('.') == zset_value.npos){
				lyramilk::data::var v = zset_value;
				return (lyramilk::data::uint64)v;
			}
			lyramilk::data::var v = zset_value;
			return (double)v;
		}


		lyramilk::data::var next(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			status = false;
			if(dq.size() == 0 && !flush()) return false;

			std::pair<lyramilk::data::string,lyramilk::data::string>& pr = dq.front();
			zset_key = pr.first;
			zset_value = pr.second;
			status = true;
			dq.pop();
			return true;
		}

		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["value"] = lyramilk::script::engine::functional<hiredis_zset_iterator,&hiredis_zset_iterator::value>;
			fn["score"] = lyramilk::script::engine::functional<hiredis_zset_iterator,&hiredis_zset_iterator::score>;
			fn["ok"] = lyramilk::script::engine::functional<hiredis_zset_iterator,&hiredis_zset_iterator::ok>;
			fn["next"] = lyramilk::script::engine::functional<hiredis_zset_iterator,&hiredis_zset_iterator::next>;
			p->define("hiredis.zset.iterator",fn,hiredis_zset_iterator::ctr,hiredis_zset_iterator::dtr);
			return 1;
		}
	};


	class hiredis_zset
	{
		lyramilk::log::logss log;
		hiredis* pmaster;
		lyramilk::data::string key;
	  public:
		static void* ctr(lyramilk::data::var::array args)
		{
			assert(args.size() == 2);
			return new hiredis_zset((hiredis*)args[0].userdata("hiredis"),args[1]);
		}
		static void dtr(void* p)
		{
			delete (hiredis_zset*)p;
		}

		hiredis_zset(hiredis* pc,lyramilk::data::string key):log(lyramilk::klog,"teapoy.native.hiredis.sorted_set")
		{
			pmaster = pc;
			this->key = key;
		}

		~hiredis_zset()
		{
		}

		lyramilk::data::var getkey(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			return key;
		}

		lyramilk::data::var scan(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			//MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::uint64 startof = 0;
			if(args.size() > 0){
				lyramilk::data::var& v = args.at(0);
				if(!v.type_compat(lyramilk::data::var::t_uint64)){
					throw lyramilk::exception(D("参数%d类型不兼容:%s，期待%s",1,v.type_name().c_str(),lyramilk::data::var::type_name(lyramilk::data::var::t_uint64).c_str()));
				}
				startof = v;
			}

			lyramilk::script::engine* e = (lyramilk::script::engine*)env[lyramilk::script::engine::s_env_engine()].userdata(lyramilk::script::engine::s_user_engineptr());
			
			lyramilk::data::var::array ar;
			ar.push_back(lyramilk::data::var("hiredis",pmaster));
			ar.push_back(key);
			ar.push_back(false);
			ar.push_back(startof);
			return e->createobject("hiredis.zset.iterator",ar);
		}

		lyramilk::data::var rscan(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			lyramilk::data::uint64 startof = 0;
			if(args.size() > 0){
				lyramilk::data::var& v = args.at(0);
				if(!v.type_compat(lyramilk::data::var::t_uint64)){
					throw lyramilk::exception(D("参数%d类型不兼容:%s，期待%s",1,v.type_name().c_str(),lyramilk::data::var::type_name(lyramilk::data::var::t_uint64).c_str()));
				}
				startof = v;
			}

			lyramilk::script::engine* e = (lyramilk::script::engine*)env[lyramilk::script::engine::s_env_engine()].userdata(lyramilk::script::engine::s_user_engineptr());
			
			lyramilk::data::var::array ar;
			ar.push_back(lyramilk::data::var("hiredis",pmaster));
			ar.push_back(key);
			ar.push_back(true);
			ar.push_back(startof);
			return e->createobject("hiredis.zset.iterator",ar);
		}

		lyramilk::data::var add(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			if(pmaster->isreadonly) throw lyramilk::exception(D("hiredis.zset.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_uint64);
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_str);
			lyramilk::data::string str = args[1];
			if(pmaster->c->is_ssdb() && str.size() > 200){
				throw lyramilk::exception(D("数据库错误：SSDB禁止向zset插入超长键%d:%s",str.size(),str.c_str()));
			}
			if(args[0].type() != lyramilk::data::var::t_double){
				lyramilk::data::uint64 score = args[0];
				redis_reply ret = pmaster->c->execute("zadd %s %llu %s",key.c_str(),score,str.c_str());
				if(!ret.good()) return lyramilk::data::var::nil;
				return ret.toint() == 1;
			}

			double score = args[0];
			redis_reply ret = pmaster->c->execute("zadd %s %f %s",key.c_str(),score,str.c_str());
			if(!ret.good()) return lyramilk::data::var::nil;
			return ret.toint() == 1;
		}

		lyramilk::data::var remove(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			if(pmaster->isreadonly) throw lyramilk::exception(D("hiredis.zset.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string str = args[0];

			redis_reply ret = pmaster->c->execute("zrem %s %s",key.c_str(),str.c_str());
			if(!ret.good()) return lyramilk::data::var::nil;
			return ret.toint() == 1;
		}

		lyramilk::data::var size(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			redis_reply ret = pmaster->c->execute("zcard %s",key.c_str());
			if(!ret.good()) return lyramilk::data::var::nil;
			if(ret.type() != redis_reply::t_integer) throw lyramilk::exception(D("数据库错误"));
			return ret.toint();
		}

		lyramilk::data::var count(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			if(args.size() == 0){
				redis_reply ret = pmaster->c->execute("zcount %s -inf +inf",key.c_str());
				if(!ret.good()) return lyramilk::data::var::nil;
				return ret.toint();
			}
			if(args.size() == 1){
				redis_reply ret = pmaster->c->execute("zcount %s %s +inf",key.c_str(),args[0].str().c_str());
				if(!ret.good()) return lyramilk::data::var::nil;
				return ret.toint();
			}
			if(args.size() >= 2){
				redis_reply ret = pmaster->c->execute("zcount %s %s %s",key.c_str(),args[0].str().c_str(),args[1].str().c_str());
				if(!ret.good()) return lyramilk::data::var::nil;
				return ret.toint();
			}
			throw lyramilk::exception(D("参数错误"));
		}

		lyramilk::data::var clear(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			if(pmaster->isreadonly) throw lyramilk::exception(D("hiredis.zset.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			if(pmaster->c->is_ssdb()){
				lyramilk::data::var::array ar;
				ar.push_back("zclear");
				ar.push_back(key);
				lyramilk::data::var::array ret = pmaster->c->ssdb_exec(ar);

				if(ret.size() != 2) throw lyramilk::exception(D("数据库错误"));
				int cnt = ret[1];
				return cnt > 0;
			}
			redis_reply ret = pmaster->c->execute("del %s",key.c_str());
			if(!ret.good()) return lyramilk::data::var::nil;
			if(ret.type() != redis_reply::t_integer) throw lyramilk::exception(D("数据库错误"));
			return ret.toint() == 1;
		}

		lyramilk::data::var score(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string str = args[0];

			redis_reply ret = pmaster->c->execute("zscore %s %s",key.c_str(),str.c_str());
			if(!ret.good()) return lyramilk::data::var::nil;
			if(ret.type() == redis_reply::t_nil) return lyramilk::data::var::nil;

			lyramilk::data::string retstr = ret.str();
			lyramilk::data::var v(retstr);
			if(retstr.find('.') == retstr.npos){
				return (lyramilk::data::int64)v;
			}
			return (double)v;
		}

		lyramilk::data::var at(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_int64);
			lyramilk::data::int64 i = args[0];

			redis_reply ret = pmaster->c->execute("zrange %s %lld %lld",key.c_str(),i,i);
			if(!ret.good()) return lyramilk::data::var::nil;
			if(ret.type() != redis_reply::t_array) return lyramilk::data::var::nil;

			for(unsigned long long i=0;i<ret.size();++i){
				lyramilk::data::string k = ret.at(i).str();
				return k;
			}
			return lyramilk::data::var::nil;
		}

		lyramilk::data::var random(lyramilk::data::var::array args,lyramilk::data::var::map env)
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
				e = (lyramilk::script::engine*)env[lyramilk::script::engine::s_env_engine()].userdata(lyramilk::script::engine::s_user_engineptr());
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

		lyramilk::data::var rename(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string str = args[0];

			redis_reply ret = pmaster->c->execute("rename %s %s",key.c_str(),str.c_str());
			if(!ret.good()) return lyramilk::data::var::nil;
			key = str;
			return lyramilk::data::var::nil;
		}

		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["getkey"] = lyramilk::script::engine::functional<hiredis_zset,&hiredis_zset::getkey>;
			fn["scan"] = lyramilk::script::engine::functional<hiredis_zset,&hiredis_zset::scan>;
			fn["rscan"] = lyramilk::script::engine::functional<hiredis_zset,&hiredis_zset::rscan>;
			fn["add"] = lyramilk::script::engine::functional<hiredis_zset,&hiredis_zset::add>;
			fn["remove"] = lyramilk::script::engine::functional<hiredis_zset,&hiredis_zset::remove>;
			fn["size"] = lyramilk::script::engine::functional<hiredis_zset,&hiredis_zset::size>;
			fn["count"] = lyramilk::script::engine::functional<hiredis_zset,&hiredis_zset::count>;
			fn["clear"] = lyramilk::script::engine::functional<hiredis_zset,&hiredis_zset::clear>;
			fn["score"] = lyramilk::script::engine::functional<hiredis_zset,&hiredis_zset::score>;
			fn["at"] = lyramilk::script::engine::functional<hiredis_zset,&hiredis_zset::at>;
			fn["random"] = lyramilk::script::engine::functional<hiredis_zset,&hiredis_zset::random>;
			fn["rename"] = lyramilk::script::engine::functional<hiredis_zset,&hiredis_zset::rename>;
			p->define("hiredis.zset",fn,hiredis_zset::ctr,hiredis_zset::dtr);
			return 1;
		}
	};

	class hiredis_hmap_iterator
	{
		lyramilk::log::logss log;
		hiredis* pmaster;
		lyramilk::data::string hiredis_key;
	  public:
		static void* ctr(lyramilk::data::var::array args)
		{
			assert(args.size() == 2);
			return new hiredis_hmap_iterator((hiredis*)args[0].userdata("hiredis"),args[1]);
		}
		static void dtr(void* p)
		{
			delete (hiredis_hmap_iterator*)p;
		}

		hiredis_hmap_iterator(hiredis* pc,lyramilk::data::string hiredis_key):log(lyramilk::klog,"teapoy.native.hiredis.sorted_set.iterator")
		{
			pmaster = pc;
			this->hiredis_key = hiredis_key;
		}

		~hiredis_hmap_iterator()
		{
		}

		lyramilk::data::var ok(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			TODO();
		}

		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["ok"] = lyramilk::script::engine::functional<hiredis_hmap_iterator,&hiredis_hmap_iterator::ok>;
			p->define("hiredis.hmap.iterator",fn,hiredis_hmap_iterator::ctr,hiredis_hmap_iterator::dtr);
			return 1;
		}
	};

	class hiredis_hmap
	{
		lyramilk::log::logss log;
		hiredis* pmaster;
		lyramilk::data::string key;
	  public:
		static void* ctr(lyramilk::data::var::array args)
		{
			assert(args.size() == 2);
			return new hiredis_hmap((hiredis*)args[0].userdata("hiredis"),args[1]);
		}
		static void dtr(void* p)
		{
			delete (hiredis_hmap*)p;
		}

		hiredis_hmap(hiredis* pc,lyramilk::data::string key):log(lyramilk::klog,"teapoy.native.hiredis.hashmap")
		{
			pmaster = pc;
			this->key = key;
		}

		~hiredis_hmap()
		{
		}

		lyramilk::data::var getkey(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			return key;
		}

		lyramilk::data::var scan(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			lyramilk::script::engine* e = (lyramilk::script::engine*)env[lyramilk::script::engine::s_env_engine()].userdata(lyramilk::script::engine::s_user_engineptr());
			lyramilk::data::var::array ar;
			ar.push_back(lyramilk::data::var("hiredis",pmaster));
			ar.push_back(key);
			return e->createobject("hiredis.hmap.iterator",ar);
		}

		lyramilk::data::var get(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string field = args[0];

			redis_reply ret = pmaster->c->execute("hget %s %s",key.c_str(),field.c_str());
			if(!ret.good()) return lyramilk::data::var::nil;
			if(ret.type() != redis_reply::t_string && ret.type() != redis_reply::t_integer) return lyramilk::data::var::nil;
			return ret.str();
		}

		lyramilk::data::var set(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			if(pmaster->isreadonly) throw lyramilk::exception(D("hiredis.hashmap.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_str);


			lyramilk::data::string field = args[0];
			lyramilk::data::string value = args[1];

			redis_reply ret = pmaster->c->execute("hset %s %s %s",key.c_str(),field.c_str(),value.c_str());
			if(!ret.good()) return lyramilk::data::var::nil;
			if(ret.type() != redis_reply::t_integer) return false;
			return ret.toint();
		}

		lyramilk::data::var size(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			redis_reply ret = pmaster->c->execute("hlen %s",key.c_str());
			if(!ret.good()) return false;
			if(ret.type() != redis_reply::t_integer) throw lyramilk::exception(D("数据库错误"));

			return ret.toint();
		}

		lyramilk::data::var exist(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);

			lyramilk::data::string field = args[0];

			redis_reply ret = pmaster->c->execute("hexists %s %s",key.c_str(),field.c_str());
			if(!ret.good()) return lyramilk::data::var::nil;
			if(ret.type() != redis_reply::t_integer) return true;
			return ret.toint() == 1;
		}

		lyramilk::data::var clear(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			if(pmaster->isreadonly) throw lyramilk::exception(D("hiredis.hashmap.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			if(pmaster->c->is_ssdb()){
				lyramilk::data::var::array ar;
				ar.push_back("hclear");
				ar.push_back(key);
				lyramilk::data::var::array ret = pmaster->c->ssdb_exec(ar);

				if(ret.size() != 2) throw lyramilk::exception(D("数据库错误"));
				int cnt = ret[1];
				return cnt > 0;
			}

			redis_reply ret = pmaster->c->execute("del %s",key.c_str());
			if(!ret.good()) return lyramilk::data::var::nil;
			if(ret.type() != redis_reply::t_integer) throw lyramilk::exception(D("数据库错误"));
			return ret.toint() == 1;
		}

		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["getkey"] = lyramilk::script::engine::functional<hiredis_hmap,&hiredis_hmap::getkey>;
			fn["scan"] = lyramilk::script::engine::functional<hiredis_hmap,&hiredis_hmap::scan>;
			fn["get"] = lyramilk::script::engine::functional<hiredis_hmap,&hiredis_hmap::get>;
			fn["set"] = lyramilk::script::engine::functional<hiredis_hmap,&hiredis_hmap::set>;
			fn["exist"] = lyramilk::script::engine::functional<hiredis_hmap,&hiredis_hmap::exist>;
			fn["clear"] = lyramilk::script::engine::functional<hiredis_hmap,&hiredis_hmap::clear>;
			p->define("hiredis.hmap",fn,hiredis_hmap::ctr,hiredis_hmap::dtr);
			return 1;
		}
	};

	///////////////////////////////

	class hiredis_list_iterator
	{
		lyramilk::log::logss log;
		hiredis* pmaster;
		lyramilk::data::string hiredis_key;
		bool status;

		lyramilk::data::string list_value;
		std::queue<lyramilk::data::string> dq;

		long long cursor;
	  public:
		static void* ctr(lyramilk::data::var::array args)
		{
			assert(args.size() == 2);
			return new hiredis_list_iterator((hiredis*)args[0].userdata("hiredis"),args[1]);
		}
		static void dtr(void* p)
		{
			delete (hiredis_list_iterator*)p;
		}

		hiredis_list_iterator(hiredis* pc,lyramilk::data::string hiredis_key):log(lyramilk::klog,"teapoy.native.hiredis.list.iterator")
		{
			pmaster = pc;
			this->hiredis_key = hiredis_key;
			status = false;
			cursor = -1;
		}

		~hiredis_list_iterator()
		{
		}

		bool flush()
		{
			if(!dq.empty()) return true;
			bool first = (cursor == -1);
			if(first){
				cursor = 0;
			}else if(cursor == 0){
				return false;
			}

			redis_reply ret = pmaster->c->execute("lrange %s %lld %lld",hiredis_key.c_str(),cursor,cursor + 20);
			if(!ret.good()) return false;
			if(ret.type() != redis_reply::t_array) return false;

			for(unsigned long long i=0;i<ret.size();++i){
				lyramilk::data::string v = ret.at(i).str();
				dq.push(v);
			}

			if(dq.size() < 20){
				cursor = 0;
			}else{
				cursor += dq.size();
			}
			if(dq.size() == 0) return false;
			if(first){
				list_value = dq.front();
				status = true;
				dq.pop();
			}
			return true;
		}

		lyramilk::data::var ok(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			if(cursor == -1){
				flush();
			}
			return status;
		}

		lyramilk::data::var get(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			return list_value;
		}

		lyramilk::data::var value(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			return list_value;
		}

		lyramilk::data::var next(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			status = false;
			if(dq.size() == 0 && !flush()) return false;

			list_value = dq.front();
			status = true;
			dq.pop();
			return true;
		}

		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["ok"] = lyramilk::script::engine::functional<hiredis_list_iterator,&hiredis_list_iterator::ok>;
			fn["get"] = lyramilk::script::engine::functional<hiredis_list_iterator,&hiredis_list_iterator::get>;
			fn["value"] = lyramilk::script::engine::functional<hiredis_list_iterator,&hiredis_list_iterator::get>;
			fn["next"] = lyramilk::script::engine::functional<hiredis_list_iterator,&hiredis_list_iterator::next>;
			p->define("hiredis.list.iterator",fn,hiredis_list_iterator::ctr,hiredis_list_iterator::dtr);
			return 1;
		}
	};

	class hiredis_list
	{
		lyramilk::log::logss log;
		hiredis* pmaster;
		lyramilk::data::string key;
	  public:
		static void* ctr(lyramilk::data::var::array args)
		{
			assert(args.size() == 2);
			return new hiredis_list((hiredis*)args[0].userdata("hiredis"),args[1]);
		}
		static void dtr(void* p)
		{
			delete (hiredis_list*)p;
		}

		hiredis_list(hiredis* pc,lyramilk::data::string key):log(lyramilk::klog,"teapoy.native.hiredis.list")
		{
			pmaster = pc;
			this->key = key;
		}

		~hiredis_list()
		{
		}

		lyramilk::data::var getkey(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			return key;
		}

		lyramilk::data::var push_back(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			if(pmaster->isreadonly) throw lyramilk::exception(D("hiredis.list.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);

			lyramilk::data::string field = args[0];

			redis_reply ret = pmaster->c->execute("rpush %s %s",key.c_str(),field.c_str());
			if(!ret.good()) return 0;
			if(ret.type() != redis_reply::t_string && ret.type() != redis_reply::t_integer) return 0;

			return ret.toint();
		}

		lyramilk::data::var push_front(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			if(pmaster->isreadonly) throw lyramilk::exception(D("hiredis.list.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);

			lyramilk::data::string field = args[0];

			redis_reply ret = pmaster->c->execute("lpush %s %s",key.c_str(),field.c_str());
			if(!ret.good()) return 0;
			if(ret.type() != redis_reply::t_string && ret.type() != redis_reply::t_integer) return 0;

			return ret.toint();
		}

		lyramilk::data::var pop_back(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			if(pmaster->isreadonly) throw lyramilk::exception(D("hiredis.list.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			redis_reply ret = pmaster->c->execute("rpop %s",key.c_str());
			if(!ret.good()) return 0;
			if(ret.type() != redis_reply::t_string && ret.type() != redis_reply::t_integer) return 0;

			return ret.str().c_str();
		}

		lyramilk::data::var pop_front(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			if(pmaster->isreadonly) throw lyramilk::exception(D("hiredis.list.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			redis_reply ret = pmaster->c->execute("lpop %s",key.c_str());
			if(!ret.good()) return 0;
			if(ret.type() != redis_reply::t_string && ret.type() != redis_reply::t_integer) return 0;

			return ret.str().c_str();
		}

		lyramilk::data::var front(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			redis_reply ret = pmaster->c->execute("lindex %s 0",key.c_str());
			if(!ret.good()) return 0;
			if(ret.type() != redis_reply::t_string && ret.type() != redis_reply::t_integer) return 0;

			return ret.str().c_str();
		}

		lyramilk::data::var back(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			redis_reply ret = pmaster->c->execute("lindex %s -1",key.c_str());
			if(!ret.good()) return 0;
			if(ret.type() != redis_reply::t_string && ret.type() != redis_reply::t_integer) return 0;

			return ret.str().c_str();
		}

		lyramilk::data::var get(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_int64);
			long long index = args[0];
			redis_reply ret = pmaster->c->execute("lindex %s %lld",key.c_str(),index);
			if(!ret.good()) return 0;
			if(ret.type() != redis_reply::t_string && ret.type() != redis_reply::t_integer) return 0;

			return ret.str().c_str();
		}

		lyramilk::data::var size(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			redis_reply ret = pmaster->c->execute("llen %s",key.c_str());
			if(!ret.good()) return false;
			if(ret.type() != redis_reply::t_integer) throw lyramilk::exception(D("数据库错误"));

			return ret.toint();
		}

		lyramilk::data::var scan(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			lyramilk::script::engine* e = (lyramilk::script::engine*)env[lyramilk::script::engine::s_env_engine()].userdata(lyramilk::script::engine::s_user_engineptr());
			
			lyramilk::data::var::array ar;
			ar.push_back(lyramilk::data::var("hiredis",pmaster));
			ar.push_back(key);
			return e->createobject("hiredis.list.iterator",ar);
		}

		lyramilk::data::var clear(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			if(pmaster->isreadonly) throw lyramilk::exception(D("hiredis.list.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			if(pmaster->c->is_ssdb()){
				lyramilk::data::var::array ar;
				ar.push_back("qclear");
				ar.push_back(key);
				lyramilk::data::var::array ret = pmaster->c->ssdb_exec(ar);

				if(ret.size() != 2) throw lyramilk::exception(D("数据库错误"));
				int cnt = ret[1];
				return cnt > 0;
			}
			redis_reply ret = pmaster->c->execute("del %s",key.c_str());
			if(!ret.good()) return lyramilk::data::var::nil;
			if(ret.type() != redis_reply::t_integer) throw lyramilk::exception(D("数据库错误"));
			return ret.toint() == 1;
		}

		lyramilk::data::var random(lyramilk::data::var::array args,lyramilk::data::var::map env)
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
				e = (lyramilk::script::engine*)env[lyramilk::script::engine::s_env_engine()].userdata(lyramilk::script::engine::s_user_engineptr());
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

		lyramilk::data::var rename(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string str = args[0];

			redis_reply ret = pmaster->c->execute("rename %s %s",key.c_str(),str.c_str());
			if(!ret.good()) return lyramilk::data::var::nil;
			key = str;
			return lyramilk::data::var::nil;
		}

		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["getkey"] = lyramilk::script::engine::functional<hiredis_list,&hiredis_list::getkey>;
			fn["push_back"] = lyramilk::script::engine::functional<hiredis_list,&hiredis_list::push_back>;
			fn["push_front"] = lyramilk::script::engine::functional<hiredis_list,&hiredis_list::push_front>;
			fn["pop_back"] = lyramilk::script::engine::functional<hiredis_list,&hiredis_list::pop_back>;
			fn["pop_front"] = lyramilk::script::engine::functional<hiredis_list,&hiredis_list::pop_front>;
			fn["front"] = lyramilk::script::engine::functional<hiredis_list,&hiredis_list::front>;
			fn["back"] = lyramilk::script::engine::functional<hiredis_list,&hiredis_list::back>;
			fn["get"] = lyramilk::script::engine::functional<hiredis_list,&hiredis_list::get>;
			fn["size"] = lyramilk::script::engine::functional<hiredis_list,&hiredis_list::size>;
			fn["scan"] = lyramilk::script::engine::functional<hiredis_list,&hiredis_list::scan>;
			fn["clear"] = lyramilk::script::engine::functional<hiredis_list,&hiredis_list::clear>;
			fn["random"] = lyramilk::script::engine::functional<hiredis_list,&hiredis_list::random>;
			fn["rename"] = lyramilk::script::engine::functional<hiredis_list,&hiredis_list::rename>;
			p->define("hiredis.list",fn,hiredis_list::ctr,hiredis_list::dtr);
			return 1;
		}
	};

	class hiredis_kv
	{
		lyramilk::log::logss log;
		hiredis* pmaster;
		lyramilk::data::string key;
	  public:
		static void* ctr(lyramilk::data::var::array args)
		{
			assert(args.size() == 2);
			return new hiredis_kv((hiredis*)args[0].userdata("hiredis"),args[1]);
		}
		static void dtr(void* p)
		{
			delete (hiredis_kv*)p;
		}

		hiredis_kv(hiredis* pc,lyramilk::data::string key):log(lyramilk::klog,"teapoy.native.hiredis.kv")
		{
			pmaster = pc;
			this->key = key;
		}

		~hiredis_kv()
		{
		}

		lyramilk::data::var getkey(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			return key;
		}

		lyramilk::data::var get(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			redis_reply ret = pmaster->c->execute("get %s",key.c_str());
			if(!ret.good()) return lyramilk::data::var::nil;
			if(ret.type() != redis_reply::t_string) return lyramilk::data::var::nil;
			return ret.str();
		}

		lyramilk::data::var set(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			if(pmaster->isreadonly) throw lyramilk::exception(D("hiredis.kv.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string value = args[0];

			redis_reply ret = pmaster->c->execute("set %s %s",key.c_str(),value.c_str());
			if(!ret.good()) return lyramilk::data::var::nil;
			return ret.type() != redis_reply::t_status;
		}

		lyramilk::data::var incr(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			if(pmaster->isreadonly) throw lyramilk::exception(D("hiredis.kv.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			redis_reply ret = pmaster->c->execute("incr %s",key.c_str());
			if(!ret.good()) return lyramilk::data::var::nil;
			return ret.toint();
		}

		lyramilk::data::var decr(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			if(pmaster->isreadonly) throw lyramilk::exception(D("hiredis.kv.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			redis_reply ret = pmaster->c->execute("decr %s",key.c_str());
			if(!ret.good()) return lyramilk::data::var::nil;
			return ret.toint();
		}

		lyramilk::data::var clear(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			if(pmaster->isreadonly) throw lyramilk::exception(D("hiredis.kv.%s：禁止向只读Redis实例写入数据",__FUNCTION__));
			if(pmaster->c->is_ssdb()){
				lyramilk::data::var::array ar;
				ar.push_back("del");
				ar.push_back(key);
				lyramilk::data::var::array ret = pmaster->c->ssdb_exec(ar);

				if(ret.size() != 2) throw lyramilk::exception(D("数据库错误"));
				int cnt = ret[1];
				return cnt > 0;
			}

			redis_reply ret = pmaster->c->execute("del %s",key.c_str());
			if(!ret.good()) return lyramilk::data::var::nil;
			if(ret.type() != redis_reply::t_integer) throw lyramilk::exception(D("数据库错误"));
			return ret.toint() == 1;
		}

		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["getkey"] = lyramilk::script::engine::functional<hiredis_kv,&hiredis_kv::getkey>;
			fn["get"] = lyramilk::script::engine::functional<hiredis_kv,&hiredis_kv::get>;
			fn["set"] = lyramilk::script::engine::functional<hiredis_kv,&hiredis_kv::set>;
			fn["incr"] = lyramilk::script::engine::functional<hiredis_kv,&hiredis_kv::incr>;
			fn["decr"] = lyramilk::script::engine::functional<hiredis_kv,&hiredis_kv::decr>;
			fn["clear"] = lyramilk::script::engine::functional<hiredis_kv,&hiredis_kv::clear>;
			p->define("hiredis.kv",fn,hiredis_kv::ctr,hiredis_kv::dtr);
			return 1;
		}
	};


	static int define(lyramilk::script::engine* p)
	{
		int i = 0;
		i+= hiredis::define(p);
		i+= hiredis_zset::define(p);
		i+= hiredis_zset_iterator::define(p);
		i+= hiredis_hmap::define(p);
		i+= hiredis_hmap_iterator::define(p);
		i+= hiredis_list::define(p);
		i+= hiredis_list_iterator::define(p);
		i+= hiredis_kv::define(p);
		return i;
	}

	static __attribute__ ((constructor)) void __init()
	{
		lyramilk::teapoy::script2native::instance()->regist("hiredis",define);
	}
}}}
