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
	lyramilk::log::logss static log(lyramilk::klog,"teapoy.native.HttpServer");
	class httpserver:public epoll_selector
	{
	  public:
		lyramilk::teapoy::httplistener* http;
		static lyramilk::script::sclass* ctr(const lyramilk::data::array& args)
		{
			httpserver* p = new httpserver();
			if(p){
				p->http->scheme = "http";
				p->http->define_http_protocols(http_1_0::proto_info.name,http_1_0::proto_info.ctr,http_1_0::proto_info.dtr);
				p->http->define_http_protocols(http_1_1::proto_info.name,http_1_1::proto_info.ctr,http_1_1::proto_info.dtr);
#ifdef NGHTTP2_FOUND
				p->http->define_http_protocols(http_2_0::proto_info.name,http_2_0::proto_info.ctr,http_2_0::proto_info.dtr);
#endif
			}
			return p;
		}

		static lyramilk::script::sclass* ctr2(const lyramilk::data::array& args)
		{
			httpserver* p = new httpserver();
			if(p){
				p->http->scheme = "https";
				p->http->define_http_protocols(http_1_0::proto_info.name,http_1_0::proto_info.ctr,http_1_0::proto_info.dtr);
				p->http->define_http_protocols(http_1_1::proto_info.name,http_1_1::proto_info.ctr,http_1_1::proto_info.dtr);
#ifdef NGHTTP2_FOUND
				p->http->define_http_protocols(http_2_0::proto_info.name,http_2_0::proto_info.ctr,http_2_0::proto_info.dtr);
#endif
			}
			return p;
		}
		static void dtr(lyramilk::script::sclass* p)
		{
			delete (httpserver*)p;
		}

		httpserver()
		{
			http = new lyramilk::teapoy::httplistener;
			http->session_mgr = lyramilk::teapoy::httpsession_manager_factory::instance()->create("default");
			selector = http;
		}

		virtual ~httpserver()
		{
			delete http;
		}

		lyramilk::data::var open(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			if(args.size() == 1){
				MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_int);
				if(!http) return false;
				return http->open(args[0]);
			}else if(args.size() == 2){
				MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
				MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_int);
				if(!http) return false;
				return http->open(args[0].str(),args[1]);
			}
			return false;
		}

		lyramilk::data::var open_unixsocket(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_int);

			if(!http) return false;
			return http->open_unixsocket(args[0]);
		}

		lyramilk::data::var set_website(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_map);
			lyramilk::data::map siteinfo = args[0];

			{
				const lyramilk::data::var& v = siteinfo["host"];
				if(!v.type_like(lyramilk::data::var::t_str)){
					lyramilk::data::string str = D("参数1.host 类型不兼容:%s，期待%s",v.type_name().c_str(),"t_str");
					log(lyramilk::log::warning,__FUNCTION__) << str << std::endl;
					throw lyramilk::exception(str);
				}
			}

			{
				const lyramilk::data::var& v = siteinfo["url_map"];
				if(!v.type_like(lyramilk::data::var::t_array)){
					lyramilk::data::string str = D("参数1.url_map 类型不兼容:%s，期待%s",v.type_name().c_str(),"t_array");
					log(lyramilk::log::warning,__FUNCTION__) << str << std::endl;
					throw lyramilk::exception(str);
				}
			}

			{
				const lyramilk::data::var& v = siteinfo["error_page"];
				if(!v.type_like(lyramilk::data::var::t_map)){
					lyramilk::data::string str = D("参数1.error_page 类型不兼容:%s，期待%s",v.type_name().c_str(),"t_map");
					log(lyramilk::log::warning,__FUNCTION__) << str << std::endl;
					throw lyramilk::exception(str);
				}
			}

			const lyramilk::data::string& host = siteinfo["host"];
			lyramilk::data::array& ar = siteinfo["url_map"];
			lyramilk::data::map& errpages = siteinfo["error_page"];

			{
				lyramilk::data::array::iterator it = ar.begin();
				for(;it!=ar.end();++it){
					if(it->type() != lyramilk::data::var::t_map) continue;
					lyramilk::data::map& m = *it;

					lyramilk::data::map::const_iterator type_it = m.find("type");
					if(type_it == m.end()) continue;
					url_regex_selector* s = (url_regex_selector*)url_selector_factory::instance()->create(type_it->second);
					if(s){
						if(s->init(m)){
							if(this->http->add_dispatcher_selector(host,s)){
								lyramilk::data::map::const_iterator pattern_it = m.find("pattern");
								lyramilk::data::map::const_iterator module_it = m.find("module");
								log(lyramilk::log::debug,__FUNCTION__) << D("定义url映射成功：类型%s 正则表达式%s 模式%s",type_it->second.str().c_str(),pattern_it->second.str().c_str(),module_it->second.str().c_str()) << std::endl;
							}
						}else{
							url_selector_factory::instance()->destory(type_it->second,s);
						}
					}
				}
			}

			{
				lyramilk::data::map::const_iterator it = errpages.begin();
				for(;it!=errpages.end();++it){
					///	TODO;
				}
			}

			return true;
		}



		lyramilk::data::var set_session_manager(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			http->session_mgr = lyramilk::teapoy::httpsession_manager_factory::instance()->create(args[0].str());
			if(http->session_mgr){
				if(args.size() > 1){
					MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_map);
					http->session_mgr->init(args[1]);
				}else{
					lyramilk::data::map m;
					http->session_mgr->init(m);
				}
			}
			return true;
		}

		lyramilk::data::var set_url_encrypt(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			//worker.set_urlhook(args[0].str());
			return true;
		}

		lyramilk::data::var set_ssl(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_str);
			http->init_ssl(args[0],args[1]);
			return true;
		}

		lyramilk::data::var set_ssl_verify_locations(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_array);
			return http->ssl_load_verify_locations(args[0]);
		}

		lyramilk::data::var set_ssl_client_verify(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_bool);
			return http->ssl_use_client_verify(args[0]);
		}

		static lyramilk::data::var DefineMimeType(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_str);
			lyramilk::data::string extname = args[0];
			lyramilk::data::string mimetype = args[1];
			mimetype::define_mimetype_fileextname(extname,mimetype);
			return true;
		}

		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["open"] = lyramilk::script::engine::functional<httpserver,&httpserver::open>;
			fn["open_unixsocket"] = lyramilk::script::engine::functional<httpserver,&httpserver::open_unixsocket>;
			fn["set_website"] = lyramilk::script::engine::functional<httpserver,&httpserver::set_website>;
			fn["set_session_manager"] = lyramilk::script::engine::functional<httpserver,&httpserver::set_session_manager>;
			fn["set_ssl"] = lyramilk::script::engine::functional<httpserver,&httpserver::set_ssl>;
			fn["set_url_encrypt"] = lyramilk::script::engine::functional<httpserver,&httpserver::set_url_encrypt>;
			fn["set_ssl_verify_locations"] = lyramilk::script::engine::functional<httpserver,&httpserver::set_ssl_verify_locations>;
			fn["set_ssl_client_verify"] = lyramilk::script::engine::functional<httpserver,&httpserver::set_ssl_client_verify>;
			p->define("HttpServer",fn,httpserver::ctr,httpserver::dtr);
			p->define("HttpsServer",fn,httpserver::ctr2,httpserver::dtr);
			//p->define("DefineMimeType",DefineMimeType);
			return 1;
		}

		static __attribute__ ((constructor)) void __init()
		{
			lyramilk::teapoy::script_interface_master::instance()->regist("http",define);
		}
	};

}}}