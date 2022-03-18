/*
#include <libmilk/var.h>
#include <libmilk/netaio.h>
#include <libmilk/xml.h>
*/
#include "config.h"
#include <libmilk/scriptengine.h>
#include <libmilk/dict.h>
#include <libmilk/log.h>
#include "script.h"
#include <zlib.h>

namespace lyramilk{ namespace teapoy{
	static lyramilk::log::logss log(lyramilk::klog,"teapoy.native");

	bool gzip_encode(const std::string& src,lyramilk::data::chunk* bin)
	{
#ifdef ZLIB_FOUND
		z_stream strm = {0};
		strm.zalloc = NULL;
		strm.zfree = NULL;
		strm.opaque = NULL;
		if(deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED,MAX_WBITS + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK){
			return false;
		}

		lyramilk::data::istringstream ss(src);

		char buff[16384] = {0};
		while(ss){
			ss.read(buff,sizeof(buff));
			lyramilk::data::int64 gcount = ss.gcount();
			if(gcount >= 0){
				unsigned char buff_chunkbody[16384] = {0};
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
						if(bin) bin->append(buff_chunkbody,sz);
					}
				}while(strm.avail_out == 0);
				if(strm.avail_in > 0){
					ss.seekg(0 - strm.avail_in,std::istream::cur);
				}
			}else{
				break;
			}
		}
		deflateEnd(&strm);
		return true;
#else
		return false;
#endif
	}

	lyramilk::data::var sni_gz(const lyramilk::data::array& args,const lyramilk::data::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
		lyramilk::data::string str = args[0].str();
		lyramilk::data::chunk bin;
		bool b = gzip_encode(str,&bin);
		if(!b) return lyramilk::data::var::nil;
		return bin;
	}


	static int define(lyramilk::script::engine* p)
	{
		p->define("gzip",sni_gz);
		return 2;
	}


	static __attribute__ ((constructor)) void __init()
	{
		lyramilk::teapoy::script2native::instance()->regist("zlib",define);
	}

}}