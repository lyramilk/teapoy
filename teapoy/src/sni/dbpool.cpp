#include <libmilk/log.h>
#include <libmilk/multilanguage.h>
#include <libmilk/thread.h>
#include <sys/poll.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include "dbconnpool.h"
#include "script.h"

#include <unistd.h>
#include <netdb.h>

#include <errno.h>
#include <string.h>

namespace lyramilk{ namespace teapoy{ namespace native
{
	static lyramilk::log::logss log(lyramilk::klog,"lyramilk.teapoy.native.dbpool");

	lyramilk::data::var GetRedis(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
		redis_clients::ptr p = redis_clients_multiton::instance()->getobj(args[0].str());
		lyramilk::script::engine* e = (lyramilk::script::engine*)env.find(lyramilk::script::engine::s_env_engine())->second.userdata(lyramilk::script::engine::s_user_engineptr());
		lyramilk::data::var::array ar(2);
		ar[0].assign("__redis_client",&p);
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

	lyramilk::data::var GetMysql(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);

		mysql_clients::ptr p = mysql_clients_multiton::instance()->getobj(args[0].str());
		lyramilk::script::engine* e = (lyramilk::script::engine*)env.find(lyramilk::script::engine::s_env_engine())->second.userdata(lyramilk::script::engine::s_user_engineptr());
		lyramilk::data::var::array ar;
		lyramilk::data::var ariv("__mysql_client",&p);
		ar.push_back(ariv);
		return e->createobject("Mysql",ar);
	}


	lyramilk::data::var DefineDataBase(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_str);
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,2,lyramilk::data::var::t_map);

		lyramilk::data::string type = args[0].str();
		if(type == "redis"){
			redis_clients_multiton::instance()->add_config(args[1].str(),args[2]);
			return true;
		}else if(type == "mysql"){
			mysql_clients_multiton::instance()->add_config(args[1].str(),args[2]);
			return true;
		}
		return false;
	}


	static int define(lyramilk::script::engine* p)
	{
		p->define("GetRedis",GetRedis);
		p->define("GetMysql",GetMysql);
		p->define("DefineDataBase",DefineDataBase);
		return 3;
	}

	static __attribute__ ((constructor)) void __init()
	{
		lyramilk::teapoy::script2native::instance()->regist("dbpool",define);
	}

}}}