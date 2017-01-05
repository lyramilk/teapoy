#include "processer_master.h"
#include "script.h"
#include <libmilk/log.h>
#include <libmilk/multilanguage.h>
#include <libmilk/testing.h>
#include <pcre.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <fstream>
#include <utime.h>
#include <libmilk/codes.h>

namespace lyramilk{ namespace teapoy { namespace web {
	/********** engine_master_js ***********/

	class engine_master_js:public lyramilk::script::engines
	{
	  public:
		engine_master_js()
		{
		}

		virtual ~engine_master_js()
		{
		}

		static engine_master_js* instance()
		{
			static engine_master_js _mm;
			return &_mm;
		}

		virtual lyramilk::script::engine* underflow()
		{
			lyramilk::script::engine* eng_tmp = lyramilk::script::engine::createinstance("js");
			if(!eng_tmp){
				lyramilk::klog(lyramilk::log::error,"teapoy.web.engine_master_js.underflow") << D("创建引擎对象失败(%s)","js") << std::endl;
				return nullptr;
			}

			lyramilk::teapoy::script2native::instance()->fill(eng_tmp);
			return eng_tmp;
		}

		virtual void onfire(lyramilk::script::engine* o)
		{
			o->set("clearonreset",lyramilk::data::var::map());
			o->reset();
		}

		virtual void onremove(lyramilk::script::engine* o)
		{
			lyramilk::script::engine::destoryinstance("js",o);
		}

		lyramilk::teapoy::strings preloads;
	};


	/********** processer_js ***********/
	processer* processer_js::ctr(void* args)
	{
		return new processer_js;
	}

	void processer_js::dtr(processer* ptr)
	{
		delete ptr;
	}

	processer_js::processer_js()
	{
	}

	processer_js::~processer_js()
	{}
	
	bool processer_js::invoke(lyramilk::data::string proc_file,methodinvoker* invoker,lyramilk::teapoy::http::request* req,std::ostream& os)
	{
		//debug
		/*
		lyramilk::data::string k = D("%s -> %s ",req->url.c_str(),proc_file.c_str());
		lyramilk::debug::nsecdiff td;
		lyramilk::debug::clocktester _d(td,lyramilk::klog(lyramilk::log::debug,"teapoy.web.js"),k);*/

		engine_master_js::ptr p = engine_master_js::instance()->get();
		if(!p->load_file(proc_file)){
			COUT << "加载文件" << proc_file << "失败" << std::endl;
		}

		processer_args args(proc_file,invoker,req,os);

		lyramilk::data::var::array ar;
		{
			lyramilk::data::var var_processer_args("__http_processer_args",&args);

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

	/********** processer_jsx ***********/
	processer* processer_jsx::ctr(void* args)
	{
		return new processer_jsx;
	}

	void processer_jsx::dtr(processer* ptr)
	{
		delete ptr;
	}

	processer_jsx::processer_jsx()
	{}

	processer_jsx::~processer_jsx()
	{}

	bool processer_jsx::exec_jsx(lyramilk::data::string proc_file,methodinvoker* invoker,lyramilk::teapoy::http::request* req,std::ostream& os)
	{
		//debug
		/*
		lyramilk::data::string k = D("%s -> %s ",req->url.c_str(),proc_file.c_str());
		lyramilk::debug::nsecdiff td;
		lyramilk::debug::clocktester _d(td,lyramilk::klog(lyramilk::log::debug,"teapoy.web.jsx"),k);*/



		lyramilk::data::string jsx_jsfile = proc_file + ".js";
		struct stat statbuf[2];
		if(-1==stat (proc_file.c_str(), &statbuf[0])){
			if(errno == ENOENT){
				return false;
			}
			if(errno == EACCES){
				os <<	"HTTP/1.1 403 Forbidden\r\n"
						"Server: " TEAPOY_VERSION "\r\n"
						"\r\n";
				return true;
			}
			if(errno == ENAMETOOLONG){
				os <<	"HTTP/1.1 400 Bad Request\r\n"
						"Server: " TEAPOY_VERSION "\r\n"
						"\r\n";
				return true;
			}
			os <<	"HTTP/1.1 500 Internal Server Error\r\n"
					"Server: " TEAPOY_VERSION "\r\n"
					"\r\n";
			return false;
		}
		stat (jsx_jsfile.c_str(), &statbuf[1]);

		if(statbuf[0].st_mtime != statbuf[1].st_mtime){
			lyramilk::data::string jsxcache;
			std::ifstream ifs;
			ifs.open(proc_file.c_str(),std::ifstream::binary);
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

		//	加载 jsx的js 文件
		engine_master_js::ptr p = engine_master_js::instance()->get();

		if(!p->load_file(jsx_jsfile)){
			COUT << "加载文件" << proc_file << "失败" << std::endl;
		}

		processer_args args(proc_file,invoker,req,os);

		lyramilk::data::var::array ar;
		{
			lyramilk::data::var var_processer_args("__http_processer_args",&args);

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

	bool processer_jsx::invoke(lyramilk::data::string proc_file,methodinvoker* invoker,lyramilk::teapoy::http::request* req,std::ostream& os)
	{
		return exec_jsx(proc_file,invoker,req,os);
	}
}}}
