#include <libmilk/var.h>
#include <libmilk/log.h>
#include <libmilk/multilanguage.h>
#include <libmilk/netaio.h>
#include <libmilk/scriptengine.h>
#include "script.h"

namespace lyramilk{ namespace teapoy{

	class server_poll:public lyramilk::io::aiopoll
	{
		lyramilk::log::logss log;
	  public:

		static void* ctr(const lyramilk::data::var::array& args)
		{
			return new server_poll();
		}
		static void dtr(void* p)
		{
			delete (server_poll*)p;
		}

		server_poll():log(lyramilk::klog,"teapoy.server.epoll")
		{
		}

		~server_poll()
		{}

		lyramilk::data::var add(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_user);
			const void* pobj = args[0].userdata(lyramilk::script::engine::s_user_nativeobject());
			if(!pobj) return false;
			lyramilk::netio::aiolistener* psrv = (lyramilk::netio::aiolistener*)pobj;
			return lyramilk::io::aiopoll::add(psrv);
		}

		lyramilk::data::var active(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			if(args.size() == 0){
				lyramilk::io::aiopoll::active();
				log(lyramilk::log::debug,__FUNCTION__) << D("启动了%d个线程",size()) << std::endl;
			}else{
				MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_int);
				int i = args[0];
				lyramilk::io::aiopoll::active(i);
				log(lyramilk::log::debug,__FUNCTION__) << D("启动了%d个线程",i) << std::endl;
			}
			return true;
		}

		lyramilk::data::var wait(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			lyramilk::io::aiopoll::svc();
			return true;
		}

		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["add"] = lyramilk::script::engine::functional<server_poll,&server_poll::add>;
			fn["active"] = lyramilk::script::engine::functional<server_poll,&server_poll::active>;
			fn["wait"] = lyramilk::script::engine::functional<server_poll,&server_poll::wait>;
			p->define("epoll",fn,server_poll::ctr,server_poll::dtr);
			return 1;
		}
	};

	static int define(lyramilk::script::engine* p)
	{
		int i = 0;
		i+= server_poll::define(p);
		return i;
	}


	static __attribute__ ((constructor)) void __init()
	{
		lyramilk::teapoy::script2native::instance()->regist("netio",define);
	}
}}
