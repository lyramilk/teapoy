#include "script.h"
#include "env.h"
#include <libmilk/log.h>
#include <libmilk/dict.h>

namespace lyramilk{ namespace teapoy{ namespace native
{
	static lyramilk::log::logss log(lyramilk::klog,"teapoy.native.GlobalEnv");

	class globalenv:public lyramilk::script::sclass
	{
	  public:
		static lyramilk::script::sclass* ctr(const lyramilk::data::array& args)
		{
			return new globalenv;
		}
		static void dtr(lyramilk::script::sclass* p)
		{
			delete (globalenv*)p;
		}

		globalenv()
		{
		}

		virtual ~globalenv()
		{
		}

		lyramilk::data::var set(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			if(args.size() < 2) return false;
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string key = args[0].str();
			lyramilk::teapoy::env::set(key,args[1]);
			return true;
		}

		lyramilk::data::var get(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string key = args[0].str();

			return lyramilk::teapoy::env::get(key);
		}

		lyramilk::data::var remove(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string key = args[0].str();

			lyramilk::teapoy::env::set(key,lyramilk::data::var::nil);
			return true;
		}

		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["get"] = lyramilk::script::engine::functional<globalenv,&globalenv::get>;
			fn["set"] = lyramilk::script::engine::functional<globalenv,&globalenv::set>;
			fn["remove"] = lyramilk::script::engine::functional<globalenv,&globalenv::remove>;
			p->define("GlobalEnv",fn,globalenv::ctr,globalenv::dtr);
			return 1;
		}
	};

	static int define(lyramilk::script::engine* p)
	{
		int i = 0;
		i+= globalenv::define(p);
		return i;
	}


	static __attribute__ ((constructor)) void __init()
	{
		lyramilk::teapoy::script_interface_master::instance()->regist("env",define);
	}

}}}