#include "script.h"
#include "env.h"
#include "functional_impl_cavedb.h"

#include <libmilk/log.h>
#include <libmilk/dict.h>
#include <libmilk/factory.h>
#include <mysql/mysql.h>
#define MAROC_MYSQL MYSQL
#include <cassert>
#ifndef my_bool
	#define my_bool bool
#endif

namespace lyramilk{ namespace teapoy{ namespace native{
	static lyramilk::log::logss log(lyramilk::klog,"teapoy.native.cavedb");

	class cavedb_leveldb_reader:public lyramilk::script::sclass
	{
		lyramilk::log::logss log;
		functional_impl_cavedb_instance* p;
		functional_impl_cavedb_instance::ptr pp;
	  public:
		static lyramilk::script::sclass* ctr(const lyramilk::data::array& args)
		{
			lyramilk::teapoy::functional_multi* f = functional_master::instance()->get(args[0].str());
			if(f){
				functional::ptr p = f->get_instance();
				if(p){
					cavedb_leveldb_reader* ins = new cavedb_leveldb_reader();
					if(ins){
						ins->pp = p;
						ins->p = p.as<functional_impl_cavedb_instance>();
						return ins;
					}
				}
			}
			return nullptr;
		}
		static void dtr(lyramilk::script::sclass* p)
		{
			delete (cavedb_leveldb_reader*)p;
		}

		cavedb_leveldb_reader():log(lyramilk::klog,"teapoy.native.cavedb")
		{
			pp = nullptr;
		}

		virtual ~cavedb_leveldb_reader()
		{
		}

		lyramilk::data::var hget(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_str);
			if(!p) return lyramilk::data::var::nil;
			return p->hget(args[0].str(),args[1].str());
		}

		lyramilk::data::var hexist(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_str);
			if(!p) return lyramilk::data::var::nil;
			return p->hexist(args[0].str(),args[1].str());
		}

		lyramilk::data::var hgetall(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			if(!p) return lyramilk::data::var::nil;
			return p->hgetallv(args[0].str());
		}

		static int define(lyramilk::script::engine* p)
		{
			{
				lyramilk::script::engine::functional_map fn;
				fn["hget"] = lyramilk::script::engine::functional<cavedb_leveldb_reader,&cavedb_leveldb_reader::hget>;
				fn["hexist"] = lyramilk::script::engine::functional<cavedb_leveldb_reader,&cavedb_leveldb_reader::hexist>;
				fn["hgetall"] = lyramilk::script::engine::functional<cavedb_leveldb_reader,&cavedb_leveldb_reader::hgetall>;
				p->define("..invoker.instance.cavedb",fn,cavedb_leveldb_reader::ctr,cavedb_leveldb_reader::dtr);
			}
			return 2;
		}
	};

	static int define(lyramilk::script::engine* p)
	{
		int i = 0;
		i+= cavedb_leveldb_reader::define(p);
		return i;
	}


	static __attribute__ ((constructor)) void __init()
	{
		invoker_map::instance()->define("cavedb","..invoker.instance.cavedb");
		lyramilk::teapoy::script_interface_master::instance()->regist("cavedb",define);
	}
}}}
