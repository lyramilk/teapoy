#ifndef _lyramilk_teapoy_httpclient_h_
#define _lyramilk_teapoy_httpclient_h_

#include "config.h"
#include "mime.h"
#include <libmilk/var.h>
#include <libmilk/netio.h>

namespace lyramilk{ namespace teapoy
{
	class httpclient:public lyramilk::netio::client
	{
		int timeout_msec;
		lyramilk::data::string url;
		lyramilk::data::string host;
	  public:
		httpclient();
		virtual ~httpclient();

		virtual bool open(const lyramilk::data::string& url);
		virtual bool post(const http_header_type& headers,const lyramilk::data::string& body);
		virtual bool get(const http_header_type& headers);

		virtual lyramilk::data::string get_host();


		virtual bool wait_response(int timeout_msec = 3600 * 1000);
		virtual int get_response(http_header_type* headers,lyramilk::data::string* body);

		static lyramilk::data::string rcall(const char* url,int timeout_msec = 2000);
		static lyramilk::data::chunk rcallb(const char* url,int timeout_msec = 2000);
		static lyramilk::data::string makeurl(const char* url,const lyramilk::data::stringdict& params);
		static lyramilk::data::string makeurl(const char* url,const lyramilk::data::map& params);
	};	
}}

#endif
