#include "url_dispatcher.h"
#include "stringutil.h"
#include "mimetype.h"
#include <libmilk/multilanguage.h>
#include <libmilk/log.h>
#include <sys/stat.h>
#include <errno.h>
#include "script.h"

namespace lyramilk{ namespace teapoy {

	class url_selector_js:public url_regex_selector
	{
		lyramilk::script::engines* pool;
	  public:
		url_selector_js(lyramilk::script::engines* es)
		{
			pool = es;
		}

		virtual ~url_selector_js()
		{}

		virtual bool call(httprequest* request,httpresponse* response,httpadapter* adapter,const lyramilk::data::string& real)
		{
			return vcall(request,response,adapter,real);
		}

		bool vcall(httprequest* request,httpresponse* response,httpadapter* adapter,const lyramilk::data::string& real)
		{
			lyramilk::script::engines::ptr p = pool->get();
			if(!p->load_file(real)){
				lyramilk::klog(lyramilk::log::warning,"teapoy.web.js") << D("加载文件%s失败",real.c_str()) << std::endl;
				return false;
			}

			lyramilk::data::var::array ar;
			{
				lyramilk::data::var jsin_adapter_param("..http.session.adapter",adapter);

				lyramilk::data::var::array jsin_param;
				jsin_param.push_back(jsin_adapter_param);

				ar.push_back(p->createobject("HttpRequest",jsin_param));
				ar.push_back(p->createobject("HttpResponse",jsin_param));
				if(request->data.empty()){
					ar.push_back(lyramilk::data::var::nil);
				}else{
					ar.push_back(request->data);
				}
			}

			lyramilk::data::var vret = p->call(request->mode.empty()?"onrequest":request->mode,ar);
			if(vret.type_like(lyramilk::data::var::t_bool)){
				return vret;
			}

			return false;
		}


		static url_selector* ctr(void*)
		{
			lyramilk::script::engines* es = engine_pool::instance()->get("js");
			if(es)return new url_selector_js(es);
			return nullptr;
		}

		static void dtr(url_selector* p)
		{
			delete (url_selector_js*)p;
		}
	};

	static __attribute__ ((constructor)) void __init()
	{
		url_selector_factory::instance()->define("js",url_selector_js::ctr,url_selector_js::dtr);
	}

}}
