#include <libmilk/var.h>
#include "functional_master.h"
#include <libmilk/log.h>
#include <libmilk/dict.h>
#include "script.h"

namespace lyramilk{ namespace teapoy{ namespace native
{
	static lyramilk::log::logss log(lyramilk::klog,"teapoy.teapoy_functional");



	class teapoy_functional:public lyramilk::script::sclass
	{
		functional::ptr p;

	  public:
		static lyramilk::script::sclass* ctr(const lyramilk::data::array& args)
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
		static void dtr(lyramilk::script::sclass* p)
		{
			delete (teapoy_functional*)p;
		}

		teapoy_functional()
		{
		}

		virtual ~teapoy_functional()
		{
		}

		lyramilk::data::var exec(const lyramilk::data::array& args,const lyramilk::data::map& env)
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


	class teapoy_functional_invoker:public lyramilk::script::sclass
	{
	  public:
		static lyramilk::script::sclass* ctr(const lyramilk::data::array& args)
		{
			return new teapoy_functional_invoker();
		}
		static void dtr(lyramilk::script::sclass* p)
		{
			delete (teapoy_functional_invoker*)p;
		}

		teapoy_functional_invoker()
		{
		}

		virtual ~teapoy_functional_invoker()
		{
		}

		lyramilk::data::var add(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_str);
			lyramilk::data::string type = args[1];

			functional_multi* fun = functional_type_master::instance()->create(type,nullptr);
			if(fun == nullptr) return false;

			lyramilk::data::string key = args[0];

			if(args.size() > 2){
				MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,2,lyramilk::data::var::t_map);
				lyramilk::data::map info = args[2];
				fun->init(info);
			}

			functional_master::instance()->define(key,fun);
			return true;
		}

		lyramilk::data::var remove(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string key = args[0];
			TODO();
		}

		lyramilk::data::var get(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string key = args[0];

			lyramilk::teapoy::functional_multi* f = functional_master::instance()->get(key);
			lyramilk::data::string invoker_instance;
			if(f){
				invoker_instance = invoker_map::instance()->get(f->name());
			}
			if(invoker_instance.empty()){
				invoker_instance = "..invoker.instance";
			}

			lyramilk::data::map::const_iterator it_env_eng = env.find(lyramilk::script::engine::s_env_engine());
			if(it_env_eng != env.end()){
				lyramilk::data::datawrapper* urd = it_env_eng->second.userdata();
				if(urd && urd->name() == lyramilk::script::engine_datawrapper::class_name()){
					lyramilk::script::engine_datawrapper* urdp = (lyramilk::script::engine_datawrapper*)urd;
					if(urdp->eng){
						lyramilk::data::array ar;
						ar.push_back(key);
						return urdp->eng->createobject(invoker_instance,ar);
					}
				}
			}
			return lyramilk::data::var::nil;
		}

		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["add"] = lyramilk::script::engine::functional<teapoy_functional_invoker,&teapoy_functional_invoker::add>;
			//fn["remove"] = lyramilk::script::engine::functional<teapoy_functional_invoker,&teapoy_functional_invoker::remove>;
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