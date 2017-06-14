#include "web.h"
#include "script.h"
#include <libmilk/log.h>
#include <libmilk/multilanguage.h>

namespace lyramilk{ namespace teapoy { namespace web {

	class url_worker_lua : public url_worker
	{
		lyramilk::script::engines* pool;
		bool call(lyramilk::teapoy::http::request* req,std::ostream& os,lyramilk::data::string real,website_worker& worker) const
		{
			url_worker_loger _("teapoy.web.lua",req);

			lyramilk::script::engines::ptr p = pool->get();
			if(!p->load_file(real)){
				lyramilk::klog(lyramilk::log::warning,"teapoy.web.lua") << D("加载文件%s失败",real.c_str()) << std::endl;
				return false;
			}

			session_info si(real,req,os,worker);

			lyramilk::data::var::array ar;
			{
				lyramilk::data::var var_processer_args("__http_session_info",&si);

				lyramilk::data::var::array args;
				args.push_back(var_processer_args);

				ar.push_back(p->createobject("HttpRequest",args));
				ar.push_back(p->createobject("HttpResponse",args));
			}

			lyramilk::data::var vret = p->call("onrequest",ar);
			if(vret.type_like(lyramilk::data::var::t_bool)){
				return vret;
			}
			return false;
		}
	  public:
		url_worker_lua(lyramilk::script::engines* es)
		{
			pool = es;
		}
		virtual ~url_worker_lua()
		{
		}

		static url_worker* ctr(void*)
		{
			lyramilk::script::engines* es = engine_pool::instance()->get("lua");
			if(es)return new url_worker_lua(es);
			return nullptr;
		}

		static void dtr(url_worker* p)
		{
			delete (url_worker_lua*)p;
		}
	};


	class url_worker_luax : public url_worker
	{
		lyramilk::script::engines* pool;

		bool call(lyramilk::teapoy::http::request* req,std::ostream& os,lyramilk::data::string real,website_worker& worker) const
		{
			TODO();
		}
	  public:
		url_worker_luax(lyramilk::script::engines* es)
		{
			pool = es;
		}
		virtual ~url_worker_luax()
		{
		}

		static url_worker* ctr(void*)
		{
			lyramilk::script::engines* es = engine_pool::instance()->get("lua");
			if(es)return new url_worker_luax(es);
			return nullptr;
		}

		static void dtr(url_worker* p)
		{
			delete (url_worker_luax*)p;
		}
	};
	static __attribute__ ((constructor)) void __init()
	{
		url_worker_master::instance()->define("lua",url_worker_lua::ctr,url_worker_lua::dtr);
		url_worker_master::instance()->define("luax",url_worker_luax::ctr,url_worker_luax::dtr);
	}
}}}
