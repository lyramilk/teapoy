//#define _FILE_OFFSET_BITS 64
//#define _LARGE_FILE
#include "web.h"
#include "stringutil.h"
#include "env.h"
#include <fstream>
#include <sys/stat.h>
#include <errno.h>
#include <libmilk/log.h>
#include <libmilk/multilanguage.h>
#include <libmilk/testing.h>
#include <pcre.h>
#include <zlib.h>
//#define NOSENDFILE

#include <unistd.h>

#ifndef NOSENDFILE
	#include <sys/sendfile.h>
	#include <fcntl.h>
	#include <string.h>
#endif

namespace lyramilk{ namespace teapoy { namespace web {
	class url_worker_static : public url_worker
	{
		bool onpost(lyramilk::teapoy::http::request* req,std::ostream& os,lyramilk::data::string real) const
		{
			//	POST只支持最基本的文件发送，不含缓存，分断下载，gzip压缩。
			//	获取文件本地路径
			lyramilk::data::string rawfile = real;

			std::size_t pos = rawfile.find("?");
			if(pos != rawfile.npos){
				rawfile = rawfile.substr(0,pos);
			}

			if(rawfile.find("/../") != rawfile.npos){
				lyramilk::teapoy::http::make_response_header(os,"400 Bad Request",true,req->ver.major,req->ver.minor);
				return false;
			}

			struct stat st = {0};
			if(0 !=::stat(rawfile.c_str(),&st)){
				if(errno == ENOENT){
					lyramilk::teapoy::http::make_response_header(os,"404 Not Found",true,req->ver.major,req->ver.minor);
					return false;
				}
				if(errno == EACCES){
					lyramilk::teapoy::http::make_response_header(os,"403 Forbidden",true,req->ver.major,req->ver.minor);
					return false;
				}
				if(errno == ENAMETOOLONG){
					lyramilk::teapoy::http::make_response_header(os,"400 Bad Request",true,req->ver.major,req->ver.minor);
					return false;
				}
				lyramilk::teapoy::http::make_response_header(os,"500 Internal Server Error",true,req->ver.major,req->ver.minor);
				return false;
			}

			if(st.st_mode&S_IFDIR){
				lyramilk::data::string rawdir;
				std::size_t pos_end = rawfile.find_last_not_of("/");
				rawdir = rawfile.substr(0,pos_end + 1);
				rawdir.push_back('/');
				lyramilk::data::var indexs = env::get_config("web.index");
				if(indexs.type() == lyramilk::data::var::t_array){
					lyramilk::data::var::array& ar = indexs;
					lyramilk::data::var::array::iterator it = ar.begin();
					for(;it!=ar.end();++it){
						rawfile = rawdir + it->str();
						if(0 == ::stat(rawfile.c_str(),&st) && !(st.st_mode&S_IFDIR)){
							break;
						}
					}
				}
			}

			lyramilk::data::string mimetype = mime::getmimetype_byname(rawfile);
			if(mimetype.empty()){
				mimetype = mime::getmimetype_byfile(rawfile);
			}

			lyramilk::data::uint64 datacount = st.st_size;

			lyramilk::data::stringstream ss;
			ss <<	"HTTP/1.1 200 OK\r\n";
			if(!mimetype.empty()){
				ss <<	"Content-Type: " << mimetype << "\r\n";
			}
			ss <<		"Server: " TEAPOY_VERSION "\r\n";
			ss <<		"Content-Length: " << datacount << "\r\n";
			ss <<		"Access-Control-Allow-Origin: *\r\n";
			ss <<		"Access-Control-Allow-Methods: *\r\n";
			ss <<		"\r\n";

			os << ss.str();
#ifdef NOSENDFILE
			std::ifstream ifs;
			ifs.open(rawfile.c_str(),std::ifstream::binary|std::ifstream::in);
			if(!ifs.is_open()){
				lyramilk::teapoy::http::make_response_header(os,"404 Not Found",true,req->ver.major,req->ver.minor);
				return false;
			}
			char buff[65536];
			for(;ifs && datacount > 0;){
				ifs.read(buff,sizeof(buff));
				lyramilk::data::int64 gcount = ifs.gcount();
				if(gcount > 0){
					datacount -= gcount;
					os.write(buff,gcount);
				}else{
					ifs.close();
					return false;
				}
			}
			ifs.close();
#else
			int filefd = open(rawfile.c_str(),O_RDONLY);
			if(filefd < 1){
				lyramilk::teapoy::http::make_response_header(os,"404 Not Found",true,req->ver.major,req->ver.minor);
				return false;
			}
			off_t ptroff = 0;
			while(datacount>0){
				size_t bc = 1*1024*1024*1024;
				if(bc > datacount){
					bc = datacount;
				}
				ssize_t sz = sendfile(req->fd,filefd,&ptroff,bc);
				if(sz == -1 && errno == EAGAIN){
					usleep(3000);
					continue;
				}
				if(sz > 0 && (lyramilk::data::uint64)sz <= datacount){
					datacount -= sz;
				}else{
					::close(filefd);
					return false;
				}
			}
			::close(filefd);
#endif
			return true;
		}

