#include <libmilk/scriptengine.h>
#include <libmilk/log.h>
#include <libmilk/dict.h>
#include <libmilk/netaio.h>
#include "script.h"
#include "web.h"
#include "env.h"
#include "mime.h"
#include "session.h"
#include "webhook.h"
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include "sni_selector.h"

namespace lyramilk{ namespace teapoy{ namespace native
{


	class epoll_server:public lyramilk::netio::aiolistener
	{
	  public:
		web::website_worker* worker;
		virtual lyramilk::netio::aiosession* create()
		{
			lyramilk::teapoy::web::aiohttpsession* ss = lyramilk::netio::aiosession::__tbuilder<lyramilk::teapoy::web::aiohttpsession>();
			ss->worker = worker;
			ss->sessionmgr = lyramilk::teapoy::web::sessions::defaultinstance();
			return ss;
		}
	};



	class httpserver:public epoll_selector
	{
		lyramilk::log::logss log;
		lyramilk::data::string root;
		web::website_worker worker;
		epoll_server* http;
	  public:

		static lyramilk::script::sclass* ctr(const lyramilk::data::array& args)
		{
			return new httpserver();
		}
		static void dtr(lyramilk::script::sclass* p)
		{
			delete (httpserver*)p;
		}

		httpserver():log(lyramilk::klog,"teapoy.server.httpserver")
		{
			http = new epoll_server;
			http->worker = &worker;
			selector = http;
		}

		virtual ~httpserver()
		{
			delete http;
		}

		lyramilk::data::var open(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_int);
			return http->open(args[0]);
		}

		lyramilk::data::var open_unixsocket(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_int);
			return http->open_unixsocket(args[0]);
		}

		lyramilk::data::var bind_url(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_array);
			const lyramilk::data::array& ar = args[0];
			lyramilk::data::array::const_iterator it = ar.begin();
			for(;it!=ar.end();++it){
				lyramilk::data::string method = it->at("method");
				lyramilk::data::string type = it->at("type");
				lyramilk::data::string pattern = it->at("pattern");
				lyramilk::data::var module = it->at("module");
				lyramilk::data::string hooktype = it->at("hook");
				const lyramilk::data::var& auth = it->at("auth");
				lyramilk::data::strings index;

				{
					const lyramilk::data::var& v = it->at("index");
					if(v.type() == lyramilk::data::var::t_array){
						const lyramilk::data::array& t = v;
						for(lyramilk::data::array::const_iterator it = t.begin();it!=t.end();++it){
							index.push_back(it->str());
						}
					}
				}
				lyramilk::data::strings pattern_premise;

				{
					const lyramilk::data::var& v = it->at("assert");
					if(v.type() == lyramilk::data::var::t_array){
						const lyramilk::data::array& t = v;
						for(lyramilk::data::array::const_iterator it = t.begin();it!=t.end();++it){
							pattern_premise.push_back(it->str());
						}
					}
				}

				web::url_worker *w = web::url_worker_master::instance()->create(type);
				if(w){
					if(module.type() == lyramilk::data::var::t_invalid){
						w->init(method,pattern_premise,pattern,"",index,lyramilk::teapoy::web::allwebhooks::instance()->get(hooktype));
					}else{
						w->init(method,pattern_premise,pattern,module,index,lyramilk::teapoy::web::allwebhooks::instance()->get(hooktype));
					}
					w->init_extra(*it);
					if(auth.type() == lyramilk::data::var::t_map){
						w->init_auth(auth["type"],auth["module"]);
					}
					worker.lst.push_back(w);
				}else{
					log(lyramilk::log::warning,__FUNCTION__) << D("定义url处理器失败：%s类型未定义",type.c_str()) << std::endl;
				}
			}
			return true;
		}

		lyramilk::data::var set_urlhook(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			worker.set_urlhook(args[0].str());
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
			lyramilk::log::logss log(lyramilk::klog,"teapoy.server.httpserver");
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_str);
			lyramilk::data::string extname = args[0];
			lyramilk::data::string mimetype = args[1];
			mime::define_mimetype_fileextname(extname,mimetype);
			return true;
		}

		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["open"] = lyramilk::script::engine::functional<httpserver,&httpserver::open>;
			fn["open_unixsocket"] = lyramilk::script::engine::functional<httpserver,&httpserver::open_unixsocket>;
			fn["bind_url"] = lyramilk::script::engine::functional<httpserver,&httpserver::bind_url>;
			fn["set_ssl"] = lyramilk::script::engine::functional<httpserver,&httpserver::set_ssl>;
			fn["set_urlhook"] = lyramilk::script::engine::functional<httpserver,&httpserver::set_urlhook>;
			fn["set_ssl_verify_locations"] = lyramilk::script::engine::functional<httpserver,&httpserver::set_ssl_verify_locations>;
			fn["set_ssl_client_verify"] = lyramilk::script::engine::functional<httpserver,&httpserver::set_ssl_client_verify>;
			p->define("httpserver",fn,httpserver::ctr,httpserver::dtr);
			p->define("DefineMimeType",DefineMimeType);
			return 1;
		}

		static __attribute__ ((constructor)) void __init()
		{
			lyramilk::teapoy::script2native::instance()->regist("http",define);
		}
	};

}}}