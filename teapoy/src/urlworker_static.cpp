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
#include <zlib.h>

#include <unistd.h>
#include <string.h>

namespace lyramilk{ namespace teapoy { namespace web {
	class url_worker_static : public url_worker
	{
		bool onpost(session_info* si) const
		{
			//	POST只支持最基本的文件发送，不含缓存，分断下载，gzip压缩。
			//	获取文件本地路径
			lyramilk::data::string rawfile = si->real;

			std::size_t pos = rawfile.find("?");
			if(pos != rawfile.npos){
				rawfile = rawfile.substr(0,pos);
			}

			if(rawfile.find("/../") != rawfile.npos){
				si->rep.send_header_and_length("400 Bad Request",strlen("400 Bad Request"),0);
				return false;
			}

			struct stat st = {0};
			if(0 !=::stat(rawfile.c_str(),&st)){
				if(errno == ENOENT){
					si->rep.send_header_and_length("404 Not Found",strlen("404 Not Found"),0);
					return false;
				}
				if(errno == EACCES){
					si->rep.send_header_and_length("403 Forbidden",strlen("403 Forbidden"),0);
					return false;
				}
				if(errno == ENAMETOOLONG){
					si->rep.send_header_and_length("400 Bad Request",strlen("400 Bad Request"),0);
					return false;
				}
				si->rep.send_header_and_length("500 Internal Server Error",strlen("500 Internal Server Error"),0);
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
				if(mimetype.empty()){
					mimetype = "application/octet-stream";
				}
			}

			std::ifstream ifs;
			ifs.open(rawfile.c_str(),std::ifstream::binary|std::ifstream::in);
			if(!ifs.is_open()){
				si->rep.send_header_and_length("404 Not Found",strlen("404 Not Found"),0);
				return false;
			}


			lyramilk::data::uint64 datacount = st.st_size;
			si->rep.set("Content-Type",mimetype);
			si->rep.send_header_and_length("200 OK",strlen("200 OK"),datacount);

			char buff[65536];
			for(;ifs && datacount > 0;){
				ifs.read(buff,sizeof(buff));
				lyramilk::data::int64 gcount = ifs.gcount();
				if(gcount > 0){
					datacount -= gcount;
					si->rep.send_body(buff,gcount);
				}else{
					ifs.close();
					return true;
				}
			}
			ifs.close();
			return true;
		}

		bool onget(session_info* si) const
		{
			//	GET请求支持缓存、断点续传。,
			//	获取文件本地路径
			lyramilk::data::string rawfile = si->real;
			std::size_t pos = rawfile.find("?");
			if(pos != rawfile.npos){
				rawfile = rawfile.substr(0,pos);
			}

			if(rawfile.find("/../") != rawfile.npos){
				si->rep.send_header_and_length("400 Bad Request",strlen("400 Bad Request"),0);
				return false;
			}

			struct stat st = {0};
			if(0 !=::stat(rawfile.c_str(),&st)){
				if(errno == ENOENT){
					si->rep.send_header_and_length("404 Not Found",strlen("404 Not Found"),0);
					return false;
				}
				if(errno == EACCES){
					si->rep.send_header_and_length("403 Forbidden",strlen("403 Forbidden"),0);
					return false;
				}
				if(errno == ENAMETOOLONG){
					si->rep.send_header_and_length("400 Bad Request",strlen("400 Bad Request"),0);
					return false;
				}
				si->rep.send_header_and_length("500 Internal Server Error",strlen("500 Internal Server Error"),0);
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

			//现在 st是原始文件的信息。

			bool needcache = true;
			if(nocache){
				needcache = false;
			}else if(st.st_size < 1024){
				needcache = false;
			}

			lyramilk::data::string etag;
			if(needcache){
				//取得ETag
				{
					char buff_etag[100];
					long long unsigned filemtime = st.st_mtime;
					long long unsigned filesize = st.st_size;
					sprintf(buff_etag,"%llx-%llx",filemtime,filesize);
					etag = buff_etag;
				}

				lyramilk::data::var vifetagnotmatch = si->req->header->get("If-None-Match");
				if(vifetagnotmatch.type() != lyramilk::data::var::t_invalid && vifetagnotmatch == etag){

					si->rep.set("Cache-Control","max-age=3600,public");
					si->rep.send_header_and_length("304 Not Modified",strlen("304 Not Modified"),0);
					return true;
				}
			}

			lyramilk::data::string mimetype = mime::getmimetype_byname(rawfile);
			if(mimetype.empty()){
				mimetype = mime::getmimetype_byfile(rawfile);
				if(mimetype.empty()){
					mimetype = "application/octet-stream";
				}
			}
			enum {
				NO_GZIP,
				CACHED_GZIP,
				DYNAMIC_GZIP,
			}gzipmode = NO_GZIP;
#ifdef ZLIB_FOUND
			if(threshold > 0 && st.st_size > this->threshold){
				//支持gzip
				lyramilk::data::var v = si->req->header->get("Accept-Encoding");
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
								}else if(si->req->header->major <= 1 && si->req->header->minor < 1){
									gzipmode = NO_GZIP;
								}else{
									gzipmode = DYNAMIC_GZIP;
								}
							}else{
								gzipmode = CACHED_GZIP;
								rawfile = cachedgzip;
								::stat(rawfile.c_str(),&st);
							}
						}else if(si->req->header->major <= 1 && si->req->header->minor < 1){
							gzipmode = NO_GZIP;
						}else{
							gzipmode = DYNAMIC_GZIP;
						}
					}
				}
			}
#endif
			//此时 st有可能是gzip缓存文件的信息。
			lyramilk::data::uint64 filesize = st.st_size;
			lyramilk::data::uint64 datacount = filesize;
			lyramilk::data::int64 range_start = 0;
			lyramilk::data::int64 range_end = -1;
			bool is_range = false;