		bool onget(lyramilk::teapoy::http::request* req,std::ostream& os,lyramilk::data::string real) const
		{
			//	GET请求支持缓存、断点续传。,
			//	获取文件本地路径
			lyramilk::data::string rawfile = real;
			std::size_t pos = rawfile.find("?");
			if(pos != rawfile.npos){
				rawfile = rawfile.substr(0,pos);
			}

			if(rawfile.find("/../") != rawfile.npos){
				lyramilk::teapoy::http::make_response_header(os,"400 Bad Request",true,req->ver.major,req->ver.minor);
				return false;
			}

			struct stat st = {0};
			if(0 !=::stat(rawfile.c_str(),&st)){
				if(errno == ENOENT){
					lyramilk::teapoy::http::make_response_header(os,"404 Not Found",true,req->ver.major,req->ver.minor);
					return false;
				}
				if(errno == EACCES){
					lyramilk::teapoy::http::make_response_header(os,"403 Forbidden",true,req->ver.major,req->ver.minor);
					return false;
				}
				if(errno == ENAMETOOLONG){
					lyramilk::teapoy::http::make_response_header(os,"400 Bad Request",true,req->ver.major,req->ver.minor);
					return false;
				}
				lyramilk::teapoy::http::make_response_header(os,"500 Internal Server Error",true,req->ver.major,req->ver.minor);
				return false;
			}

			if(st.st_mode&S_IFDIR){
				lyramilk::data::string rawdir;
				std::size_t pos_end = rawfile.find_last_not_of("/");
				rawdir = rawfile.substr(0,pos_end + 1);
				rawdir.push_back('/');
				lyramilk::data::var indexs = env::get_config("web.index");
				if(indexs.type() == lyramilk::data::var::t_array){
					lyramilk::data::var::array& ar = indexs;
					lyramilk::data::var::array::iterator it = ar.begin();
					for(;it!=ar.end();++it){
						rawfile = rawdir + it->str();
						if(0 == ::stat(rawfile.c_str(),&st) && !(st.st_mode&S_IFDIR)){
							break;
						}
					}
				}
			}
			lyramilk::data::string mimetype = mime::getmimetype_byname(rawfile);
			if(mimetype.empty()){
				mimetype = mime::getmimetype_byfile(rawfile);
			}
			enum {
				NO_GZIP,
				CACHED_GZIP,
				DYNAMIC_GZIP,
			}gzipmode = NO_GZIP;
#ifdef ZLIB_FOUND
			if(st.st_size > this->threshold){
				//支持gzip
				lyramilk::data::var v = req->get("Accept-Encoding");
				if(v.type() == lyramilk::data::var::t_str){
					lyramilk::data::string stracc = v.str();
					if(stracc.find(compress_type)!= stracc.npos){
						if(this->usegzipcache){
							lyramilk::data::string cachedgzip = rawfile + ".gzipcache";

							if(::stat(cachedgzip.c_str(),&st) != 0){
								std::ofstream ofs(cachedgzip.c_str(),std::ofstream::binary|std::ofstream::out);
								if(ofs.is_open()){
									z_stream strm = {0};
									strm.zalloc = NULL;
									strm.zfree = NULL;
									strm.opaque = NULL;
									if(deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED,MAX_WBITS + 16, 8, Z_DEFAULT_STRATEGY) == Z_OK){
										std::ifstream ifs;
										ifs.open(rawfile.c_str(),std::ifstream::binary|std::ifstream::in);
										if(ifs.is_open()){
											char buff[16384] = {0};
											while(ifs){
												ifs.read(buff,sizeof(buff));
												lyramilk::data::int64 gcount = ifs.gcount();
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
													unsigned int sz = sizeof(buff_chunkbody) - strm.avail_out;
													if(sz > 0){
														ofs.write(buff_chunkbody,sz);
													}
												}while(strm.avail_out == 0);
												if(strm.avail_in > 0){
													ifs.seekg(0 - strm.avail_in,std::ifstream::cur);
												}
											}
											ifs.close();
											ofs.close();

											gzipmode = CACHED_GZIP;
											rawfile = cachedgzip;
											::stat(rawfile.c_str(),&st);
										}
										deflateEnd(&strm);
									}
								}else if(req->ver.major <= 1 && req->ver.minor < 1){
									gzipmode = NO_GZIP;
								}else{
									gzipmode = DYNAMIC_GZIP;
								}
							}else{
								gzipmode = CACHED_GZIP;
								rawfile = cachedgzip;
								::stat(rawfile.c_str(),&st);
							}
						}else if(req->ver.major <= 1 && req->ver.minor < 1){
							gzipmode = NO_GZIP;
						}else{
							gzipmode = DYNAMIC_GZIP;
						}
					}
				}
			}
#endif
			lyramilk::data::uint64 filesize = st.st_size;
			lyramilk::data::uint64 datacount = filesize;
			lyramilk::data::int64 range_start = 0;
			lyramilk::data::int64 range_end = -1;
			bool is_range = false;

