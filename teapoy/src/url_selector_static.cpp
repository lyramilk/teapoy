#include "url_dispatcher.h"
#include "stringutil.h"
#include "mimetype.h"
#include <libmilk/dict.h>
#include <libmilk/stringutil.h>
#include <sys/stat.h>
#include <errno.h>
#include "teapoy_gzip.h"
#include "util.h"

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

		virtual dispatcher_check_status call(httprequest* request,httpresponse* response,httpadapter* adapter,const lyramilk::data::string& real)
		{
			url_selector_loger _("teapoy.web.static",adapter);
			vcall(request,response,adapter,path_simplify(real));
			return cs_ok;
		}

		void vcall(httprequest* request,httpresponse* response,httpadapter* adapter,const lyramilk::data::string& real)
		{
			struct stat st = {0};
			if(0 !=::stat(real.c_str(),&st)){
				if(errno == ENOENT){
					adapter->response->code = 404;
				}else if(errno == EACCES){
					adapter->response->code = 403;
				}else if(errno == ENAMETOOLONG){
					adapter->response->code = 400;
				}
				return ;
			}

			if(st.st_mode&S_IFDIR){
				adapter->response->code = 404;
				return ;
			}

			bool usecache = true;
			if(nocache){
				usecache = false;
			}else if(st.st_size < threshold){
				usecache = false;
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

			}

			if(usecache){
				lyramilk::data::string sifetagnotmatch = request->get("If-None-Match");
				if(sifetagnotmatch == etag){
					response->set("Cache-Control","max-age=3600,public");
					adapter->response->code = 304;
					return ;
				}
			}

			lyramilk::data::string mimetype = mimetype::getmimetype_byname(real);
			//	取得 Content-Type
			if(mimetype.empty()){
				mimetype = mimetype::getmimetype_byfile(real);
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
			if(adapter->allow_gzip() && adapter->allow_chunk()){
				lyramilk::data::string sacceptencoding = request->get("Accept-Encoding");
				if(sacceptencoding.find(compress_type) != sacceptencoding.npos && (!nogzip) && threshold > 0 && st.st_size > threshold){
					gzipmode = CHUNKED_GZIP;
				}
			}
#endif
			// range支持
			lyramilk::data::uint64 filesize = st.st_size;
			lyramilk::data::int64 datacount = filesize;

			std::map<lyramilk::data::int64,lyramilk::data::int64> range_map;

			if(gzipmode != CHUNKED_GZIP)
			{
				lyramilk::data::string sifrange = request->get("If-Range");
				lyramilk::data::string sifmatch = request->get("If-Match");

				do{
					if(!sifrange.empty() && sifrange != etag){
						//Range失效
						break;
					}

					if(!sifmatch.empty() && sifmatch != etag){
						//Range失效
						break;
					}


					//取得分段下载的范围
					lyramilk::data::string srange = request->get("Range");
					if(!srange.empty()){
						if(srange.compare(0,6,"bytes=") == 0){
							lyramilk::data::string sranges = srange.substr(6);
							lyramilk::data::strings ranges = lyramilk::data::split(sranges,",");
							lyramilk::data::strings::iterator it = ranges.begin();
							for(;it!=ranges.end();++it){
								lyramilk::data::strings range = lyramilk::data::split(*it,"-");
								if(range.size() > 1){
									if(range[0].empty()){
										char* p;
										lyramilk::data::int64 s = 0 - strtoll(range[1].c_str(),&p,10);
										s += filesize;
										range_map[s] = s;
										continue;
									}
									if(range[1].empty()){
										range[1] = "-1";
									}
									char* p;
									lyramilk::data::int64 s = strtoll(range[0].c_str(),&p,10);
									lyramilk::data::int64 t = strtoll(range[1].c_str(),&p,10);
									if(s < 0){
										s += filesize;
									}
									if(t < 0){
										t += filesize;
									}

									range_map[s] = t;
								}else if(range.size() == 1){
									char* p;
									lyramilk::data::int64 s = strtoll(range[0].c_str(),&p,10);
									if(s < 0){
										s += filesize;
									}
									range_map[s] = s;
								}
							}
						}

					}
				}while(false);
			}

			bool is_range = false;
			std::map<lyramilk::data::int64,lyramilk::data::int64>::iterator range_it = range_map.begin();
			if(range_it!=range_map.end()){
				//多分段处理起来比较麻烦，只处理第一分段。
				is_range = true;
				datacount = std::min(range_it->second + 1,(lyramilk::data::int64)filesize) - range_it->first;
			}

			//准备发送响应头
			response->set("Content-Type",mimetype);
			if(gzipmode == NO_GZIP){
				//lyramilk::data::multilanguage::dict::format("%llu",datacount):
				if(is_range){
					char cr[1024];
					int icr = snprintf(cr,sizeof(cr),"bytes %lld-%lld/%llu",range_it->first,std::min(range_it->second,(lyramilk::data::int64)filesize - 1),filesize);
					if(icr < 1){
						adapter->response->code = 500;
						return ;
					}
					response->set("Content-Range",cr);
				}else{
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

			adapter->response->code = is_range?206:200;
			if(gzipmode == NO_GZIP){
				adapter->response->content_length = datacount;
				std::ifstream ifs;
				ifs.open(real.c_str(),std::ifstream::binary|std::ifstream::in);
				if(!ifs.is_open()){
					adapter->response->code = 403;
					return ;
				}
				if(is_range){
					ifs.seekg(range_it->first,std::ifstream::beg);
				}
				char buff[2048];
				for(;ifs && datacount > 0;){
					ifs.read(buff,std::min(sizeof(buff),(std::size_t)datacount));
					lyramilk::data::int64 gcount = ifs.gcount();
					if(gcount > 0){
						datacount -= gcount;
						adapter->send_data(buff,gcount);
					}else{
						break;
					}
				}
				ifs.close();
			}else if(gzipmode == CHUNKED_GZIP){
				std::ifstream ifs;
				ifs.open(real.c_str(),std::ifstream::binary|std::ifstream::in);
				if(!ifs.is_open()){
					adapter->response->code = 500;
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
