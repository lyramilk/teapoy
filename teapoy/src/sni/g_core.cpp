#include <libmilk/scriptengine.h>
#include <libmilk/log.h>
#include <libmilk/multilanguage.h>
#include <libmilk/json.h>
#include "script.h"
#include "env.h"

#include <stdio.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/wait.h>

namespace lyramilk{ namespace teapoy{ namespace native
{
	static lyramilk::log::logss log(lyramilk::klog,"teapoy.g");

	lyramilk::data::var require(lyramilk::data::var::array args,lyramilk::data::var::map env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);

		lyramilk::data::string filename = env::get_config("js.require");
		if(!filename.empty() && filename.at(filename.size() - 1) != '/') filename.push_back('/');
		filename += args[0].str();
		lyramilk::script::engine* e = (lyramilk::script::engine*)env[lyramilk::script::engine::s_env_engine()].userdata(lyramilk::script::engine::s_user_engineptr());
/*
		lyramilk::data::var& v = e->get("require");
		v.type(lyramilk::data::var::t_array);
		lyramilk::data::var::array& ar = v;

		lyramilk::data::var::array::iterator it = ar.begin();
		for(;it!=ar.end();++it){
			lyramilk::data::string str = *it;
			if(str == args[0].str()){
				//log(lyramilk::log::warning,__FUNCTION__) << D("请不要重复包含文件%s",str.c_str()) << std::endl;
				return false;
			}
		}

		ar.push_back(args[0].str());*/
		if(e->load_file(filename)){
			return true;
		}
		return false;
	}
/*
	lyramilk::data::var daemon(lyramilk::data::var::array args,lyramilk::data::var::map env)
	{
		//::daemon(1,0);
		int pid = 0;
		do{
			pid = fork();
			if(pid == 0){
				break;
			}
		}while(waitpid(pid,NULL,0));
		return true;
	}*/

	lyramilk::data::var su(lyramilk::data::var::array args,lyramilk::data::var::map env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
		bool permanent = false;
		if(args.size() > 1){
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_bool);
			permanent = args[1];
		}

		lyramilk::data::string username = args[0];
		// useradd -s /sbin/nologin username
		struct passwd *pw = getpwnam(username.c_str());
		if(pw){
			if(permanent){
				if(seteuid(pw->pw_uid) == 0){
					log(__FUNCTION__) << D("切换到用户[%s]",username.c_str()) << std::endl;
					return true;
				}
			}else{
				if(setuid(pw->pw_uid) == 0){
					log(__FUNCTION__) << D("切换到用户[%s]",username.c_str()) << std::endl;
					return true;
				}
			}
		}
		log(lyramilk::log::warning,__FUNCTION__) << D("切换到用户[%s]失败",username.c_str()) << std::endl;
		return false;
	}

	lyramilk::data::var add_require_dir(lyramilk::data::var::array args,lyramilk::data::var::map env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
		TODO();
		return true;
	}

	lyramilk::data::var serialize(lyramilk::data::var::array args,lyramilk::data::var::map env)
	{
		if(args.size() < 1) throw lyramilk::exception(D("%s参数不足",__FUNCTION__));
		lyramilk::data::var v = args[0];
		lyramilk::data::stringstream ss;
		v.serialize(ss);

		lyramilk::data::string str = ss.str();
		lyramilk::data::chunk bstr((const unsigned char*)str.c_str(),str.size());
		return bstr;
	}

	lyramilk::data::var deserialize(lyramilk::data::var::array args,lyramilk::data::var::map env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_bin);
		lyramilk::data::string seq = args[0];
		lyramilk::data::var v;
		lyramilk::data::stringstream ss(seq);
		v.deserialize(ss);
		return v;
	}

	lyramilk::data::var json_stringify(lyramilk::data::var::array args,lyramilk::data::var::map env)
	{
		if(args.size() < 1)  throw lyramilk::exception(D("%s参数不足",__FUNCTION__));
		lyramilk::data::json j(args[0]);
		return j.str();
	}

	lyramilk::data::var json_parse(lyramilk::data::var::array args,lyramilk::data::var::map env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);

		lyramilk::data::var v;
		lyramilk::data::json j(v);
		j.str(args[0]);
		return v;
	}

	lyramilk::data::var system(lyramilk::data::var::array args,lyramilk::data::var::map env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
		lyramilk::data::string str = args[0];

		lyramilk::data::string ret;
		FILE *pp = popen(str.c_str(), "r");
		char tmp[1024];
		while (fgets(tmp, sizeof(tmp), pp) != NULL) {
			ret.append(tmp);
		}
		pclose(pp);
		return ret;
	}

	lyramilk::data::var env(lyramilk::data::var::array args,lyramilk::data::var::map env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);

		if(args.size() > 1){
			env::set_config(args[0],args[1]);
			return true;
		}
		return env::get_config(args[0]);
	}

	lyramilk::data::var set_config(lyramilk::data::var::array args,lyramilk::data::var::map env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_map);
		env::set_config("config",args[0]);
		return true;
	}

	lyramilk::data::var get_config(lyramilk::data::var::array args,lyramilk::data::var::map env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
		lyramilk::data::var cfg = env::get_config("config");
		lyramilk::data::string item = args[0].str();
		return cfg.path(item);
	}

	static int define(lyramilk::script::engine* p)
	{
		int i = 0;
		{
			p->define("require",require);++i;
			//p->define("daemon",daemon);++i;
			p->define("su",su);++i;
			p->define("add_require_dir",add_require_dir);++i;
			p->define("serialize",serialize);++i;
			p->define("deserialize",deserialize);++i;
			p->define("json_stringify",json_stringify);++i;
			p->define("json_parse",json_parse);++i;
			p->define("system",system);++i;
			p->define("env",env);++i;
			p->define("set_config",set_config);++i;
			p->define("config",get_config);++i;
		}
		return i;
	}

	static __attribute__ ((constructor)) void __init()
	{
		lyramilk::teapoy::script2native::instance()->regist("g.core",define);
	}
}}}