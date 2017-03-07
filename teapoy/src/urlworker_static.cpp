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
//#define NOSENDIFLE

#include <unistd.h>

#ifndef NOSENDIFLE
	#include <sys/sendfile.h>
	#include <fcntl.h>
	#include <string.h>
#endif

namespace lyramilk{ namespace teapoy { namespace web {
	class url_worker_static : public url_worker
	{
		bool onpost(lyramilk::teapoy::http::request* req,std::ostream& os,lyramilk::data::string real) const
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

			lyramilk::data::var v = req->get("Range");
			lyramilk::data::int64 range_start = 0;
			lyramilk::data::int64 range_end = -1;
			bool is_range = false;
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
			}

			lyramilk::data::uint64 filesize = st.st_size;
			lyramilk::data::uint64 datacount = filesize;

			if(is_range){
				//b
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
				//e
				datacount = range_end-range_start;
			}

			lyramilk::data::var vifmodified = req->get("If-Modified-Since");
			lyramilk::data::var vifetagnotmatch = req->get("If-None-Match");

			lyramilk::data::string lastmodified;
			lyramilk::data::string etag;

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
				lyramilk::teapoy::http::make_response_header(os,"304 Not Modified",true,req->ver.major,req->ver.minor);
				return true;
			}

			if(vifmodified.type() != lyramilk::data::var::t_invalid && vifmodified == lastmodified){
				lyramilk::teapoy::http::make_response_header(os,"304 Not Modified",true,req->ver.major,req->ver.minor);
				return true;
			}


