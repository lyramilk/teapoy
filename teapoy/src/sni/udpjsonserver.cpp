#include <libmilk/scriptengine.h>
#include <libmilk/log.h>
#include <libmilk/dict.h>
#include <libmilk/netaio.h>
#include <libmilk/aio.h>
#include <libmilk/json.h>
#include "script.h"
#include "webservice.h"
#include "sni_selector.h"
#include "cvmap_dispatcher.h"

namespace lyramilk{ namespace teapoy{ namespace native
{
	lyramilk::log::logss static log(lyramilk::klog,"teapoy.native.UDPJsonServer");

	class udpjsonserver_impl:public lyramilk::netio::udplistener
	{
	  public:
		cvmap_dispatcher* dispatcher;
		lyramilk::data::string scripttype;
		lyramilk::data::string scriptpath;
		udpjsonserver_impl()
		{
		}

		virtual ~udpjsonserver_impl()
		{}



		bool dorequest(const char* cache, int size, lyramilk::data::ostream& os,const struct sockaddr* addr,socklen_t addr_len)
		{
			if(dispatcher == nullptr) return false;

			lyramilk::data::var vreq;
			if(lyramilk::data::json::parse(lyramilk::data::string(cache,size),&vreq)){

				if(vreq.type() != lyramilk::data::var::t_map){
					log(lyramilk::log::warning,__FUNCTION__) << D("运行[%s]类型文件%s失败:%s",scripttype.c_str(),scriptpath.c_str(),D("json内容类型错误").c_str()) << std::endl;
					return false;
				}

				lyramilk::data::map& req = vreq;
				lyramilk::data::map rep;

				lyramilk::data::string c = req["c"];
				lyramilk::data::int64 v = req["v"];


				lyramilk::netio::netaddress srcaddr;
				if(addr_len == sizeof(sockaddr_in)){
					const struct sockaddr_in* inaddr = (const struct sockaddr_in*)addr;
					srcaddr = lyramilk::netio::netaddress(inaddr->sin_addr.s_addr,ntohs(inaddr->sin_port));
				}

				if(dispatcher->call(srcaddr,c,v,req,&rep)){
					lyramilk::data::string str;
					if(lyramilk::data::json::stringify(rep,&str)){
						os << str;
						return true;
					}
					return false;
				}
				log(lyramilk::log::warning,__FUNCTION__) << D("运行[%s]类型文件%s失败",scripttype.c_str(),scriptpath.c_str()) << std::endl;
				return false;
			}
			log(lyramilk::log::warning,__FUNCTION__) << D("运行[%s]类型文件%s失败:%s",scripttype.c_str(),scriptpath.c_str(),D("json解析错误").c_str()) << std::endl;
			return false;
		}


		bool onrequest(const char* cache, int size, lyramilk::data::ostream& os,const struct sockaddr* addr,socklen_t addr_len)
		{
			return dorequest(cache,size,os,addr,addr_len);
		}
	};

	class udpjsonserver:public epoll_selector
	{
	  public:
		cvmap_dispatcher* dispatcher;

		udpjsonserver_impl* udpsrv;
		static lyramilk::script::sclass* ctr(const lyramilk::data::array& args)
		{
			udpjsonserver* p = new udpjsonserver();
			return p;
		}

		static void dtr(lyramilk::script::sclass* p)
		{
			delete (udpjsonserver*)p;
		}

		udpjsonserver()
		{
			udpsrv = new udpjsonserver_impl;
			dispatcher = new cvmap_dispatcher;
			udpsrv->dispatcher = dispatcher;
			selector = udpsrv;
		}

		virtual ~udpjsonserver()
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
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_array);

			const lyramilk::data::array& ar = args[0];
			{
				lyramilk::data::array::const_iterator it = ar.begin();
				for(;it!=ar.end();++it){
					if(it->type() != lyramilk::data::var::t_map) continue;
					const lyramilk::data::map& m = *it;

					lyramilk::data::map::const_iterator type_it = m.find("type");
					if(type_it == m.end()) continue;

					lyramilk::data::map::const_iterator module_it = m.find("module");
					if(module_it == m.end()) continue;

					cvmap_script_selector* s = new cvmap_script_selector;
					if(s){
						if(s->init(type_it->second.str(),module_it->second.str())){
							if(dispatcher->add(s)){
								log(lyramilk::log::debug,__FUNCTION__) << D("定义cv映射成功：类型%s 模式%s",type_it->second.str().c_str(),module_it->second.str().c_str()) << std::endl;
							}else{
								delete s;
							}
						}else{
							delete s;
						}
					}
				}
			}

			return true;
		}

		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["open"] = lyramilk::script::engine::functional<udpjsonserver,&udpjsonserver::open>;
			fn["set_action"] = lyramilk::script::engine::functional<udpjsonserver,&udpjsonserver::set_action>;
			p->define("UDPJsonServer",fn,udpjsonserver::ctr,udpjsonserver::dtr);
			return 1;
		}

		static __attribute__ ((constructor)) void __init()
		{
			lyramilk::teapoy::script_interface_master::instance()->regist("udp",define);
		}
	};

}}}