			if(gzipmode != DYNAMIC_GZIP)
			{
				//取得分段下载的范围
				lyramilk::data::var v = si->req->header->get("Range");
				bool has_range_start = false;
				bool has_range_end = false;
				if(v != lyramilk::data::var::nil){
					is_range = true;
					lyramilk::data::string rangstr = v.str();
					lyramilk::data::strings range;
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
						si->rep.send_header_and_length("500 Internal Server Error",strlen("500 Internal Server Error"),0);
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


			if(is_range){
				//判断分断下载是否因原文件修改而失效。
				lyramilk::data::var vifrange = si->req->header->get("If-Range");
				lyramilk::data::var vifmatch = si->req->header->get("If-Match");

				if(vifrange.type() != lyramilk::data::var::t_invalid && (vifrange != etag)){
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
			ss <<				"Content-Type: " << mimetype << "\r\n";

			if(gzipmode != DYNAMIC_GZIP){
				if(gzipmode != NO_GZIP){
					ss <<		"Content-Encoding: gzip\r\n";
				}
				ss <<			"Content-Length: " << datacount << "\r\nAccept-Ranges: bytes\r\n";
				if(is_range){
					ss <<		"Content-Range: " << range_start << "-" << range_end << "/" << filesize << "\r\n";
				}
			}else{
				ss <<			"Content-Encoding: gzip\r\nTransfer-Encoding: chunked\r\n";
			}

			if(nocache){
				ss <<			"Cache-Control: no-cache\r\npragma: no-cache\r\n";
			}else if(!etag.empty()){
				ss <<			"Cache-Control: max-age=3600,public\r\nETag: " << etag << "\r\n";
			}else{
				ss <<			"Cache-Control: max-age=3600,public\r\n";
			}
			ss <<				"Access-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: *\r\n\r\n";

			lyramilk::data::string strheader = ss.str();
			si->rep.send_body(strheader.c_str(),strheader.size());

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
						si->rep.send_body(buff,gcount);
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
								si->rep.send_chunk(buff_chunkbody,sz);
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
				si->rep.send_chunk_finish();
			}
			return true;
		}

		virtual bool call(session_info* si) const
		{
			url_worker_loger _("teapoy.web.s",si->req);
			if(si->req->header->method == "GET") return onget(si);
			if(si->req->header->method == "POST") return onpost(si);
			if(si->req->header->method == "HEAD") return onget(si);

			si->rep.set("Allow","GET,POST");
			si->rep.send_header_and_length("405 Method Not Allowed",strlen("405 Method Not Allowed"),0);
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
			return url_worker::init_extra(extra);
		}

		lyramilk::data::string compress_type;
		lyramilk::data::int64 threshold;
		bool usegzipcache;
		bool nocache;
		url_worker_static()
		{
			threshold = -1;
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
