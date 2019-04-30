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
				if(request->header_extend.empty()){
					ar.push_back(lyramilk::data::var::nil);
				}else{
					ar.push_back(request->header_extend);
				}
			}

			lyramilk::data::var vret = p->call(request->mode.empty()?"onrequest":request->mode,ar);
			if(vret.type() == lyramilk::data::var::t_invalid){
				adapter->response->code = 500;
				adapter->response->content_length = 0;
				adapter->send_header();
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
				if(adapter->allow_gzip() && adapter->allow_chunk()){
					std::size_t datasize = ss.str().size();
					lyramilk::data::string sacceptencoding = adapter->request->get("Accept-Encoding");
					if(sacceptencoding.find("gzip") != sacceptencoding.npos && datasize > 512){
						adapter->response->set("Content-Encoding","gzip");

						adapter->response->code = response_code;

						if(adapter->send_header() != httpadapter::ss_error){
							lyramilk::teapoy::http_chunked_gzip(ss,datasize,adapter);
							usedgzip = true;
						}else{
							adapter->response->set("Content-Encoding","");
						}

					}
				}
#endif
				if(!usedgzip){
					lyramilk::data::string str_body = ss.str();

					adapter->response->code = response_code;
					adapter->response->content_length = str_body.size();


					if(adapter->send_header() == httpadapter::ss_need_body){
						adapter->send_data(str_body.c_str(),str_body.size());
						adapter->send_finish();
					}
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