			if(gzipmode != DYNAMIC_GZIP)
			{
				//取得分段下载的范围
				lyramilk::data::var v = req->get("Range");
				bool has_range_start = false;
				bool has_range_end = false;
				if(v != lyramilk::data::var::nil){
					is_range = true;
					lyramilk::data::string rangstr = v.str();
					lyramilk::teapoy::strings range;
					if(rangstr.compare(0,6,"bytes=") == 0){
						range = lyramilk::teapoy::split(rangstr.substr(6),"-");
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
						lyramilk::teapoy::http::make_response_header(os,"500 Internal Server Error",true,req->ver.major,req->ver.minor);
						return false;
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

			bool needcache = true;
			if(nocache){
				needcache = false;
			}else if(datacount < 1024){
				needcache = false;
			}

			lyramilk::data::string lastmodified;
			lyramilk::data::string etag;
			if(needcache){
				//取得上次修改时间和ETag
				lyramilk::data::var vifmodified = req->get("If-Modified-Since");
				lyramilk::data::var vifetagnotmatch = req->get("If-None-Match");
				{
					{
						struct tm* gmt_mtime = gmtime(&st.st_mtime);
						char buff_time[100];
						strftime(buff_time,sizeof(buff_time),"%a, %0e %h %Y %T GMT",gmt_mtime);
						lastmodified = buff_time;
					}
					{
						char buff_etag[100];
						sprintf(buff_etag,"%lx-%llx",st.st_mtime,filesize);
						etag = buff_etag;
					}
				}

				if(vifetagnotmatch.type() != lyramilk::data::var::t_invalid && vifetagnotmatch == etag){
					lyramilk::teapoy::http::make_response_header(os,"304 Not Modified",false,req->ver.major,req->ver.minor);
					os << "Cache-Control: max-age=3600,public\r\n\r\n";
					return true;
				}

				if(vifmodified.type() != lyramilk::data::var::t_invalid && vifmodified == lastmodified){
					lyramilk::teapoy::http::make_response_header(os,"304 Not Modified",false,req->ver.major,req->ver.minor);
					os << "Cache-Control: max-age=3600,public\r\n\r\n";
					return true;
				}
			}


			if(is_range){
				//判断分断下载是否因原文件修改而失效。
				lyramilk::data::var vifrange = req->get("If-Range");
				lyramilk::data::var vifunmodified = req->get("If-Unmodified-Since");
				lyramilk::data::var vifmatch = req->get("If-Match");

				if(vifrange.type() != lyramilk::data::var::t_invalid && (vifrange != etag) && (vifrange != lastmodified)){
					//Range失效
					is_range = false;
				}

				if(vifunmodified.type() != lyramilk::data::var::t_invalid && vifunmodified != lastmodified){
					//Range失效
					is_range = false;
				}

				if(vifmatch.type() != lyramilk::data::var::t_invalid && vifmatch != etag){
					//Range失效
					is_range = false;
				}

			}
			lyramilk::data::stringstream ss;
			if(is_range){
				ss <<			"HTTP/1.1 206 Partial Content\r\nServer: " TEAPOY_VERSION "\r\n";
			}else{
				ss <<			"HTTP/1.1 200 OK\r\nServer: " TEAPOY_VERSION "\r\n";
			}
			if(!mimetype.empty()){
				ss <<			"Content-Type: " << mimetype << "\r\n";
			}

			if(gzipmode != DYNAMIC_GZIP){
				if(gzipmode != NO_GZIP){
					ss <<		"Content-Encoding: gzip\r\n";
				}
				ss <<			"Accept-Ranges: bytes\r\n";
				if(needcache){
					time_t t_now = time(nullptr);
					struct tm* gmt_now = gmtime(&t_now);
					char buff_time[100] = {0};
					strftime(buff_time,sizeof(buff_time),"%a, %0e %h %Y %T GMT",gmt_now);
					ss <<		"Date: " << buff_time << "\r\n";

					//建议客户端缓存3600秒。
					t_now += 3600;
					struct tm* gmt_expires = gmtime(&t_now);
					char buff_time2[100] = {0};
					strftime(buff_time2,sizeof(buff_time2),"%a, %0e %h %Y %T GMT",gmt_expires);

					ss <<		"Expires: "<< buff_time2 << "\r\nCache-Control: max-age=3600,public\r\n";

					if(!lastmodified.empty()){
						ss <<	"Last-Modified: " << lastmodified << "\r\n";
					}
					if(!etag.empty()){
						ss <<	"ETag: " << etag << "\r\n";
					}
				}else if(nocache){
					ss <<		"Cache-Control: no-cache\r\npragma: no-cache\r\n";
				}else{
					ss <<		"Cache-Control: max-age=3600,public\r\n";
				}
				ss <<			"Content-Length: " << datacount << "\r\n";
				if(is_range){
					ss <<		"Content-Range: " << range_start << "-" << range_end << "/" << filesize << "\r\n";
				}
			}else{
				ss <<			"Content-Encoding: gzip\r\nTransfer-Encoding: chunked\r\n";
			}
			ss <<				"Access-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: *\r\n\r\n";

			os << ss.str();
#ifndef NOSENDFILE
			if(gzipmode != DYNAMIC_GZIP){
				int filefd = open(rawfile.c_str(),O_RDONLY);
				if(filefd < 1){
					return false;
				}
				off_t ptroff = range_start;
				while(datacount>0){
					size_t bc = 1*1024*1024*1024;
					if(bc > datacount){
						bc = datacount;
					}
					ssize_t sz = sendfile(req->fd,filefd,&ptroff,bc);
					if(sz == -1 && errno == EAGAIN){
						usleep(3000);
						continue;
					}
					if(sz > 0 && (lyramilk::data::uint64)sz <= datacount){
						datacount -= sz;
					}else{
						::close(filefd);
						return false;
					}
				}
				::close(filefd);
				return true;
			}
#endif
			if(gzipmode != DYNAMIC_GZIP){
				std::ifstream ifs;
				ifs.open(rawfile.c_str(),std::ifstream::binary|std::ifstream::in);
				if(!ifs.is_open()){
					return false;
				}
				if(is_range){
					ifs.seekg(range_start,std::ifstream::beg);
				}
				char buff[65536];
				for(;ifs && datacount > 0;){
					ifs.read(buff,sizeof(buff));
					lyramilk::data::int64 gcount = ifs.gcount();
					if(gcount > 0){
						datacount -= gcount;
						os.write(buff,gcount);
					}else{
						ifs.close();
						return false;
					}
				}
				ifs.close();
			}else{
				z_stream strm = {0};
				strm.zalloc = NULL;
				strm.zfree = NULL;
				strm.opaque = NULL;
				if(deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED,MAX_WBITS + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK){
					return false;
				}

				std::ifstream ifs;
				ifs.open(rawfile.c_str(),std::ifstream::binary|std::ifstream::in);
				if(!ifs.is_open()){
					deflateEnd(&strm);
					return false;
				}
				if(is_range){
					ifs.seekg(range_start,std::ifstream::beg);
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
							unsigned int sz = sizeof(buff_chunkbody) - strm.avail_out;
							if(sz > 0){
								char buff_chunkheader[30];
								unsigned int szh = sprintf(buff_chunkheader,"%x\r\n",sz);
								os.write(buff_chunkheader,szh);
								os.write(buff_chunkbody,sz);
								os.write("\r\n",2);
							}
						}while(strm.avail_out == 0);
						if(strm.avail_in > 0){
							ifs.seekg(0 - strm.avail_in,std::ifstream::cur);
						}
					}else{
						ifs.close();
						deflateEnd(&strm);
						return false;
					}
					//deflateReset(&strm);
				}
				ifs.close();
				deflateEnd(&strm);

				lyramilk::data::string str = "0\r\n\r\n";
				os.write(str.c_str(),str.size());
			}
			return true;
		}

