#include <libmilk/var.h>
#include <libmilk/log.h>
#include <libmilk/dict.h>
#include <libmilk/netaio.h>
#include <libmilk/scriptengine.h>
#include "script.h"
#include "sni_selector.h"

namespace lyramilk{ namespace teapoy{

	class server_poll:public lyramilk::script::sclass,public lyramilk::io::aiopoll
	{
		lyramilk::log::logss log;
	  public:

		static lyramilk::script::sclass* ctr(const lyramilk::data::array& args)
		{
			return new server_poll();
		}
		static void dtr(lyramilk::script::sclass* p)
		{
			delete (server_poll*)p;
		}

		server_poll():log(lyramilk::klog,"teapoy.epoll")
		{
		}

		virtual ~server_poll()
		{}

		lyramilk::data::var add(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_user);
			const lyramilk::data::datawrapper* urd = args[0].userdata();
			if(urd && urd->name() == lyramilk::script::objadapter_datawrapper::class_name()){
				const lyramilk::script::objadapter_datawrapper* pobj = (lyramilk::script::objadapter_datawrapper*)urd;
				if(!pobj) return false;
				lyramilk::teapoy::epoll_selector* psrv = (lyramilk::teapoy::epoll_selector*)pobj->_sclass;
				//return lyramilk::io::aiopoll::add(psrv->selector);
				return psrv->onadd(this);
			}
			return false;
		}

		lyramilk::data::var active(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			if(args.size() == 0){
				if(lyramilk::io::aiopoll::active(8)){
					log(lyramilk::log::debug,__FUNCTION__) << D("启动了%d个线程",capacity()) << std::endl;
				}else{
					log(lyramilk::log::error,__FUNCTION__) << D("启动了线程失败") << std::endl;
				}
			}else{
				MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_int);
				int i = args[0];
				if(lyramilk::io::aiopoll::active(i)){
					log(lyramilk::log::debug,__FUNCTION__) << D("启动了%d个线程",capacity()) << std::endl;
				}else{
					log(lyramilk::log::error,__FUNCTION__) << D("启动了线程失败") << std::endl;
				}
			}
			return true;
		}

		lyramilk::data::var wait(const lyramilk::data::array& args,const lyramilk::data::map& env)
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



	class server_poll_safe:public lyramilk::script::sclass,public lyramilk::io::aiopoll_safe
	{
		lyramilk::log::logss log;
	  public:

		static lyramilk::script::sclass* ctr(const lyramilk::data::array& args)
		{
			if(args.size() > 0 && args[0].type() == lyramilk::data::var::t_int){
				return new server_poll_safe(args[0]);
			}
			return new server_poll_safe(8);
		}
		static void dtr(lyramilk::script::sclass* p)
		{
			delete (server_poll_safe*)p;
		}

		server_poll_safe(int threadscount):lyramilk::io::aiopoll_safe(threadscount),log(lyramilk::klog,"teapoy.epoll_safe")
		{
		}

		virtual ~server_poll_safe()
		{}

		lyramilk::data::var add(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_user);
			const lyramilk::data::datawrapper* urd = args[0].userdata();
			if(urd && urd->name() == lyramilk::script::objadapter_datawrapper::class_name()){
				const lyramilk::script::objadapter_datawrapper* pobj = (lyramilk::script::objadapter_datawrapper*)urd;
				if(!pobj) return false;
				lyramilk::teapoy::epoll_selector* psrv = (lyramilk::teapoy::epoll_selector*)pobj->_sclass;
				//return lyramilk::io::aiopoll_safe::add(psrv->selector);
				return psrv->onadd(this);
			}
			return false;
		}

		lyramilk::data::var active(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			if(lyramilk::io::aiopoll_safe::active(8)){
				log(lyramilk::log::debug,__FUNCTION__) << D("启动了%d个线程",capacity()) << std::endl;
			}else{
				log(lyramilk::log::error,__FUNCTION__) << D("启动了线程失败") << std::endl;
			}
			return true;
		}

		lyramilk::data::var wait(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			lyramilk::io::aiopoll_safe::svc();
			return true;
		}

		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["add"] = lyramilk::script::engine::functional<server_poll_safe,&server_poll_safe::add>;
			fn["active"] = lyramilk::script::engine::functional<server_poll_safe,&server_poll_safe::active>;
			fn["wait"] = lyramilk::script::engine::functional<server_poll_safe,&server_poll_safe::wait>;
			p->define("epoll_safe",fn,server_poll_safe::ctr,server_poll_safe::dtr);
			return 1;
		}
	};




	static int define(lyramilk::script::engine* p)
	{
		int i = 0;
		i+= server_poll::define(p);
		i+= server_poll_safe::define(p);
		return i;
	}


	static __attribute__ ((constructor)) void __init()
	{
		lyramilk::teapoy::script_interface_master::instance()->regist("netio",define);
	}
}}
