#include "web.h"
#include "script.h"
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


	struct url_worker_lua_loger
	{
		lyramilk::debug::nsecdiff td;
		const lyramilk::log::logss& loger;
		lyramilk::teapoy::http::request* req;

		url_worker_lua_loger(const lyramilk::log::logss& _log,lyramilk::teapoy::http::request* req):loger(_log)
		{
			td.mark();
			this->req = req;
		}

		~url_worker_lua_loger()
		{
			long long des = td.diff();
			loger(lyramilk::log::trace) << D("%s:%u-->%s 耗时：%lld(纳秒)",req->dest.c_str(),req->dest_port,req->url.c_str(),des) << std::endl;
		}
	};

	class url_worker_lua : public url_worker
	{
		lyramilk::script::engines* pool;
		lyramilk::log::logss log;
		bool call(lyramilk::teapoy::http::request* req,std::ostream& os,lyramilk::data::string real,website_worker& worker) const
		{
			url_worker_lua_loger _(log,req);

			req->parse_cookies();
			req->parse_body_param();
			req->parse_url();

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
		url_worker_lua(lyramilk::script::engines* es):log(lyramilk::klog,"teapoy.web.lua")
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
