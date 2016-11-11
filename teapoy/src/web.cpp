#include "web.h"
#include "stringutil.h"
#include "env.h"
#include <fstream>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

namespace lyramilk{ namespace teapoy { namespace web {

	/**************** methodinvoker ********************/
	methodinvoker::methodinvoker()
	{
		puf = nullptr;
		paf = nullptr;
		pcc = nullptr;
	}

	methodinvoker::~methodinvoker()
	{
	}

	void methodinvoker::set_url_filter(urlfilter* uf)
	{
		puf = uf;
	}

	void methodinvoker::set_parameters_filter(parametersfilter* af)
	{
		paf = af;
	}

	void methodinvoker::set_processer(processer_selector* cs)
	{
		pcc = cs;
	}

	bool methodinvoker::convert_url(lyramilk::data::string& url) const
	{
		if(!puf) return true;
		return puf->convert(url);
	}

	bool methodinvoker::convert_parameters(lyramilk::data::var::map& args) const
	{
		if(!paf) return true;
		return paf->convert(args);
	}

	bool methodinvoker::process(lyramilk::teapoy::http::request* req,std::ostream& os,bool* ret)
	{
		if(!pcc) return false;
		return pcc->invoke(this,req,os,ret);
	}

	/**************** methodinvokers ********************/
	methodinvokers* methodinvokers::instance()
	{
		static methodinvokers _mm;
		return &_mm;
	}

	/**************** aiohttpsession ********************/

	aiohttpsession::aiohttpsession()
	{
		
	}

	aiohttpsession::~aiohttpsession()
	{
	}

	bool aiohttpsession::onrequest(const char* cache,int size,std::ostream& os)
	{
//COUT.write(cache,size) << std::endl;
		int remain = 0;
		if(!req.parse(cache,size,&remain)){
			return true;
		}
		if(req.bad()){
			os <<	"HTTP/1.1 400 Bad Request\r\n"
					"Server: " SERVER_VER "\r\n"
					"\r\n";
			return false;
		}
		req.parse_cookies();

/*
os <<	"HTTP/1.1 401 Authorization Required\r\n"
		"Server: " SERVER "\r\n"
		"WWW-Authenticate: Digest username=\"Alice\", realm=\"Contacts Realm via Digest Authentication\",nonce=\"MTQwMTk3OTkwMDkxMzo3MjdjNDM2NTYzMTU2NTA2NWEzOWU2NzBlNzhmMjkwOA==\", uri=\"/secret\", cnonce=\"MTQwMTk3\", nc=00000001, qop=\"auth\",response=\"fd5798940c32e51c128ecf88472151af\"\r\n"
		"\r\n";
COUT <<	"HTTP/1.1 401 Authorization Required\r\n"
		"Server: " SERVER "\r\n"
		"\r\n";
return false;
*/


		// 动态调用
		methodinvoker* invoder = methodinvokers::instance()->get(req.method);
		if(invoder == nullptr){
			lyramilk::data::var::array ar = methodinvokers::instance()->keys();
			lyramilk::data::var::array::iterator it = ar.begin();
			lyramilk::data::string support;
			support.reserve(128);
			for(;it!=ar.end();++it){
				support+= it->str();
				support.push_back(',');
			}
			if(support.size()){
				support.erase(support.end()-1);
			}
			os <<	"HTTP/1.1 405 Method Not Allowed\r\n"
					"Server: " SERVER_VER "\r\n"
					"Allow: " << support << "\r\n"
					"\r\n";
			return false;
		}

		req.root = root;

		// URL过滤器
		invoder->convert_url(req.url);
		req.parse_args();
		// 参数过滤器
		invoder->convert_parameters(req.parameter);
		// 动态处理
		{
			bool dm_ret=true;
			if(invoder->process(&req,os,&dm_ret)){
				if(dm_ret){
					if(req.ver.major <= 1 && req.ver.minor < 1){
						return false;
					}

					lyramilk::data::var vconnection = req.get("Connection");
					lyramilk::data::var strconnection;
					if(vconnection.type_compat(lyramilk::data::var::t_str)){
						strconnection = lyramilk::teapoy::lowercase(strconnection);
					}
					if(strconnection == "keep-alive"){
						req.reset();
						if(remain != 0){
							//如果多个请求的数据在一个包中，
							return onrequest(cache+size-remain,remain,os);
						}
						return true;
					}
					return false;
				}
				return false;
			}
		}

		//静态处理
		if(req.ver.major == 1 && req.ver.minor < 1){
			invoder->call(&req,os);
			return false;
		}else if(invoder->call(&req,os)){
			lyramilk::data::var vconnection = req.get("Connection");
			lyramilk::data::var strconnection;
			if(vconnection.type_compat(lyramilk::data::var::t_str)){
				strconnection = lyramilk::teapoy::lowercase(vconnection);
			}
			if(strconnection == "keep-alive"){
				req.reset();
				if(remain != 0){
					//如果多个请求的数据在一个包中，
					return onrequest(cache+size-remain,remain,os);
				}
				return false;
			}
		}
		return false;
	}

