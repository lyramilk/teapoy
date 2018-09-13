#ifndef _lyramilk_teapoy_http_2_0_h_
#define _lyramilk_teapoy_http_2_0_h_

#include "config.h"
#include "webservice.h"
#include "http_2_0_hpack.h"
#include <nghttp2/nghttp2.h>

#ifdef OPENSSL_FOUND
	#include <openssl/ssl.h>
#endif

namespace lyramilk{ namespace teapoy {

	class http_2_0:public httpadapter
	{
	  protected:
		hpack hp;
	  public:
		http_2_0(std::ostream& oss);
		virtual ~http_2_0();

		virtual void dtr();

		virtual bool oninit(std::ostream& os);
		virtual bool onrequest(const char* cache,int size,std::ostream& os);
		virtual bool call();

		//virtual bool onh2request(const char* cache,int size,std::ostream& os);

		virtual bool send_bodydata(httpresponse* response,const char* p,lyramilk::data::uint32 l);
		virtual bool send_header_with_chunk(httpresponse* response,lyramilk::data::uint32 code);
		virtual bool send_header_with_length(httpresponse* response,lyramilk::data::uint32 code,lyramilk::data::uint64 content_length);
	  protected:
		fcache* request_cache;
		std::stringstream oss;
		std::string recvcache;
		lyramilk::data::uint64 stream_id;
	  public:
		static httpadapter_creater proto_info;
	 
	};

}}

#endif
