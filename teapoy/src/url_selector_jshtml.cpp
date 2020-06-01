#include "url_dispatcher.h"
#include "stringutil.h"
#include "mimetype.h"
#include <libmilk/dict.h>
#include <libmilk/log.h>
#include <sys/stat.h>
#include <errno.h>
#include "script.h"
#include "teapoy_gzip.h"

namespace lyramilk{ namespace teapoy {

	class url_selector_jshtml:public url_regex_selector
	{
		lyramilk::script::engines* pool;
	  public:
		url_selector_jshtml(lyramilk::script::engines* es)
		{
			pool = es;
		}

		virtual ~url_selector_jshtml()
		{}

		virtual dispatcher_check_status call(httprequest* request,httpresponse* response,httpadapter* adapter,const lyramilk::data::string& real)
		{
			url_selector_loger _("teapoy.web.jshtml",adapter);
			return vcall(request,response,adapter,real);
		}

		dispatcher_check_status vcall(httprequest* request,httpresponse* response,httpadapter* adapter,const lyramilk::data::string& real)
		{
			lyramilk::script::engines::ptr p = pool->get();
			if(!p->load_file(real)){
				lyramilk::klog(lyramilk::log::warning,"teapoy.web.jshtml") << D("加载文件%s失败",real.c_str()) << std::endl;
				adapter->response->code = 500;
				return cs_error;
			}

			lyramilk::data::stringstream ss;
			lyramilk::data::array ar;
			{
				lyramilk::data::array jsin_param;
				jsin_param.push_back(session_response_datawrapper(adapter,ss));

				response->set("Content-Type","text/html;charset=utf-8");

				ar.push_back(p->createobject("HttpRequest",jsin_param));
				ar.push_back(p->createobject("HttpResponse",jsin_param));
				if(request->header_extend.empty()){
					ar.push_back(lyramilk::data::var::nil);
				}else{
					ar.push_back(request->header_extend);
				}
			}

			lyramilk::data::var vret;
			if(!p->call(request->mode.empty()?"onrequest":request->mode,ar,&vret)){
				adapter->response->code = 503;
				return cs_error;
			}
			if(vret.type() == lyramilk::data::var::t_invalid){
				adapter->response->code = 500;
				return cs_error;
			}
			if(vret.type_like(lyramilk::data::var::t_bool)){
				bool r = vret;
				if(r){
					request->set("Connection","close");
				}
			}

			if(adapter->response->code > 0){
				bool usedgzip = false;
#ifdef ZLIB_FOUND
				if(adapter->allow_gzip() && adapter->allow_chunk()){
					std::size_t datasize = ss.str().size();
					lyramilk::data::string sacceptencoding = adapter->request->get("Accept-Encoding");
					if(sacceptencoding.find("gzip") != sacceptencoding.npos && datasize > 512){
						adapter->response->set("Content-Encoding","gzip");

						lyramilk::teapoy::http_chunked_gzip(ss,datasize,adapter);
						usedgzip = true;
					}
				}
#endif
				if(!usedgzip){
					lyramilk::data::string str_body = ss.str();

					adapter->response->content_length = str_body.size();
					adapter->send_data(str_body.c_str(),str_body.size());
				}
			}

			return cs_ok;
		}


		static url_selector* ctr(void*)
		{
			lyramilk::script::engines* es = engine_pool::instance()->get("jshtml");
			if(es)return new url_selector_jshtml(es);
			return nullptr;
		}

		static void dtr(url_selector* p)
		{
			delete (url_selector_jshtml*)p;
		}
	};

	static __attribute__ ((constructor)) void __init()
	{
		url_selector_factory::instance()->define("jshtml",url_selector_jshtml::ctr,url_selector_jshtml::dtr);
	}

}}