	class method_get_invoker:public methodinvoker
	{
	  protected:
		virtual bool call(lyramilk::teapoy::http::request* req,std::ostream& os)
		{
			//	GET请求支持缓存、断点续传。,
			//	获取文件本地路径
			lyramilk::data::string rawfile = req->root + req->url;

			std::size_t pos = rawfile.find("?");
			if(pos != rawfile.npos){
				rawfile = rawfile.substr(0,pos);
			}

			struct stat st = {0};
			if(0 !=::stat(rawfile.c_str(),&st)){
				if(errno == ENOENT){
					os <<	"HTTP/1.1 404 Not Found\r\n"
							"Server: " SERVER_VER "\r\n"
							"\r\n";
					return true;
				}
				if(errno == EACCES){
					os <<	"HTTP/1.1 403 Forbidden\r\n"
							"Server: " SERVER_VER "\r\n"
							"\r\n";
					return false;
				}
				if(errno == ENAMETOOLONG){
					os <<	"HTTP/1.1 400 Bad Request\r\n"
							"Server: " SERVER_VER "\r\n"
							"\r\n";
					return true;
				}
				os <<	"HTTP/1.1 500 Internal Server Error\r\n"
						"Server: " SERVER_VER "\r\n"
						"\r\n";
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


			std::ifstream ifs;
			ifs.open(rawfile.c_str(),std::ifstream::binary|std::ifstream::in);
			if(!ifs.is_open()){
				os <<	"HTTP/1.1 404 Not Found\r\n"
						"Server: " SERVER_VER "\r\n"
						"\r\n";
				return true;
			}

			ifs.seekg(0,std::ifstream::end);
			lyramilk::data::uint64 filesize = ifs.tellg();
			lyramilk::data::uint64 datacount = filesize;
			if(is_range){
				if(!has_range_start && !has_range_end){
					os <<	"HTTP/1.1 500 Internal Server Error\r\n"
							"Server: " SERVER_VER "\r\n"
							"\r\n";
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
				ifs.seekg(range_start,std::ifstream::beg);
			}else{
				ifs.seekg(0,std::ifstream::beg);
			}

			if(is_range){
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
				os <<	"HTTP/1.1 304 Not Modified\r\n"
						"Server: " SERVER_VER "\r\n"
						"\r\n";
				return true;
			}

			if(vifmodified.type() != lyramilk::data::var::t_invalid && vifmodified == lastmodified){
				os <<	"HTTP/1.1 304 Not Modified\r\n"
						"Server: " SERVER_VER "\r\n"
						"\r\n";
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

			lyramilk::data::stringstream ss;
			if(is_range){
				ss <<	"HTTP/1.1 206 Partial Content\r\n";
			}else{
				ss <<	"HTTP/1.1 200 OK\r\n";
			}

			ss <<		"Server: " SERVER_VER "\r\n";
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
			char buff[4096];
			for(;ifs && datacount > 0;){
				ifs.read(buff,4096);
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
			return true;
		}
	};

	class method_post_invoker:public methodinvoker
	{
	  protected:
		virtual bool call(lyramilk::teapoy::http::request* req,std::ostream& os)
		{
			//POST请求不支持缓存和分块

			//	获取文件本地路径
			lyramilk::data::string rawfile = req->root + req->url;
				
			std::size_t pos = rawfile.find("?");
			if(pos != rawfile.npos){
				rawfile = rawfile.substr(0,pos);
			}

			struct stat st = {0};
			if(0 !=::stat(rawfile.c_str(),&st)){
				if(errno == ENOENT){
					os <<	"HTTP/1.1 404 Not Found\r\n"
							"Server: " SERVER_VER "\r\n"
							"\r\n";
					return true;
				}
				if(errno == EACCES){
					os <<	"HTTP/1.1 403 Forbidden\r\n"
							"Server: " SERVER_VER "\r\n"
							"\r\n";
					return false;
				}
				if(errno == ENAMETOOLONG){
					os <<	"HTTP/1.1 400 Bad Request\r\n"
							"Server: " SERVER_VER "\r\n"
							"\r\n";
					return true;
				}
				os <<	"HTTP/1.1 500 Internal Server Error\r\n"
						"Server: " SERVER_VER "\r\n"
						"\r\n";
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

			std::ifstream ifs;
			ifs.open(rawfile.c_str(),std::ifstream::binary|std::ifstream::in);
			if(!ifs.is_open()){
				os <<	"HTTP/1.1 404 Not Found\r\n"
						"Server: " SERVER_VER "\r\n"
						"\r\n";
				return true;
			}

			ifs.seekg(0,std::ifstream::end);
			lyramilk::data::uint64 filesize = ifs.tellg();
			ifs.seekg(0,std::ifstream::beg);

			lyramilk::data::stringstream ss;
			ss <<	"HTTP/1.1 200 OK\r\n";
			ss <<	"Server: " SERVER_VER "\r\n";
			ss <<	"Content-Length: " << filesize << "\r\n";
			ss <<	"Connection: Keep-Alive\r\n";
			ss <<	"Access-Control-Allow-Origin: *\r\n";
			ss <<	"Access-Control-Allow-Methods: *\r\n";
			{
				time_t t_now = time(nullptr);
				struct tm* gmt_now = gmtime(&t_now);
				char buff_time[100] = {0};
				strftime(buff_time,sizeof(buff_time),"%a, %0e %h %Y %T GMT",gmt_now);
				ss <<"Date: " << buff_time << "\r\n";
			}
			ss <<	"\r\n";
			os << ss.str();


			char buff[4096];
			for(;ifs;){
				ifs.read(buff,4096);
				lyramilk::data::int64 gcount = ifs.gcount();
				if(gcount > 0){
					os.write(buff,gcount);
				}
			}
			ifs.close();
			return true;


			/*
			int max = req->getmultipartcount();
			for(int i=0;i<max;++i){
				mime& m = req->selectpart(i);
				std::ofstream ofs;
				ofs.open("/tmp/1",std::ofstream::binary|std::ofstream::out);
				ofs.write(m.getbodyptr(),m.getbodylength());
				ofs.close();

				FILE* fp = popen("/usr/bin/md5sum /tmp/1","r");
				if(fp){
					char buff[409600];
					int r = fread(buff,1,409600,fp);
					fclose(fp);
					if(r > 0){
COUT.write(buff,r) << std::endl;
					}
				}
			}*/
		}
	};

	static __attribute__ ((constructor)) void __init()
	{
		methodinvokers::instance()->define("GET",new method_get_invoker);
		methodinvokers::instance()->define("HEAD",new method_get_invoker);
		methodinvokers::instance()->define("POST",new method_post_invoker);
	}

}}}
