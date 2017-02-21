#include <libmilk/scriptengine.h>
#include <libmilk/log.h>
#include <libmilk/multilanguage.h>
#include <libmilk/netaio.h>
#include "script.h"
#include "web.h"
#include "env.h"
#include "mime.h"
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

namespace lyramilk{ namespace teapoy{ namespace native
{
	class httpserver:public lyramilk::netio::aiolistener
	{
		lyramilk::log::logss log;
		lyramilk::data::string root;

		web::website_worker worker;
	  public:

		static void* ctr(const lyramilk::data::var::array& args)
		{
			return new httpserver();
		}
		static void dtr(void* p)
		{
			delete (httpserver*)p;
		}

		httpserver():log(lyramilk::klog,"teapoy.server.httpserver")
		{
		}

		~httpserver()
		{}

		lyramilk::data::var open(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_int);
/*
			proc_master.define("jsx",web::processer_jsx::ctr,web::processer_jsx::dtr);
			log(lyramilk::log::debug,__FUNCTION__) << D("注册脚本引擎适配器：%s","jsx") << std::endl;
			proc_master.define("js",web::processer_js::ctr,web::processer_js::dtr);
			log(lyramilk::log::debug,__FUNCTION__) << D("注册脚本引擎适配器：%s","js") << std::endl;
			proc_master.define("lua",web::processer_lua::ctr,web::processer_lua::dtr);
			log(lyramilk::log::debug,__FUNCTION__) << D("注册脚本引擎适配器：%s","lua") << std::endl;
			lyramilk::data::var::array keys = web::methodinvokers::instance()->keys();
			for(lyramilk::data::var::array::iterator it = keys.begin();it!=keys.end();++it){
				web::methodinvoker* invoker = web::methodinvokers::instance()->get(it->str());
				if(invoker){
					invoker->set_processer(&proc_master);
				}
			}
*/
			return lyramilk::netio::aiolistener::open(args[0]);
		}

		lyramilk::data::var bind_url(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_array);
			const lyramilk::data::var::array& ar = args[0];
			lyramilk::data::var::array::const_iterator it = ar.begin();
			for(;it!=ar.end();++it){
				lyramilk::data::string method = it->at("method");
				lyramilk::data::string type = it->at("type");
				lyramilk::data::string pattern = it->at("pattern");
				lyramilk::data::string module = it->at("module");
				lyramilk::data::var auth = it->at("auth");
				lyramilk::data::var::array index;

				{
					const lyramilk::data::var& v = it->at("index");
					if(v.type() == lyramilk::data::var::t_array){
						index = v;
					}
				}

				web::url_worker *w = web::url_worker_master::instance()->create(type);
				if(w){
					w->init(method,pattern,module,index);
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

		virtual lyramilk::netio::aiosession* create()
		{
			lyramilk::teapoy::web::aiohttpsession* ss = lyramilk::netio::aiosession::__tbuilder<lyramilk::teapoy::web::aiohttpsession>();
			ss->worker = &worker;
			return ss;
		}
		 		
		static lyramilk::data::var DefineMimeType(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
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
			fn["bind_url"] = lyramilk::script::engine::functional<httpserver,&httpserver::bind_url>;
			fn["set_ssl"] = lyramilk::script::engine::functional<httpserver,&httpserver::set_ssl>;
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