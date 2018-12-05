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

		virtual bool call(httprequest* request,httpresponse* response,httpadapter* adapter,const lyramilk::data::string& real)
		{
			url_selector_loger _("teapoy.web.jshtml",adapter);
			return vcall(request,response,adapter,real);
		}

		bool vcall(httprequest* request,httpresponse* response,httpadapter* adapter,const lyramilk::data::string& real)
		{
			lyramilk::script::engines::ptr p = pool->get();
			if(!p->load_file(real)){
				lyramilk::klog(lyramilk::log::warning,"teapoy.web.jshtml") << D("加载文件%s失败",real.c_str()) << std::endl;
				return false;
			}

			lyramilk::data::stringstream ss;
			lyramilk::data::uint32 response_code = 200;
			lyramilk::data::array ar;
			{
				lyramilk::data::var jsin_adapter_param("..http.session.adapter",adapter);
				jsin_adapter_param.userdata("..http.session.response.stream",&(std::ostream&)ss);
				jsin_adapter_param.userdata("..http.session.response.code",&response_code);

				lyramilk::data::array jsin_param;
				jsin_param.push_back(jsin_adapter_param);

				response->set("Content-Type","text/html;charset=utf-8");

				ar.push_back(p->createobject("HttpRequest",jsin_param));
				ar.push_back(p->createobject("HttpResponse",jsin_param));
				if(request->data.empty()){
					ar.push_back(lyramilk::data::var::nil);
				}else{
					ar.push_back(request->data);
				}
			}

			lyramilk::data::var vret = p->call(request->mode.empty()?"onrequest":request->mode,ar);
			if(vret.type() == lyramilk::data::var::t_invalid){
				adapter->send_header_with_length(adapter->response,500,0);
				return true;
			}
			if(vret.type_like(lyramilk::data::var::t_bool)){
				bool r = vret;
				if(r){
					request->set("Connection","close");
				}
			}

			if(response_code > 0){
				bool usedgzip = false;
#ifdef ZLIB_FOUND
				{
					std::size_t datasize = ss.str().size();
					lyramilk::data::string sacceptencoding = adapter->request->get("Accept-Encoding");
					if(sacceptencoding.find("gzip") != sacceptencoding.npos && datasize > 512){
						adapter->response->set("Content-Encoding","gzip");
						adapter->send_header_with_chunk(adapter->response,response_code);
						lyramilk::teapoy::http_chunked_gzip(ss,datasize,adapter);
						usedgzip = true;
					}
				}
#endif
				if(!usedgzip){
					lyramilk::data::string str_body = ss.str();
					adapter->send_header_with_length(adapter->response,response_code,str_body.size());
					adapter->send_bodydata(adapter->response,str_body.c_str(),str_body.size());
				}
			}

			return true;
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
