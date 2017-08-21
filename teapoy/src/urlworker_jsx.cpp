#include "web.h"
#include "script.h"
#include "script_jsx.h"
#include <libmilk/log.h>
#include <libmilk/multilanguage.h>

namespace lyramilk{ namespace teapoy { namespace web {

	class url_worker_jsx : public url_worker
	{
		lyramilk::script::engines* pool;
		bool call(session_info* si) const
		{
			url_worker_loger _("teapoy.web.jsx",si->req);

			lyramilk::script::engines::ptr p = pool->get();
			if(!p->load_file(si->real)){
				lyramilk::klog(lyramilk::log::warning,"teapoy.web.jsx") << D("加载文件%s失败",si->real.c_str()) << std::endl;
				return false;
			}

			lyramilk::data::var::array ar;
			{
				lyramilk::data::var var_processer_args("__http_session_info",si);

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
		url_worker_jsx(lyramilk::script::engines* es)
		{
			pool = es;
		}
		virtual ~url_worker_jsx()
		{
		}

		static url_worker* ctr(void*)
		{
			lyramilk::script::engines* es = engine_pool::instance()->get("jsx");
			if(es)return new url_worker_jsx(es);
			return nullptr;
		}

		static void dtr(url_worker* p)
		{
			delete (url_worker_jsx*)p;
		}
	};
	static __attribute__ ((constructor)) void __init()
	{
		url_worker_master::instance()->define("jsx",url_worker_jsx::ctr,url_worker_jsx::dtr);
	}
}}}
