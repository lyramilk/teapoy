#include "web.h"
#include "script.h"
#include "script_jsx.h"
#include <fstream>
#include <sys/stat.h>
#include <errno.h>
#include <libmilk/log.h>
#include <libmilk/multilanguage.h>
#include <libmilk/testing.h>
#include <pcre.h>
#include <libmilk/codes.h>
#include <utime.h>

namespace lyramilk{ namespace teapoy { namespace web {


	struct url_worker_js_loger
	{
		lyramilk::debug::nsecdiff td;
		const lyramilk::log::logss& loger;
		lyramilk::teapoy::http::request* req;

		url_worker_js_loger(const lyramilk::log::logss& _log,lyramilk::teapoy::http::request* req):loger(_log)
		{
			td.mark();
			this->req = req;
		}

		~url_worker_js_loger()
		{
			long long des = td.diff();
			loger(lyramilk::log::trace) << D("%s:%u-->%s 耗时：%lld(纳秒)",req->dest().c_str(),req->dest_port(),req->header->uri.c_str(),des) << std::endl;
		}
	};

	class url_worker_js : public url_worker
	{
		lyramilk::script::engines* pool;
		lyramilk::log::logss log;
		bool call(lyramilk::teapoy::http::request* req,std::ostream& os,lyramilk::data::string real,website_worker& worker) const
		{
			url_worker_js_loger _(log,req);

			lyramilk::script::engines::ptr p = pool->get();
			if(!p->load_file(real)){
				log(lyramilk::log::warning,__FUNCTION__) << D("加载文件%s失败",real.c_str()) << std::endl;
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
		url_worker_js(lyramilk::script::engines* es):log(lyramilk::klog,"teapoy.web.js")
		{
			pool = es;
		}
		virtual ~url_worker_js()
		{
		}

		static url_worker* ctr(void*)
		{
			lyramilk::script::engines* es = engine_pool::instance()->get("js");
			if(es)return new url_worker_js(es);
			return nullptr;
		}

		static void dtr(url_worker* p)
		{
			delete (url_worker_js*)p;
		}
	};


	class url_worker_jsx : public url_worker
	{
		lyramilk::script::engines* pool;
		lyramilk::log::logss log;
		bool call(lyramilk::teapoy::http::request* req,std::ostream& os,lyramilk::data::string real,website_worker& worker) const
		{
			url_worker_js_loger _(log,req);

			lyramilk::script::engines::ptr p = pool->get();
			if(!p->load_file(real)){
				log(lyramilk::log::warning,__FUNCTION__) << D("加载文件%s失败",real.c_str()) << std::endl;
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
		url_worker_jsx(lyramilk::script::engines* es):log(lyramilk::klog,"teapoy.web.jsx")
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
		url_worker_master::instance()->define("js",url_worker_js::ctr,url_worker_js::dtr);
		url_worker_master::instance()->define("jsx",url_worker_jsx::ctr,url_worker_jsx::dtr);
	}
}}}
