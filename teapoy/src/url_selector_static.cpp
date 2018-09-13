#include "url_dispatcher.h"
#include "stringutil.h"
#include "mimetype.h"
#include <libmilk/multilanguage.h>
#include <sys/stat.h>
#include <errno.h>
#include "teapoy_gzip.h"

#include <fstream>

namespace lyramilk{ namespace teapoy {

	class url_selector_static:public url_regex_selector
	{
		bool nocache;
		bool nogzip;
		lyramilk::data::int64 threshold;
		lyramilk::data::string compress_type;
	  public:
		url_selector_static()
		{
			nocache = false;
			nogzip = true;
			threshold = 1;
			compress_type = "gzip";
		}

		virtual ~url_selector_static()
		{}

		virtual bool call(httprequest* request,httpresponse* response,httpadapter* adapter,const lyramilk::data::string& real)
		{
			vcall(request,response,adapter,real);
			return true;
		}

		void vcall(httprequest* request,httpresponse* response,httpadapter* adapter,const lyramilk::data::string& real)
		{
			if(real.find("/../") != real.npos){
				adapter->send_header_with_length(nullptr,404,0);
				return ;
			}

			struct stat st = {0};
			if(0 !=::stat(real.c_str(),&st)){
				if(errno == ENOENT){
					adapter->send_header_with_length(nullptr,404,0);
					return ;
				}
				if(errno == EACCES){
					adapter->send_header_with_length(nullptr,403,0);
					return ;
				}
				if(errno == ENAMETOOLONG){
					adapter->send_header_with_length(nullptr,400,0);
					return ;
				}
				adapter->send_header_with_length(nullptr,500,0);
				return ;
			}

			if(st.st_mode&S_IFDIR){
				adapter->send_header_with_length(nullptr,404,0);
				return ;
			}

			bool usecache = true;
			if(nocache){
				usecache = false;
			/*}else if(st.st_size < 1024){
				usecache = false;*/
			}
			lyramilk::data::string etag;
			//	取得 ETag
			if(usecache){
				{
					char buff_etag[100];
					long long unsigned filemtime = st.st_mtime;
					long long unsigned filesize = st.st_size;
					sprintf(buff_etag,"%llx-%llx",filemtime,filesize);
					etag = buff_etag;
				}

				lyramilk::data::string sifetagnotmatch = request->get("If-None-Match");
				if(sifetagnotmatch == etag){
					response->set("Cache-Control","max-age=3600,public");
					adapter->send_header_with_length(response,304,0);
					return ;
				}
			}

			lyramilk::data::string mimetype = mime::getmimetype_byname(real);
			//	取得 Content-Type
			if(mimetype.empty()){
				mimetype = mime::getmimetype_byfile(real);
				if(mimetype.empty()){
					mimetype = "application/octet-stream";
				}
			}

			// gzip支持
			enum {
				NO_GZIP,
				CHUNKED_GZIP,
			}gzipmode = NO_GZIP;


#ifdef ZLIB_FOUND
			lyramilk::data::string sacceptencoding = request->get("Accept-Encoding");
			if(sacceptencoding.find(compress_type) != sacceptencoding.npos && (!nogzip) && threshold > 0 && st.st_size > threshold){
				gzipmode = CHUNKED_GZIP;
			}
#endif
			// range支持
			lyramilk::data::uint64 filesize = st.st_size;
			lyramilk::data::uint64 datacount = filesize;
			lyramilk::data::int64 range_start = 0;
			lyramilk::data::int64 range_end = -1;
			bool is_range = false;
			if(gzipmode != CHUNKED_GZIP)
			{
				//取得分段下载的范围
				lyramilk::data::string srange = request->get("Range");
				bool has_range_start = false;
				bool has_range_end = false;
				if(!srange.empty()){
					is_range = true;
					lyramilk::data::strings range;
					if(srange.compare(0,6,"bytes=") == 0){
						range = lyramilk::teapoy::split(srange.substr(6),"-");
					}
					if(range.size() > 0 && !range[0].empty()){
						range_start = lyramilk::data::var(range[0]);
						has_range_start = true;
					}

					if(range.size() > 1 && !range[1].empty()){
						range_end = lyramilk::data::var(range[1]);
						has_range_end = true;
					}
					if(!has_range_start && !has_range_end){
						is_range = false;
					}

					if(!has_range_start){
						//最后range_end个字节
						range_start = filesize - range_end;
						range_end = filesize;
					}
					if(!has_range_end){
						//从range_start到最后
						range_end = filesize;
					}
					datacount = range_end-range_start;
				}
			}
			if(is_range){
				//判断分断下载是否因原文件修改而失效。
				lyramilk::data::string sifrange = request->get("If-Range");
				lyramilk::data::string sifmatch = request->get("If-Match");

				if(sifrange != etag){
					//Range失效
					is_range = false;
				}

				if(sifmatch != etag){
					//Range失效
					is_range = false;
				}
			}

			//准备发送响应头
			response->set("Content-Type",mimetype);
			if(gzipmode == NO_GZIP){
				//lyramilk::data::multilanguage::dict::format("%llu",datacount):
				if(is_range){
					{
						char cr[1024];
						int icr = snprintf(cr,sizeof(cr),"%lld-%lld/%llu",range_start,range_end,filesize);
						if(icr < 1){
							adapter->send_header_with_length(nullptr,500,0);
							return ;
						}
						response->set("Content-Range",cr);
					}
					response->set("Accept-Ranges","bytes");
				}
			}else{
				response->set("Content-Encoding","gzip");
			}

			if(nocache){
				response->set("Cache-Control","no-cache");
				response->set("pragma","no-cache");
			}else{
				response->set("Cache-Control","max-age=3600,public");
				if(!etag.empty()){
					response->set("ETag",etag);
				}
			}

			if(gzipmode == CHUNKED_GZIP){
				adapter->send_header_with_chunk(response,is_range?206:200);
			}else{
				adapter->send_header_with_length(response,is_range?206:200,datacount);
			}
			if(gzipmode != CHUNKED_GZIP){
				std::ifstream ifs;
				ifs.open(real.c_str(),std::ifstream::binary|std::ifstream::in);
				if(!ifs.is_open()){
					return ;
				}
				if(is_range){
					ifs.seekg(range_start,std::ifstream::beg);
				}
				char buff[16384];
				for(;ifs && datacount > 0;){
					ifs.read(buff,sizeof(buff));
					lyramilk::data::int64 gcount = ifs.gcount();
					if(gcount > 0){
						datacount -= gcount;
						adapter->send_bodydata(response,buff,gcount);
					}else{
						break;
					}
				}
				ifs.close();
			}else if(gzipmode == CHUNKED_GZIP){
				std::ifstream ifs;
				ifs.open(real.c_str(),std::ifstream::binary|std::ifstream::in);
				if(!ifs.is_open()){
					adapter->send_header_with_length(nullptr,500,0);
					return ;
				}

				http_chunked_gzip(ifs,datacount,adapter);
			}
		}



		static url_selector* ctr(void*)
		{
			return new url_selector_static;
		}

		static void dtr(url_selector* p)
		{
			delete (url_selector_static*)p;
		}
	};



	static __attribute__ ((constructor)) void __init()
	{
		url_selector_factory::instance()->define("static",url_selector_static::ctr,url_selector_static::dtr);
	}

}}
