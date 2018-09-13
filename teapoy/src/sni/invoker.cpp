#include <libmilk/var.h>
#include "functional_master.h"
#include <libmilk/log.h>
#include <libmilk/multilanguage.h>
#include "script.h"
/*
#include <libmilk/codes.h>
#include <libmilk/md5.h>

#include <stdlib.h>

#include "script.h"
#include "stringutil.h"
#include "webservice.h"

#include <sys/stat.h>
#include <errno.h>
#include <fstream>

#include "teapoy_gzip.h"
#include "httplistener.h"
#include "upstream_caller.h"
*/
namespace lyramilk{ namespace teapoy{ namespace native
{
	static lyramilk::log::logss log(lyramilk::klog,"teapoy.teapoy_functional");



	class teapoy_functional
	{
		functional::ptr p;

	  public:
		static void* ctr(const lyramilk::data::var::array& args)
		{
			lyramilk::teapoy::functional_multi* f = functional_master::instance()->get(args[0].str());
			if(f){
				functional::ptr p = f->get_instance();
				if(p){
					teapoy_functional* ins = new teapoy_functional;
					if(ins){
						ins->p = p;
						return ins;
					}
				}
			}
			return nullptr;
		}
		static void dtr(void* p)
		{
			delete (teapoy_functional*)p;
		}

		teapoy_functional()
		{
		}

		virtual ~teapoy_functional()
		{
		}

		lyramilk::data::var exec(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			return p->exec(args);
		}


		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["exec"] = lyramilk::script::engine::functional<teapoy_functional,&teapoy_functional::exec>;
			p->define("..invoker.instance",fn,teapoy_functional::ctr,teapoy_functional::dtr);
			return 1;
		}
	};


	class teapoy_functional_invoker
	{
	  public:
		static void* ctr(const lyramilk::data::var::array& args)
		{
			return new teapoy_functional_invoker();
		}
		static void dtr(void* p)
		{
			delete (teapoy_functional_invoker*)p;
		}

		teapoy_functional_invoker()
		{
		}

		virtual ~teapoy_functional_invoker()
		{
		}

		lyramilk::data::var add(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_str);
			lyramilk::data::string type = args[1];

			functional_multi* fun = functional_type_master::instance()->create(type,nullptr);
			if(fun == nullptr) return false;

			lyramilk::data::string key = args[0];

			if(args.size() > 2){
				MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,2,lyramilk::data::var::t_map);
				lyramilk::data::var::map info = args[2];
				fun->init(info);
			}

			functional_master::instance()->define(key,fun);
			return true;
		}

		lyramilk::data::var remove(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string key = args[0];
			TODO();
		}

		lyramilk::data::var get(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::script::engine* e = (lyramilk::script::engine*)env.find(lyramilk::script::engine::s_env_engine())->second.userdata(lyramilk::script::engine::s_env_engine());
			lyramilk::data::var::array ar;
			ar.reserve(1);
			ar.push_back(args[0].str());
			return e->createobject("..invoker.instance",ar);
		}

		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["add"] = lyramilk::script::engine::functional<teapoy_functional_invoker,&teapoy_functional_invoker::add>;
			fn["remove"] = lyramilk::script::engine::functional<teapoy_functional_invoker,&teapoy_functional_invoker::remove>;
			fn["get"] = lyramilk::script::engine::functional<teapoy_functional_invoker,&teapoy_functional_invoker::get>;
			p->define("invoker",fn,teapoy_functional_invoker::ctr,teapoy_functional_invoker::dtr);
			return 1;
		}
	};

	static int define(lyramilk::script::engine* p)
	{
		int i = 0;
		i+= teapoy_functional::define(p);
		i+= teapoy_functional_invoker::define(p);
		return i;
	}

	static __attribute__ ((constructor)) void __init()
	{
		lyramilk::teapoy::script_interface_master::instance()->regist("invoker",define);
	}

}}}