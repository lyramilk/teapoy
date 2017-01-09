#include "web.h"
#include "script.h"
#include <fstream>
#include <sys/stat.h>
#include <errno.h>
#include <libmilk/log.h>
#include <libmilk/multilanguage.h>
#include <libmilk/testing.h>
#include <pcre.h>
#include <libmilk/codes.h>
#include <utime.h>

namespace lyramilk{ namespace teapoy { namespace web {
	class url_worker_js : public url_worker
	{
		lyramilk::script::engines* pool;

		bool call(lyramilk::teapoy::http::request* req,std::ostream& os,lyramilk::data::string real,website_worker& worker) const
		{
			req->parse_cookies();
			req->parse_body_param();
			req->parse_url();

			lyramilk::script::engines::ptr p = pool->get();
			if(!p->load_file(real)){
				lyramilk::klog(lyramilk::log::warning,"lyramilk.teapoy.web.url_worker_js.call") << D("加载文件%s失败",real.c_str()) << std::endl;
			}

			session_info si(real,req,os,worker);

			lyramilk::data::var::array ar;
			{
				lyramilk::data::var var_processer_args("__http_session_info",&si);

				lyramilk::data::var::array args;
				args.push_back(var_processer_args);

				ar.push_back(p->createobject("HttpRequest",args));
				ar.push_back(p->createobject("HttpResponse",args));
			}

			lyramilk::data::var vret = p->call("onrequest",ar);
			if(vret.type_like(lyramilk::data::var::t_bool)){
				return vret;
			}
			return false;
		}
	  public:
		url_worker_js(lyramilk::script::engines* es)
		{
			pool = es;
		}
		virtual ~url_worker_js()
		{
		}

		static url_worker* ctr(void*)
		{
			lyramilk::script::engines* es = engine_pool::instance()->get("js");
			if(es)return new url_worker_js(es);
			return nullptr;
		}

		static void dtr(url_worker* p)
		{
			delete (url_worker_js*)p;
		}
	};


	class url_worker_jsx : public url_worker
	{
		lyramilk::script::engines* pool;

		bool call(lyramilk::teapoy::http::request* req,std::ostream& os,lyramilk::data::string real,website_worker& worker) const
		{
			lyramilk::data::string jsx_jsfile = real + ".js";
			struct stat statbuf[2];
			if(-1==stat (real.c_str(), &statbuf[0])){
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

			if(-1==stat (jsx_jsfile.c_str(), &statbuf[1])){
				if(errno == EACCES){
					lyramilk::teapoy::http::make_response_header(os,"403 Forbidden",true,req->ver.major,req->ver.minor);
					return false;
				}
				if(errno == ENAMETOOLONG){
					lyramilk::teapoy::http::make_response_header(os,"400 Bad Request",true,req->ver.major,req->ver.minor);
					return false;
				}
				if(errno != ENOENT){
					lyramilk::teapoy::http::make_response_header(os,"500 Internal Server Error",true,req->ver.major,req->ver.minor);
					return false;
				}
			}

			if(statbuf[0].st_mtime != statbuf[1].st_mtime){
				lyramilk::data::string jsxcache;
				std::ifstream ifs;
				ifs.open(real.c_str(),std::ifstream::binary);
				char buff[4096];
				while(ifs){
					ifs.read(buff,4096);
					jsxcache.append(buff,ifs.gcount());
				}
				ifs.close();

				std::size_t pos_begin = 0;
				std::ofstream ofs;
				ofs.open(jsx_jsfile.c_str(),std::ofstream::binary);
				ofs << "function onrequest(request,response){\n";

				while(true){
					std::size_t pos_flag1 = jsxcache.find("<%",pos_begin);
					if(pos_flag1 == jsxcache.npos){
						//写入 html 代码
						ofs << "response.write(unescape(\"" << lyramilk::data::codes::instance()->encode("js",jsxcache.substr(pos_begin)) << "\"));\n";
						break;
					}

					std::size_t pos_flag2 = jsxcache.find("%>",pos_flag1);
					if(pos_flag2 == jsxcache.npos){
						break;
					}

					//写入 html 代码
					if(pos_flag1 != pos_begin){
						ofs << "response.write(unescape(\"" << lyramilk::data::codes::instance()->encode("js",jsxcache.substr(pos_begin,pos_flag1 - pos_begin)) << "\"));\n";
					}
					//写入 js 代码
					lyramilk::data::string tmp = jsxcache.substr(pos_flag1 + 2,pos_flag2 - pos_flag1 - 2);
					if(!tmp.empty()){
						if(tmp[0] == '='){
							ofs << "response.write(" << tmp.substr(1) << ")" << "\n";
						}else{
							ofs << tmp << "\n";
						}
					}

					pos_begin = pos_flag2 + 2;
				}

				ofs << "return true;\n}\n";
				ofs.close();
				//更新 jsx的js 文件的时间戳。
				struct utimbuf tv;
				tv.actime = statbuf[0].st_ctime;
				tv.modtime = statbuf[0].st_mtime;
				utime(jsx_jsfile.c_str(),&tv);
			}

			req->parse_cookies();
			req->parse_body_param();
			req->parse_url();

			lyramilk::script::engines::ptr p = pool->get();
			if(!p->load_file(jsx_jsfile)){
				lyramilk::klog(lyramilk::log::warning,"lyramilk.teapoy.web.url_worker_js.call") << D("加载文件%s失败",jsx_jsfile.c_str()) << std::endl;
			}

			session_info si(jsx_jsfile,req,os,worker);

			lyramilk::data::var::array ar;
			{
				lyramilk::data::var var_processer_args("__http_session_info",&si);

				lyramilk::data::var::array args;
				args.push_back(var_processer_args);

				ar.push_back(p->createobject("HttpRequest",args));
				ar.push_back(p->createobject("HttpResponse",args));
			}

			lyramilk::data::var vret = p->call("onrequest",ar);
			if(vret.type_like(lyramilk::data::var::t_bool)){
				return vret;
			}
			return false;
		}
	  public:
		url_worker_jsx(lyramilk::script::engines* es)
		{
			pool = es;
		}
		virtual ~url_worker_jsx()
		{
		}

		static url_worker* ctr(void*)
		{
			lyramilk::script::engines* es = engine_pool::instance()->get("js");
			if(es)return new url_worker_jsx(es);
			return nullptr;
		}

		static void dtr(url_worker* p)
		{
			delete (url_worker_jsx*)p;
		}
	};
	static __attribute__ ((constructor)) void __init()
	{
		url_worker_master::instance()->define("js",url_worker_js::ctr,url_worker_js::dtr);
		url_worker_master::instance()->define("jsx",url_worker_jsx::ctr,url_worker_jsx::dtr);
	}
}}}
