#ifndef _lyramilk_teapoy_gzip_h_
#define _lyramilk_teapoy_gzip_h_
#include "webservice.h"

namespace lyramilk{ namespace teapoy
{
	bool http_chunked_gzip(std::istream& is,lyramilk::data::int64 datacount,httpadapter* adapter);
	
}}
#endif
