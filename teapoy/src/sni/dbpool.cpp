#include <libmilk/log.h>
#include <libmilk/dict.h>
#include <libmilk/thread.h>
#include <sys/poll.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include "script.h"

#include <unistd.h>
#include <netdb.h>

#include <errno.h>
#include <string.h>

#pragma push_macro("D")
#undef D
#include "dbconnpool.h"
#pragma pop_macro("D")

namespace lyramilk{ namespace teapoy{ namespace native
{
	static lyramilk::log::logss log(lyramilk::klog,"lyramilk.teapoy.native.dbpool");

	inline lyramilk::script::engine* get_script_engine_by_envmap(const lyramilk::data::map& env)
	{
	
		lyramilk::data::map::const_iterator it_env_eng = env.find(lyramilk::script::engine::s_env_engine());
		if(it_env_eng != env.end()){
			lyramilk::data::datawrapper* urd = it_env_eng->second.userdata();
			if(urd && urd->name() == lyramilk::script::engine_datawrapper::class_name()){
				lyramilk::script::engine_datawrapper* urdp = (lyramilk::script::engine_datawrapper*)urd;
				return urdp->eng;
			}
		}
		return nullptr;
	}

	lyramilk::data::var GetRedis(const lyramilk::data::array& args,const lyramilk::data::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
		redis_clients::ptr p = redis_clients_multiton::instance()->getobj(args[0].str());
		if(p == nullptr) return lyramilk::data::var::nil;
		lyramilk::script::engine* e = get_script_engine_by_envmap(env);
		lyramilk::data::array ar(2);
		ar[0].assign(dbpool_pointer_datawrapper(&p));
		ar[1] = false;

		lyramilk::data::var cfg_rdonly = redis_clients_multiton::instance()->get_config(args[0].str());
		if(cfg_rdonly.type() == lyramilk::data::var::t_map){
			const lyramilk::data::var& co = cfg_rdonly["readonly"];
			if(co.type_like(lyramilk::data::var::t_bool)){
				ar[1] = (bool)co;
			}
		}
		return e->createobject("Redis",ar);
	}

	lyramilk::data::var GetMysql(const lyramilk::data::array& args,const lyramilk::data::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);

		mysql_clients::ptr p = mysql_clients_multiton::instance()->getobj(args[0].str());
		if(p == nullptr) return lyramilk::data::var::nil;

		lyramilk::script::engine* e = get_script_engine_by_envmap(env);
		lyramilk::data::array ar;
		ar.push_back(dbpool_pointer_datawrapper(&p));
		return e->createobject("Mysql",ar);
	}

#if (defined Z_HAVE_LIBMONGO) || (defined Z_HAVE_MONGODB)
	lyramilk::data::var GetMongo(const lyramilk::data::array& args,const lyramilk::data::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);

		mongo_clients::ptr p = mongo_clients_multiton::instance()->getobj(args[0].str());
		if(p == nullptr) return lyramilk::data::var::nil;

		lyramilk::script::engine* e = get_script_engine_by_envmap(env);
		lyramilk::data::array ar;
		ar.push_back(dbpool_pointer_datawrapper(&p));
		return e->createobject("Mongo",ar);
	}
#endif
	lyramilk::data::var GetLoger(const lyramilk::data::array& args,const lyramilk::data::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);

		filelogers* p = filelogers_multiton::instance()->getobj(args[0].str());
		if(p == nullptr) return lyramilk::data::var::nil;

		lyramilk::script::engine* e = get_script_engine_by_envmap(env);
		lyramilk::data::array ar;
		ar.push_back(dbpool_pointer_datawrapper(p));
		return e->createobject("Logfile",ar);
	}

#ifdef CAVEDB_FOUND
	lyramilk::data::var GetCaveDB(const lyramilk::data::array& args,const lyramilk::data::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);

		lyramilk::cave::leveldb_minimal_adapter* p = cavedb_leveldb_minimal_multiton::instance()->getobj(args[0].str());
		if(p == nullptr) return lyramilk::data::var::nil;

		lyramilk::script::engine* e = get_script_engine_by_envmap(env);
		lyramilk::data::array ar;
		ar.push_back(dbpool_pointer_datawrapper(&p));
		return e->createobject("CaveDB",ar);
	}
#endif

	lyramilk::data::var DefineDataBase(const lyramilk::data::array& args,const lyramilk::data::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_str);
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,2,lyramilk::data::var::t_map);

		lyramilk::data::string type = args[0].str();
		lyramilk::klog(lyramilk::log::debug,"teapoy.native.DefineDataBase") << D("定义%s数据库%s(%s)",type.c_str(),args[1].str().c_str(),args[2].str().c_str()) << std::endl;
		if(type == "redis"){
			redis_clients_multiton::instance()->add_config(args[1].str(),args[2]);
			return true;
		}else if(type == "mysql"){
			mysql_clients_multiton::instance()->add_config(args[1].str(),args[2]);
			return true;
#if (defined Z_HAVE_LIBMONGO) || (defined Z_HAVE_MONGODB)
		}else if(type == "mongo"){
			mongo_clients_multiton::instance()->add_config(args[1].str(),args[2]);
			return true;
#endif
#ifdef CAVEDB_FOUND
		}else if(type == "cavedb"){
			cavedb_leveldb_minimal_multiton::instance()->add_config(args[1].str(),args[2]);
			return true;
#endif
		}else if(type == "loger"){
			filelogers_multiton::instance()->add_config(args[1].str(),args[2]);
			return true;
		}
		return false;
	}


	static int define(lyramilk::script::engine* p)
	{
		p->define("GetRedis",GetRedis);
		p->define("GetMysql",GetMysql);
#if (defined Z_HAVE_LIBMONGO) || (defined Z_HAVE_MONGODB)
		p->define("GetMongo",GetMongo);
#endif
		p->define("GetLoger",GetLoger);
#ifdef CAVEDB_FOUND
		p->define("GetCaveDB",GetCaveDB);
#endif
		p->define("DefineDataBase",DefineDataBase);
		return 3;
	}

	static __attribute__ ((constructor)) void __init()
	{
		lyramilk::teapoy::script2native::instance()->regist("dbpool",define);
	}

}}}