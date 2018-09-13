#ifndef _lyramilk_teapoy_httpclient_h_
#define _lyramilk_teapoy_httpclient_h_

#include "config.h"
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
		virtual bool post(const lyramilk::data::stringdict& headers,const lyramilk::data::string& body);
		virtual bool get(const lyramilk::data::stringdict& headers);

		virtual bool wait_response();
		virtual int get_response(lyramilk::data::stringdict* headers,lyramilk::data::string* body);

		static lyramilk::data::string rcall(const char* url,const lyramilk::data::stringdict& params,int timeout_msec = 2000);
		static lyramilk::data::string makeurl(const char* url,const lyramilk::data::stringdict& params);
	};	
}}

#endif
