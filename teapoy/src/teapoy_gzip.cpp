#include "teapoy_gzip.h"
#ifdef ZLIB_FOUND
	#include <zlib.h>
#endif

namespace lyramilk{ namespace teapoy
{
	bool http_chunked_gzip(std::istream& ifs,lyramilk::data::int64 datacount,httpadapter* adapter)
	{
#ifdef ZLIB_FOUND
		z_stream strm = {0};
		strm.zalloc = NULL;
		strm.zfree = NULL;
		strm.opaque = NULL;
		if(deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED,MAX_WBITS + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK){
			adapter->send_header_with_length(nullptr,500,0);
			return false;
		}

		char buff[16384] = {0};
		for(;ifs && datacount > 0;){
			ifs.read(buff,sizeof(buff));
			lyramilk::data::int64 gcount = ifs.gcount();
			if(gcount >= 0){
				datacount -= gcount;

				char buff_chunkbody[16384] = {0};
				strm.next_in = (Bytef*)buff;
				strm.avail_in = gcount;
				do{
					strm.next_out = (Bytef*)buff_chunkbody;
					strm.avail_out = sizeof(buff_chunkbody);
					if(gcount < (lyramilk::data::int64)sizeof(buff)){
						deflate(&strm, Z_FINISH);
					}else{
						deflate(&strm, Z_NO_FLUSH);
					}
					int sz = sizeof(buff_chunkbody) - strm.avail_out;
					if(sz > 0){
						adapter->send_chunk(adapter->response,buff_chunkbody,sz);
					}
				}while(strm.avail_out == 0);
				if(strm.avail_in > 0){
					ifs.seekg(0 - strm.avail_in,std::istream::cur);
				}
			}else{
				break;
			}
			//deflateReset(&strm);
		}
		deflateEnd(&strm);
		adapter->send_chunk_finish(adapter->response);
		return true;
#else
		adapter->send_header_with_length(nullptr,200,0);
#endif
	}
}}

