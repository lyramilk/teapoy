#ifndef _lyramilk_teapoy_http_h_
#define _lyramilk_teapoy_http_h_

#include <libmilk/var.h>
#include <libmilk/netaio.h>
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

		char parse_status;
	  public:
		lyramilk::data::string _source;
		lyramilk::data::string _dest;
		lyramilk::data::uint16 _source_port;
		lyramilk::data::uint16 _dest_port;

		lyramilk::data::string method;
		lyramilk::data::string url;
		lyramilk::data::string url_pure;
		lyramilk::data::var::map parameter;
		lyramilk::data::var::map cookies;
		lyramilk::data::string ssl_peer_certificate_info;

		struct
		{
			int major;
			int minor;
		}ver;

		lyramilk::data::string source();
		lyramilk::data::string dest();
		lyramilk::data::uint16 source_port();
		lyramilk::data::uint16 dest_port();

		lyramilk::io::native_filedescriptor_type fd;

		request();
		virtual ~request();
		virtual bool parse(const char* buf,int size,int* remain);
		virtual bool bad();
		virtual void reset();
		virtual bool parse_url();
		virtual bool parse_body_param();
		virtual bool parse_cookies();

		static void make_response();
	};


	lyramilk::data::string get_error_code_desc(int code);
	void make_response_header(std::ostream& os,const char* retcodemsg,bool has_header_ending,int httpver_major = 1,int httpver_minor = 0);
	
}}}

#endif
