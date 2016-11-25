#ifndef _lyramilk_teapoy_http_h_
#define _lyramilk_teapoy_http_h_

#include <libmilk/var.h>
#include "mime.h"

namespace lyramilk{ namespace teapoy {namespace http{


	class request:public mime
	{
		lyramilk::data::string httpheader;
		enum 
		{
			s0,
			smime,
			sbad,
		}s;
	  public:
		lyramilk::data::string method;
		lyramilk::data::string url;
		lyramilk::data::string root;
		lyramilk::data::var::map parameter;
		lyramilk::data::var::map cookies;

		struct
		{
			int major;
			int minor;
		}ver;

		lyramilk::data::string source;
		lyramilk::data::string dest;
		lyramilk::data::uint16 source_port;
		lyramilk::data::uint16 dest_port;
		lyramilk::data::string nonce;	//这个值用于http摘要认证，在reset的时候不清空。	

		request();
		virtual ~request();
		virtual bool parse(const char* buf,int size,int* remain);
		virtual bool bad();
		virtual void reset();
		virtual bool parse_args();
		virtual bool parse_cookies();
	};


	lyramilk::data::string get_error_code_desc(int code);
}}}

#endif
