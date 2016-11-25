#include <libmilk/scriptengine.h>
#include <libmilk/log.h>
#include <libmilk/multilanguage.h>
#include "script.h"

namespace lyramilk{ namespace teapoy{ namespace native
{
	static lyramilk::log::logss log(lyramilk::klog,"teapoy.native");

	lyramilk::data::var test(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
	{
		return true;
	}

	lyramilk::data::var echo(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
	{
		for(lyramilk::data::var::array::const_iterator it = args.begin();it!=args.end();++it){
			std::cout << it->str();
		}
		std::cout << std::endl;
		return true;
	}

	lyramilk::data::var trace(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
	{
		lyramilk::script::engine* e = (lyramilk::script::engine*)env.find(lyramilk::script::engine::s_env_engine())->second.userdata(lyramilk::script::engine::s_user_engineptr());
		lyramilk::data::string mod = "s=" + e->filename();

		lyramilk::klog(lyramilk::log::trace,mod);
		for(lyramilk::data::var::array::const_iterator it = args.begin();it!=args.end();++it){
			lyramilk::klog << it->str();
		}
		lyramilk::klog << std::endl;
		return true;
	}

	lyramilk::data::var slog(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
		lyramilk::script::engine* e = (lyramilk::script::engine*)env.find(lyramilk::script::engine::s_env_engine())->second.userdata(lyramilk::script::engine::s_user_engineptr());
		lyramilk::data::string mod = "s=" + e->filename();
		lyramilk::data::string logtype = args[0];

		if(logtype == "debug"){
			lyramilk::klog(lyramilk::log::debug,mod);
		}else if(logtype == "trace"){
			lyramilk::klog(lyramilk::log::trace,mod);
		}else if(logtype == "warning"){
			lyramilk::klog(lyramilk::log::warning,mod);
		}else if(logtype == "error"){
			lyramilk::klog(lyramilk::log::error,mod);
		}else{
			throw lyramilk::exception(D("错误的日志类型"));
		}


		lyramilk::data::var::array::const_iterator it = args.begin();
		if(it!=args.end()){
			++it;
			for(;it!=args.end();++it){
				lyramilk::klog << it->str();
			}
		}
		lyramilk::klog << std::endl;
		return true;
	}

	static int define(bool permanent,lyramilk::script::engine* p)
	{
		int i = 0;
		{
			p->define(permanent,"test",test);++i;
			p->define(permanent,"trace",trace);++i;
			p->define(permanent,"echo",echo);++i;
			p->define(permanent,"log",slog);++i;
		}
		return i;
	}

	static __attribute__ ((constructor)) void __init()
	{
		lyramilk::teapoy::script2native::instance()->regist("g.io",define);
	}
}}}