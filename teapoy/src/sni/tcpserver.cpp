#include <libmilk/scriptengine.h>
#include <libmilk/log.h>
#include <libmilk/dict.h>
#include <libmilk/netaio.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include "script.h"
#include "webservice.h"
#include "httplistener.h"
#include "http_1_0.h"
#include "http_1_1.h"
#include "http_2_0_nghttp2.h"
#include "sessionmgr.h"
#include "sni_selector.h"
#include "mimetype.h"

namespace lyramilk{ namespace teapoy{ namespace native
{
	lyramilk::log::logss static log(lyramilk::klog,"teapoy.native.TCPServer");


	class tcpserver_session:public lyramilk::netio::aiosession_sync
	{
	  public:
		lyramilk::script::engines* es;
		lyramilk::data::string scripttype;
		lyramilk::data::string scriptpath;
		tcpserver_session()
		{}

		virtual ~tcpserver_session()
		{}

		bool onrequest(const char* cache, int size, lyramilk::data::ostream& os)
		{
			lyramilk::script::engines::ptr eng = es->get();
			if(eng == nullptr){
				log(lyramilk::log::warning,__FUNCTION__) << D("加载[%s]类型失败",scripttype.c_str()) << std::endl;
				return false;
			}
			if(!eng->load_file(scriptpath)){
				eng = nullptr;
				log(lyramilk::log::warning,__FUNCTION__) << D("加载[%s]类型文件%s失败",scripttype.c_str(),scriptpath.c_str()) << std::endl;
				return false;
			}

			lyramilk::data::array ar;
			ar.push_back(lyramilk::data::chunk((const unsigned char*)cache,size));
			lyramilk::data::var vret;
			if(eng->call("onrequest",ar,&vret)){
				if(vret.type_like(lyramilk::data::var::t_str)){
					os << vret.str();
					return true;
				}
				return true;
			}
			log(lyramilk::log::warning,__FUNCTION__) << D("运行[%s]类型文件%s失败",scripttype.c_str(),scriptpath.c_str()) << std::endl;
			return false;
		}
	};
	class tcpserver_impl:public lyramilk::netio::aioserver<tcpserver_session>
	{
	  public:
		lyramilk::script::engines* es;
		lyramilk::data::string scripttype;
		lyramilk::data::string scriptpath;

		virtual lyramilk::netio::aiosession* create()
		{
			tcpserver_session *p = lyramilk::netio::aiosession::__tbuilder<tcpserver_session>();
			p->es = es;
			p->scripttype = scripttype;
			p->scriptpath = scriptpath;
			return p;
		}
	};

	class tcpserver:public epoll_selector
	{
	  public:
		tcpserver_impl* tcpsrv;
		static lyramilk::script::sclass* ctr(const lyramilk::data::array& args)
		{
			tcpserver* p = new tcpserver();
			return p;
		}

		static void dtr(lyramilk::script::sclass* p)
		{
			delete (tcpserver*)p;
		}

		tcpserver()
		{
			tcpsrv = new tcpserver_impl;
			selector = tcpsrv;
		}

		virtual ~tcpserver()
		{
			delete tcpsrv;
		}

		lyramilk::data::var open(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_int);
			if(!tcpsrv) return false;
			return tcpsrv->open(args[0]);
		}

		lyramilk::data::var set_action(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_str);
			lyramilk::data::string scripttype = args[0].str();
			lyramilk::data::string scriptpath = args[1].str();

			lyramilk::script::engines* es = engine_pool::instance()->get(scripttype);
			if(!es){
				log(lyramilk::log::warning,__FUNCTION__) << D("加载[%s]类型失败",scripttype.c_str()) << std::endl;
				return false;
			}

			tcpsrv->es = es;
			tcpsrv->scripttype = scripttype;
			tcpsrv->scriptpath = scriptpath;
			return true;
		}

		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["open"] = lyramilk::script::engine::functional<tcpserver,&tcpserver::open>;
			fn["set_action"] = lyramilk::script::engine::functional<tcpserver,&tcpserver::set_action>;
			p->define("TCPServer",fn,tcpserver::ctr,tcpserver::dtr);
			return 1;
		}

		static __attribute__ ((constructor)) void __init()
		{
			lyramilk::teapoy::script_interface_master::instance()->regist("udp",define);
		}
	};

}}}
