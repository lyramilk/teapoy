#include <libmilk/scriptengine.h>
#include <libmilk/log.h>
#include <libmilk/dict.h>
#include <libmilk/netaio.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include "script.h"
#include "dnsproxy.h"
#include "sni_selector.h"

namespace lyramilk{ namespace teapoy{ namespace native
{
	lyramilk::log::logss static log(lyramilk::klog,"teapoy.native.dnsserver");
	class dnsserver:public epoll_selector
	{
	  public:
		dnsproxy* udpsrv;
		static lyramilk::script::sclass* ctr(const lyramilk::data::array& args)
		{
			dnsserver* p = new dnsserver();
			return p;
		}

		static void dtr(lyramilk::script::sclass* p)
		{
			delete (dnsserver*)p;
		}

		dnsserver()
		{
			udpsrv = new lyramilk::teapoy::dnsproxy;
			selector = udpsrv;
		}

		virtual ~dnsserver()
		{
			delete udpsrv;
		}

		lyramilk::data::var open(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_int);

			if(!udpsrv) return false;
			return udpsrv->open(args[0]);
		}

		lyramilk::data::var bind(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_int);

			if(!udpsrv) return false;
			return udpsrv->bind_upstream_dns(args[0].str(),args[1]);
		}

		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["open"] = lyramilk::script::engine::functional<dnsserver,&dnsserver::open>;
			fn["bind"] = lyramilk::script::engine::functional<dnsserver,&dnsserver::bind>;
			p->define("DNSServer",fn,dnsserver::ctr,dnsserver::dtr);
			return 1;
		}

		static __attribute__ ((constructor)) void __init()
		{
			lyramilk::teapoy::script_interface_master::instance()->regist("udp",define);
		}
	};

}}}
