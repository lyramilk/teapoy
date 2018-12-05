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
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_int);

			if(!http) return false;
			return http->open(args[0]);
		}

		lyramilk::data::var bind_url(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_array);
			const lyramilk::data::array& ar = args[0];
			lyramilk::data::array::const_iterator it = ar.begin();
			for(;it!=ar.end();++it){
				lyramilk::data::string emptystr;
				lyramilk::data::map emptymap;
				lyramilk::data::string method = it->at("method").conv(emptystr);
				lyramilk::data::string type = it->at("type").conv(emptystr);
				lyramilk::data::string pattern = it->at("pattern").conv(emptystr);
				lyramilk::data::string module = it->at("module").conv(emptystr);
				const lyramilk::data::map& auth = it->at("auth").conv(emptymap);
				lyramilk::data::strings defpages;
				{
					lyramilk::data::array emptyarray;
					const lyramilk::data::array& v = it->at("index").conv(emptyarray);

					lyramilk::data::array::const_iterator it = v.begin();
					for(;it!=v.end();++it){
						if(it->type_like(lyramilk::data::var::t_str)){
							defpages.push_back(it->str());
						}else{
							log(lyramilk::log::warning,__FUNCTION__) << D("定义url映射警告：默认页面%s类型错误,源信息：%s",it->type(),it->str().c_str()) << std::endl;
						}
					}
				}

				url_regex_selector* s = (url_regex_selector*)url_selector_factory::instance()->create(type);
				if(s){
					s->init(defpages,pattern,module);
					if(this->http->add(s)){
						log(lyramilk::log::debug,__FUNCTION__) << D("定义url映射成功：类型%s 正则表达式%s 模式%s",type.c_str(),pattern.c_str(),module.c_str()) << std::endl;
					}
				}else{
					log(lyramilk::log::warning,__FUNCTION__) << D("定义url映射失败：%s类型未定义",type.c_str()) << std::endl;
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

		lyramilk::data::var set_urlhook(const lyramilk::data::array& args,const lyramilk::data::map& env)
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
			fn["bind_url"] = lyramilk::script::engine::functional<httpserver,&httpserver::bind_url>;
			fn["set_session_manager"] = lyramilk::script::engine::functional<httpserver,&httpserver::set_session_manager>;
			fn["set_ssl"] = lyramilk::script::engine::functional<httpserver,&httpserver::set_ssl>;
			//fn["set_urlhook"] = lyramilk::script::engine::functional<httpserver,&httpserver::set_urlhook>;
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