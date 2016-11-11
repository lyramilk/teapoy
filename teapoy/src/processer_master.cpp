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
	/********** processer ***********/
	processer::processer()
	{
		regex = nullptr;
	}

	processer::~processer()
	{
	}

	bool processer::init(lyramilk::data::string pattern,lyramilk::data::string proc_file_pattern)
	{
		assert(regex == nullptr);
		if(regex == nullptr){
			int  erroffset = 0;
			const char *error = "";
			regex = pcre_compile(pattern.c_str(),0,&error,&erroffset,nullptr);
		}
		if(regex){
			lyramilk::data::var::array ar;
			lyramilk::data::string ret;
			int sz = proc_file_pattern.size();
			for(int i=0;i<sz;++i){
				char &c = proc_file_pattern[i];
				if(c == '$'){
					if(proc_file_pattern[i + 1] == '{'){
						std::size_t d = proc_file_pattern.find('}',i+1);
						if(d != proc_file_pattern.npos){
							lyramilk::data::string su = proc_file_pattern.substr(i+2,d - i - 2);
							int qi = atoi(su.c_str());
							ar.push_back(ret);
							ar.push_back(qi);
							ret.clear();
							i = d;
							continue;
						}
					}
				}
				ret.push_back(c);
			}
			ar.push_back(ret);
			if(ar.size() == 1){
				dest = ar[0];
			}else if(ar.size() > 1){
				dest = ar;
			}
		}
		return true;
	}

	bool processer::test(methodinvoker* invoker,lyramilk::teapoy::http::request* req,std::ostream& os,bool* ret)
	{
		lyramilk::data::string url = req->url;

		if(!regex) return false;
		int ov[256] = {0};

		int rc = pcre_exec((const pcre*)regex,nullptr,url.c_str(),url.size(),0,0,ov,256);
		if(rc > 0){
			if(dest.type() == lyramilk::data::var::t_str){
				lyramilk::data::string real = dest.str();
				std::size_t pos_args = real.find("?");
				if(pos_args!=real.npos){
					real = real.substr(0,pos_args);
				}

				if(::access(real.c_str(),F_OK|R_OK) != 0){
					if(errno == ENOENT){
						return false;
					}
					if(errno == EACCES){
						/*
						os <<	"HTTP/1.1 404 Not Found\r\n"
								"Server: " SERVER_VER "\r\n"
								"\r\n";
						if(ret)*ret = true;
						return true;*/
						return false;
					}
				}
				if(ret){
					*ret = invoke(real,invoker,req,os);
				}else{
					invoke(real,invoker,req,os);
				}
				return true;
			}else if(dest.type() == lyramilk::data::var::t_array){
				lyramilk::data::var::array& ar = dest;
				lyramilk::data::string real;
				for(lyramilk::data::var::array::iterator it = ar.begin();it!=ar.end();++it){
					if(it->type() == lyramilk::data::var::t_str){
						real.append(it->str());
					}else if(it->type_compat(lyramilk::data::var::t_int32)){
						int qi = *it;
						int bof = ov[(qi<<1)];
						int eof = ov[(qi<<1)|1];
						if(eof - bof > 0 && bof < (int)url.size()){
							real.append(url.substr(bof,eof-bof));
						}
					}
				}
				std::size_t pos_args = real.find("?");
				if(pos_args!=real.npos){
					real = real.substr(0,pos_args);
				}

				if(::access(real.c_str(),F_OK|R_OK) != 0){
					if(errno == ENOENT){
						return false;
					}
					if(errno == EACCES){
						/*
						os <<	"HTTP/1.1 404 Not Found\r\n"
								"Server: " SERVER_VER "\r\n"
								"\r\n";
						if(ret)*ret = true;
						return true;*/
						return false;
					}
				}
				if(ret){
					*ret = invoke(real,invoker,req,os);
				}else{
					invoke(real,invoker,req,os);
				}
				return true;
			}
		}
		return false;
	}

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
			o->reset();
		}

		virtual void onremove(lyramilk::script::engine* o)
		{
			lyramilk::script::engine::destoryinstance("js",o);
		}
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
	{}

	processer_js::~processer_js()
	{}
	
	bool processer_js::invoke(lyramilk::data::string proc_file,methodinvoker* invoker,lyramilk::teapoy::http::request* req,std::ostream& os)
	{
		//debug
		lyramilk::data::string k = D("%s -> %s ",req->url.c_str(),proc_file.c_str());
		lyramilk::debug::nsecdiff td;
		lyramilk::debug::clocktester _d(td,lyramilk::klog(lyramilk::log::debug),k);

		engine_master_js::ptr p = engine_master_js::instance()->get();

		if(!p->load_file(proc_file)){
			COUT << "加载文件" << proc_file << "失败" << std::endl;
		}


		processer_args args = {
			req:req,
			os:os,
			invoker:invoker,
			real:proc_file,
		};

		lyramilk::data::var::array ar;
		{
			lyramilk::data::var var_processer_args("__http_processer_args",&args);

			lyramilk::data::var::array args;
			args.push_back(var_processer_args);

			ar.push_back(p->createobject("HttpRequest",args));
			ar.push_back(p->createobject("HttpResponse",args));

		}
		return p->call("onrequest",ar);
	}
	
	bool processer_js::test(methodinvoker* invoker,lyramilk::teapoy::http::request* req,std::ostream& os,bool* ret)
	{
		return processer::test(invoker,req,os,ret);
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
		lyramilk::data::string k = D("%s -> %s ",req->url.c_str(),proc_file.c_str());
		lyramilk::debug::nsecdiff td;
		lyramilk::debug::clocktester _d(td,lyramilk::klog(lyramilk::log::debug),k);



		lyramilk::data::string jsx_jsfile = proc_file + ".js";
		struct stat statbuf[2];
		if(-1==stat (proc_file.c_str(), &statbuf[0])){
			if(errno == ENOENT){
				return false;
			}
			if(errno == EACCES){
				os <<	"HTTP/1.1 403 Forbidden\r\n"
						"Server: " SERVER_VER "\r\n"
						"\r\n";
				return true;
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
		stat (jsx_jsfile.c_str(), &statbuf[1]);

		if(statbuf[0].st_mtime != statbuf[1].st_mtime){
			COUT << "时间不同" << std::endl;

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
					ofs << "response.write(unescape(\"" << lyramilk::data::codes::instance()->encode(jsxcache.substr(pos_begin),"js") << "\"));\n";
					break;
				}

				std::size_t pos_flag2 = jsxcache.find("%>",pos_flag1);
				if(pos_flag2 == jsxcache.npos){
					break;
				}

				//写入 html 代码
				if(pos_flag1 != pos_begin){
					ofs << "response.write(unescape(\"" << lyramilk::data::codes::instance()->encode(jsxcache.substr(pos_begin,pos_flag1 - pos_begin),"js") << "\"));\n";
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

		processer_args args = {
			req:req,
			os:os,
			invoker:invoker,
			real:proc_file,
		};

		lyramilk::data::var::array ar;
		{
			lyramilk::data::var var_processer_args("__http_processer_args",&args);

			lyramilk::data::var::array args;
			args.push_back(var_processer_args);

			ar.push_back(p->createobject("HttpRequest",args));
			ar.push_back(p->createobject("HttpResponse",args));

		}
		return p->call("onrequest",ar);
	}

	bool processer_jsx::invoke(lyramilk::data::string proc_file,methodinvoker* invoker,lyramilk::teapoy::http::request* req,std::ostream& os)
	{
		return exec_jsx(proc_file,invoker,req,os);
	}
	
	bool processer_jsx::test(methodinvoker* invoker,lyramilk::teapoy::http::request* req,std::ostream& os,bool* ret)
	{
		return processer::test(invoker,req,os,ret);
	}

	/********** processer_master ***********/

	processer_master::processer_master()
	{
	}

	processer_master::~processer_master()
	{
		std::vector<processer_pair>::iterator it = es.begin();
		for(;it!=es.end();++it){
			destory(it->type,it->ptr);
		}
	}

	void processer_master::mapping(lyramilk::data::string type,lyramilk::data::string pattern,lyramilk::data::string proc_file_pattern)
	{
		processer_pair pr;
		pr.ptr = create(type);
		pr.type = type;
		if(pr.ptr){
			es.push_back(pr);
			processer* proc = es.back().ptr;
			proc->init(pattern,proc_file_pattern);
		}
	}

	bool processer_master::invoke(methodinvoker* invoker,lyramilk::teapoy::http::request* req,std::ostream& os,bool* ret)
	{
		try{
			std::vector<processer_pair>::iterator it = es.begin();
			for(;it!=es.end();++it){
				if(it->ptr->test(invoker,req,os,ret)){
					return true;
				}
			}
		}catch(...){
			os <<	"HTTP/1.1 500 Internal Server Error\r\n"
					"Server: " SERVER_VER "\r\n"
					"\r\n";
			return true;
		}
	
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
				if(ret)*ret = false;
				return true;
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
			if(ret)*ret = false;
			return true;
		}

		if(st.st_mode&S_IFDIR){
			lyramilk::data::string rawdir;
			std::size_t pos_end = rawfile.find_last_not_of("/");
			rawdir = rawfile.substr(0,pos_end + 1);
			rawdir.push_back('/');

			lyramilk::data::string tmp = rawdir +"index.jsx";
			if(0 == ::stat(tmp.c_str(),&st) && !S_ISDIR(st.st_mode)){
				rawfile = tmp;
			}
		}


		if(rawfile.compare(rawfile.size()-4,4,".jsx") == 0){
			if(ret){
				*ret = processer_jsx::exec_jsx(rawfile,invoker,req,os);
			}else{
				processer_jsx::exec_jsx(rawfile,invoker,req,os);
			}
			return true;
		}
		return false;
	}

}}}
