#ifndef _lyramilk_teapoy_http_2_0_h_
#define _lyramilk_teapoy_http_2_0_h_

#include "config.h"
#include "webservice.h"
#include <nghttp2/nghttp2.h>

#ifdef OPENSSL_FOUND
	#include <openssl/ssl.h>
#endif

namespace lyramilk{ namespace teapoy {

	class http_2_0:public httpchannel
	{
	  protected:
	  public:
		http_2_0();
		virtual ~http_2_0();

		virtual void dtr();

		virtual bool oninit(std::ostream& os);
		virtual bool onrequest(const char* cache,int size,std::ostream& os);

	  public:
		std::stringstream oss;
	  public:
		static httpchannel_creater proto_info;
	 
	};

}}

#endif
