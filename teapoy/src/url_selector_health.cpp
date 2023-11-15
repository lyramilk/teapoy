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

	class url_selector_health:public url_regex_selector
	{
		lyramilk::data::string str_body;
	  public:
		url_selector_health()
		{
			str_body = "{\"code\":200,\"msg\":\"ok\"}";
		}

		virtual ~url_selector_health()
		{}

		virtual dispatcher_check_status call(httprequest* request,httpresponse* response,httpadapter* adapter,const lyramilk::data::string& real)
		{
			response->set("Content-Type","application/json;charset=utf-8");
			
			adapter->response->code = 200;
			adapter->response->content_length = str_body.size();
			adapter->send_data(str_body.c_str(),str_body.size());
			return cs_ok;
		}

		static url_selector* ctr(void*)
		{
			static url_selector_health st;
			return &st;
		}

		static void dtr(url_selector* p)
		{
		}
	};

	static __attribute__ ((constructor)) void __init()
	{
		url_selector_factory::instance()->define("health",url_selector_health::ctr,url_selector_health::dtr);
	}

}}