		virtual bool call(lyramilk::teapoy::http::request* req,std::ostream& os,lyramilk::data::string real,website_worker& w) const
		{
			if(req->method == "GET") return onget(req,os,real);
			if(req->method == "POST") return onpost(req,os,real);
			if(req->method == "HEAD") return onget(req,os,real);
			lyramilk::teapoy::http::make_response_header(os,"405 Method Not Allowed",false,req->ver.major,req->ver.minor);
			os << "Allow: GET,POST\r\n\r\n";
			return false;
		}

		virtual bool init_extra(const lyramilk::data::var& extra)
		{
			const lyramilk::data::var& compress = extra["compress"];

			if(compress.type() == lyramilk::data::var::t_map){
				const lyramilk::data::var& crawtype = compress["type"];
				if(crawtype.type_like(lyramilk::data::var::t_str)){
					compress_type = crawtype.str();
				}else{
					compress_type = "gzip";
				}

				const lyramilk::data::var& crawthreshold = compress["threshold"];
				if(crawthreshold.type_like(lyramilk::data::var::t_int)){
					threshold = crawthreshold;
				}

				const lyramilk::data::var& crawcache = compress["cache"];
				if(crawcache.type_like(lyramilk::data::var::t_int)){
					usegzipcache = crawcache;
				}
			}
			const lyramilk::data::var& nocache = extra["nocache"];
			if(nocache.type_like(lyramilk::data::var::t_bool)){
				this->nocache = nocache;
			}
			return true;
		}

		lyramilk::data::string compress_type;
		lyramilk::data::int64 threshold;
		bool usegzipcache;
		bool nocache;
		url_worker_static()
		{
			threshold = 0;
			usegzipcache = false;
			nocache = false;
		}
		virtual ~url_worker_static()
		{
		}
	  public:
		static url_worker* ctr(void*)
		{
			return new url_worker_static();
		}

		static void dtr(url_worker* p)
		{
			delete (url_worker_static*)p;
		}

	};



	static __attribute__ ((constructor)) void __init()
	{
		url_worker_master::instance()->define("static",url_worker_static::ctr,url_worker_static::dtr);
	}
}}}
