#include <libmilk/scriptengine.h>
#include <libmilk/log.h>
#include <libmilk/multilanguage.h>
#include <libmilk/netaio.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include "script.h"
#include "webservice.h"
#include "httplistener.h"
#include "http_1_0.h"
#include "http_1_1.h"
#include "http_2_0.h"
#include "sessionmgr.h"

namespace lyramilk{ namespace teapoy{ namespace native
{
	lyramilk::log::logss static log(lyramilk::klog,"teapoy.native.HttpServer");
	class httpserver:public lyramilk::teapoy::httplistener
	{
	  public:

		static void* ctr(const lyramilk::data::var::array& args)
		{
			httpserver* p = new httpserver();
			if(p){
				p->scheme = "http";
				p->define_http_protocols(http_1_0::proto_info.name,http_1_0::proto_info.ctr,http_1_0::proto_info.dtr);
				p->define_http_protocols(http_1_1::proto_info.name,http_1_1::proto_info.ctr,http_1_1::proto_info.dtr);
				p->define_http_protocols(http_2_0::proto_info.name,http_2_0::proto_info.ctr,http_2_0::proto_info.dtr);
			}
			return p;
		}

		static void* ctr2(const lyramilk::data::var::array& args)
		{
			httpserver* p = new httpserver();
			if(p){
				p->scheme = "https";
				p->define_http_protocols(http_1_0::proto_info.name,http_1_0::proto_info.ctr,http_1_0::proto_info.dtr);
				p->define_http_protocols(http_1_1::proto_info.name,http_1_1::proto_info.ctr,http_1_1::proto_info.dtr);
				p->define_http_protocols(http_2_0::proto_info.name,http_2_0::proto_info.ctr,http_2_0::proto_info.dtr);
			}
			return p;
		}
		static void dtr(void* p)
		{
			delete (httpserver*)p;
		}

		httpserver()
		{
			session_mgr = lyramilk::teapoy::httpsession_manager_factory::instance()->create("default");
		}

		virtual ~httpserver()
		{
		
		}

		lyramilk::data::var open(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_int);
			return lyramilk::netio::aiolistener::open(args[0]);
		}

		lyramilk::data::var bind_url(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_array);
			const lyramilk::data::var::array& ar = args[0];
			lyramilk::data::var::array::const_iterator it = ar.begin();
			for(;it!=ar.end();++it){
				lyramilk::data::string emptystr;
				lyramilk::data::var::map emptymap;
				lyramilk::data::string method = it->at("method").conv(emptystr);
				lyramilk::data::string type = it->at("type").conv(emptystr);
				lyramilk::data::string pattern = it->at("pattern").conv(emptystr);
				lyramilk::data::string module = it->at("module").conv(emptystr);
				const lyramilk::data::var::map& auth = it->at("auth").conv(emptymap);
				lyramilk::data::strings defpages;
				{
					lyramilk::data::var::array emptyarray;
					const lyramilk::data::var::array& v = it->at("index").conv(emptyarray);

					lyramilk::data::var::array::const_iterator it = v.begin();
					for(;it!=v.end();++it){
						if(it->type_like(lyramilk::data::var::t_str)){
							defpages.push_back(it->str());
						}else{
							log(lyramilk::log::warning,__FUNCTION__) << D("定义url处理器警告：默认页面%s类型错误,源信息：%s",it->type(),it->str().c_str()) << std::endl;
						}
					}
				}

				url_regex_selector* s = (url_regex_selector*)url_selector_factory::instance()->create(type);
				if(s){
					s->init(defpages,pattern,module);
					if(this->add(s)){
						log(lyramilk::log::debug,__FUNCTION__) << D("定义url处理器成功：类型%s 正则表达式%s 模式%s",type.c_str(),pattern.c_str(),module.c_str()) << std::endl;
					}
				}else{
					log(lyramilk::log::warning,__FUNCTION__) << D("定义url处理器失败：%s类型未定义",type.c_str()) << std::endl;
				}
			}
			return true;
		}



		lyramilk::data::var set_session_manager(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			session_mgr = lyramilk::teapoy::httpsession_manager_factory::instance()->create(args[0].str());
			if(session_mgr){
				if(args.size() > 1){
					MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_map);
					session_mgr->init(args[1]);
				}else{
					lyramilk::data::var::map m;
					session_mgr->init(m);
				}
			}
			return true;
		}

		lyramilk::data::var set_urlhook(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			//worker.set_urlhook(args[0].str());
			return true;
		}

		lyramilk::data::var set_ssl(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_str);
			init_ssl(args[0],args[1]);
			return true;
		}

		lyramilk::data::var set_ssl_verify_locations(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_array);
			return ssl_load_verify_locations(args[0]);
		}

		lyramilk::data::var set_ssl_client_verify(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_bool);
			return ssl_use_client_verify(args[0]);
		}

		static lyramilk::data::var DefineMimeType(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_str);
			lyramilk::data::string extname = args[0];
			lyramilk::data::string mimetype = args[1];
			//mime::define_mimetype_fileextname(extname,mimetype);
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