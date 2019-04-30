#ifndef _lyramilk_teapoy_http_1_1_h_
#define _lyramilk_teapoy_http_1_1_h_

#include "config.h"
#include "webservice.h"

namespace lyramilk{ namespace teapoy {

	class http_1_1:public httpadapter
	{
		std::string headercache;
	  public:
		http_1_1(std::ostream& oss);
		virtual ~http_1_1();

		virtual void dtr();

		virtual bool oninit(std::ostream& os);
		virtual bool onrequest(const char* cache,int size,std::ostream& os);
		virtual bool reset();
	  public:
		virtual send_status send_header();
		virtual bool send_data(const char* p,lyramilk::data::uint32 l);
		virtual void send_finish();
	  public:
		virtual bool allow_gzip();
		virtual bool allow_chunk();
		virtual bool allow_cached();
	  public:
		static httpadapter_creater proto_info;
	};

}}

#endif