			if(is_range){
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

			lyramilk::data::string mimetype = mime::getmimetype_byname(rawfile);
			if(mimetype.empty()){
				mimetype = mime::getmimetype_byfile(rawfile);
			}

			lyramilk::data::stringstream ss;
			if(is_range){
				ss <<	"HTTP/1.1 206 Partial Content\r\n";
			}else{
				ss <<	"HTTP/1.1 200 OK\r\n";
			}
			if(!mimetype.empty()){
				ss <<	"Content-Type: " << mimetype << "\r\n";
			}

			ss <<		"Server: " TEAPOY_VERSION "\r\n";
			ss <<		"Accept-Ranges: bytes\r\n";
			{
				time_t t_now = time(nullptr);
				struct tm* gmt_now = gmtime(&t_now);
				char buff_time[100] = {0};
				strftime(buff_time,sizeof(buff_time),"%a, %0e %h %Y %T GMT",gmt_now);
				ss <<		"Date: " << buff_time << "\r\n";

				//建议客户端缓存60秒。
				t_now += 60;
				struct tm* gmt_expires = gmtime(&t_now);
				char buff_time2[100] = {0};
				strftime(buff_time2,sizeof(buff_time2),"%a, %0e %h %Y %T GMT",gmt_expires);

				ss <<		"Expires: "<< buff_time2 << "\r\n";
				ss <<		"Cache-Control: max-age=60\r\n";
				ss <<		"Age: 60\r\n";
			}
			if(!lastmodified.empty()){
				ss <<		"Last-Modified: " << lastmodified << "\r\n";
			}
			if(!etag.empty()){
				ss <<		"ETag: " << etag << "\r\n";
			}
			ss <<		"Content-Length: " << datacount << "\r\n";
			if(is_range){
				ss <<	"Content-Range: " << range_start << "-" << range_end << "/" << filesize << "\r\n";
			}
			ss <<		"Access-Control-Allow-Origin: *\r\n";
			ss <<		"Access-Control-Allow-Methods: *\r\n";
			ss <<		"\r\n";

			os << ss.str();
#ifdef NOSENDIFLE
			std::ifstream ifs;
			ifs.open(rawfile.c_str(),std::ifstream::binary|std::ifstream::in);
			if(!ifs.is_open()){
				lyramilk::teapoy::http::make_response_header(os,"404 Not Found",true,req->ver.major,req->ver.minor);
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
#else
			int filefd = open(rawfile.c_str(),O_RDONLY);
			if(filefd < 1){
				lyramilk::teapoy::http::make_response_header(os,"404 Not Found",true,req->ver.major,req->ver.minor);
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

			if(rawfile.find("..") != rawfile.npos){
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
			lyramilk::data::uint64 filesize = st.st_size;
			lyramilk::data::uint64 datacount = filesize;

			lyramilk::data::var v = req->get("Range");
			lyramilk::data::int64 range_start = 0;
			lyramilk::data::int64 range_end = -1;
			bool is_range = false;
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
			}

			if(is_range){
				//b
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
				//e
				datacount = range_end-range_start;
			}

			lyramilk::data::var vifmodified = req->get("If-Modified-Since");
			lyramilk::data::var vifetagnotmatch = req->get("If-None-Match");

			lyramilk::data::string lastmodified;
			lyramilk::data::string etag;

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
				lyramilk::teapoy::http::make_response_header(os,"304 Not Modified",true,req->ver.major,req->ver.minor);
				return true;
			}

			if(vifmodified.type() != lyramilk::data::var::t_invalid && vifmodified == lastmodified){
				lyramilk::teapoy::http::make_response_header(os,"304 Not Modified",true,req->ver.major,req->ver.minor);
				return true;
			}


			if(is_range){
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

			lyramilk::data::string mimetype = mime::getmimetype_byname(rawfile);
			if(mimetype.empty()){
				mimetype = mime::getmimetype_byfile(rawfile);
			}

			lyramilk::data::stringstream ss;
			if(is_range){
				ss <<	"HTTP/1.1 206 Partial Content\r\n";
			}else{
				ss <<	"HTTP/1.1 200 OK\r\n";
			}
			if(!mimetype.empty()){
				ss <<	"Content-Type: " << mimetype << "\r\n";
			}

			ss <<		"Server: " TEAPOY_VERSION "\r\n";
			ss <<		"Accept-Ranges: bytes\r\n";
			{
				time_t t_now = time(nullptr);
				struct tm* gmt_now = gmtime(&t_now);
				char buff_time[100] = {0};
				strftime(buff_time,sizeof(buff_time),"%a, %0e %h %Y %T GMT",gmt_now);
				ss <<		"Date: " << buff_time << "\r\n";

				//建议客户端缓存60秒。
				t_now += 60000;
				struct tm* gmt_expires = gmtime(&t_now);
				char buff_time2[100] = {0};
				strftime(buff_time2,sizeof(buff_time2),"%a, %0e %h %Y %T GMT",gmt_expires);

				ss <<		"Expires: "<< buff_time2 << "\r\n";
				ss <<		"Cache-Control: max-age=60000\r\n";
				ss <<		"Age: 60000\r\n";
			}
			if(!lastmodified.empty()){
				ss <<		"Last-Modified: " << lastmodified << "\r\n";
			}
			if(!etag.empty()){
				ss <<		"ETag: " << etag << "\r\n";
			}
			ss <<		"Content-Length: " << datacount << "\r\n";
			if(is_range){
				ss <<	"Content-Range: " << range_start << "-" << range_end << "/" << filesize << "\r\n";
			}
			ss <<		"Access-Control-Allow-Origin: *\r\n";
			ss <<		"Access-Control-Allow-Methods: *\r\n";
			ss <<		"\r\n";

			os << ss.str();
#ifdef NOSENDIFLE
			std::ifstream ifs;
			ifs.open(rawfile.c_str(),std::ifstream::binary|std::ifstream::in);
			if(!ifs.is_open()){
				lyramilk::teapoy::http::make_response_header(os,"404 Not Found",true,req->ver.major,req->ver.minor);
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
#else
			int filefd = open(rawfile.c_str(),O_RDONLY);
			if(filefd < 1){
				lyramilk::teapoy::http::make_response_header(os,"404 Not Found",true,req->ver.major,req->ver.minor);
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
#endif
			return true;
		}

		bool call(lyramilk::teapoy::http::request* req,std::ostream& os,lyramilk::data::string real,website_worker& w) const
		{
			if(req->method == "GET") return onget(req,os,real);
			if(req->method == "POST") return onpost(req,os,real);
			lyramilk::teapoy::http::make_response_header(os,"405 Method Not Allowed",false,req->ver.major,req->ver.minor);
			os << "Allow: GET,POST\r\n\r\n";
			return false;
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
