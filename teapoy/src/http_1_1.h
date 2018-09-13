#ifndef _lyramilk_teapoy_http_1_1_h_
#define _lyramilk_teapoy_http_1_1_h_

#include "config.h"
#include "webservice.h"

namespace lyramilk{ namespace teapoy {


	enum parse_result{
		pr_ok,
		pr_error,
		pr_continue,
	};

	enum parse_process{
		pp_head,
		pp_error,
		pp_chunked,
		pp_lengthed,
	};


	class http_1_1:public httpadapter
	{
		std::string headercache;
	  public:
		http_1_1(std::ostream& oss);
		virtual ~http_1_1();

		virtual void dtr();

		virtual bool oninit(std::ostream& os);
		virtual bool onrequest(const char* cache,int size,std::ostream& os);
	  public:
		virtual bool send_header_with_chunk(httpresponse* response,lyramilk::data::uint32 code);
		virtual bool send_header_with_length(httpresponse* response,lyramilk::data::uint32 code,lyramilk::data::uint64 content_length);
	  protected:
		fcache* request_cache;
		parse_process pps;
	  	std::size_t body_offset;
	  	std::size_t body_writep_offset;
	  	std::size_t content_length;
	  	std::size_t data_size;
		virtual parse_result parse_request(const char* cache,int size,int* surplus);
	  public:
		static httpadapter_creater proto_info;
	};

}}

#endif
