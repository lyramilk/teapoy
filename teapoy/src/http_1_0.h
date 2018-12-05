#ifndef _lyramilk_teapoy_http_1_0_h_
#define _lyramilk_teapoy_http_1_0_h_

#include "config.h"
#include "http_1_1.h"

namespace lyramilk{ namespace teapoy {

	class http_1_0:public http_1_1
	{
	  public:
		http_1_0(std::ostream& oss);
		virtual ~http_1_0();
		virtual void dtr();
	  public:
		virtual bool oninit(std::ostream& os);
		virtual bool onrequest(const char* cache,int size,std::ostream& os);
		virtual bool reset();
	  public:
		virtual bool send_header_with_chunk(httpresponse* response,lyramilk::data::uint32 code);
		virtual bool send_header_with_length(httpresponse* response,lyramilk::data::uint32 code,lyramilk::data::uint64 content_length);
	  public:
		static httpadapter_creater proto_info;
	};

}}

#endif
