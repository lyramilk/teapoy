#include "web.h"
#include "script.h"
#include <libmilk/log.h>
#include <libmilk/dict.h>

namespace lyramilk{ namespace teapoy { namespace web {

	class url_worker_lua : public url_worker
	{
		lyramilk::script::engines* pool;
		bool call(session_info* si) const
		{
			url_worker_loger _("teapoy.web.lua",si->req);

			lyramilk::script::engines::ptr p = pool->get();
			if(!p->load_file(si->real)){
				lyramilk::klog(lyramilk::log::warning,"teapoy.web.lua") << D("加载文件%s失败",si->real.c_str()) << std::endl;
				return false;
			}

			lyramilk::data::array ar;
			{
				lyramilk::teapoy::web::session_info_datawrapper var_processer_args(si);

				lyramilk::data::array args;
				args.push_back(var_processer_args);

				ar.push_back(p->createobject("HttpRequest",args));
				ar.push_back(p->createobject("HttpResponse",args));
			}

			lyramilk::data::var vret;
			bool jsret = p->call("onrequest",ar,&vret);
			if(si->response_code == 0){
				if(jsret){
					si->response_code = 200;
				}else{
					si->response_code = 503;
				} 
			}

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

		bool call(session_info* si) const
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
