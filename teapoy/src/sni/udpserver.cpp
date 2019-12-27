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
	lyramilk::log::logss static log(lyramilk::klog,"teapoy.native.UDPServer");


	class udpserver_impl:public lyramilk::netio::udplistener
	{
	  public:
		lyramilk::script::engines* es;
		lyramilk::data::string scripttype;
		lyramilk::data::string scriptpath;
		udpserver_impl()
		{}

		virtual ~udpserver_impl()
		{}

		bool onrequest(const char* cache, int size, lyramilk::data::ostream& os,const struct sockaddr* addr,socklen_t addr_len)
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

	class udpserver:public epoll_selector
	{
	  public:
		udpserver_impl* udpsrv;
		static lyramilk::script::sclass* ctr(const lyramilk::data::array& args)
		{
			udpserver* p = new udpserver();
			return p;
		}

		static void dtr(lyramilk::script::sclass* p)
		{
			delete (udpserver*)p;
		}

		udpserver()
		{
			udpsrv = new udpserver_impl;
			selector = udpsrv;
		}

		virtual ~udpserver()
		{
			delete udpsrv;
		}

		lyramilk::data::var open(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_int);

			if(!udpsrv) return false;
			return udpsrv->open(args[0]);
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

			udpsrv->es = es;
			udpsrv->scripttype = scripttype;
			udpsrv->scriptpath = scriptpath;
			return true;
		}

		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["open"] = lyramilk::script::engine::functional<udpserver,&udpserver::open>;
			fn["set_action"] = lyramilk::script::engine::functional<udpserver,&udpserver::set_action>;
			p->define("UDPServer",fn,udpserver::ctr,udpserver::dtr);
			return 1;
		}

		static __attribute__ ((constructor)) void __init()
		{
			lyramilk::teapoy::script_interface_master::instance()->regist("udp",define);
		}
	};

}}}
