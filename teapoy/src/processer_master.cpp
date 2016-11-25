#include "processer_master.h"
#include "script.h"
#include "stringutil.h"
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
#include <stdlib.h>
#include <iomanip>

namespace lyramilk{ namespace teapoy { namespace web {

	/********** sessions ***********/

	class sessions
	{
		struct timedmap
		{
			lyramilk::data::var::map m;
			time_t tm;
			timedmap();
		};
		time_t st;
		std::map<lyramilk::data::string,timedmap> k;
	  public:
		sessions();
		virtual ~sessions();
		lyramilk::data::var& get(const lyramilk::data::string& sid,const lyramilk::data::string& key);
		void gc();
		void set(const lyramilk::data::string& sid,const lyramilk::data::string& key,const lyramilk::data::var& value);
		lyramilk::data::string newid();

		static sessions* instance();
	};

	sessions::timedmap::timedmap()
	{
		tm = time(0);
	}

	sessions::sessions()
	{
		st = time(0);
	}

	sessions::~sessions()
	{}

	lyramilk::data::var& sessions::get(const lyramilk::data::string& sid,const lyramilk::data::string& key)
	{
		gc();
		timedmap& t = k[sid];
		t.tm = time(0);
		return t.m[key];
	}

	void sessions::gc()
	{
		time_t now = time(0);
		if(st < now){
			//每60秒触发一次gc。
			st = now + 10;
			//会话时间为600秒。
			time_t q = now - 600;
			for(std::map<lyramilk::data::string,timedmap>::iterator it = k.begin();it!=k.end();++it){
				if(it->second.tm < q){
					k.erase(it);
				}
			}
		}
	}

	void sessions::set(const lyramilk::data::string& sid,const lyramilk::data::string& key,const lyramilk::data::var& value)
	{
		gc();
		timedmap& t = k[sid];
		t.tm = time(0);
		t.m[key] = value;
	}

	static lyramilk::data::string makeid()
	{
		union{
			int i[4];
			char c[1];
		}u;
		u.i[0] = rand();
		u.i[1] = rand();
		u.i[2] = rand();
		u.i[3] = rand();
		int size = sizeof(u);
		lyramilk::data::string str;
		str.reserve(size);
		for(int i=0;i<size;++i){
			unsigned char c = u.c[i];
			unsigned char q = c % 36;
			if(q <10){
				str.push_back(q + '0');
			}else{
				str.push_back(q - 10 + 'A');
			}
		}
		return str;
	}

	lyramilk::data::string sessions::newid()
	{
		srand(time(0));
		std::map<lyramilk::data::string,lyramilk::data::var::map>::iterator it;
		while(true){
			lyramilk::data::string id = makeid();
			if(k.find(id) == k.end()){
				return id;
			}
		}
	}

	sessions* sessions::instance()
	{
		static sessions _mm;
		return &_mm;
	}
	
	
	/********** processer_args ***********/
	processer_args::processer_args(lyramilk::data::string proc_file,methodinvoker* invoker,lyramilk::teapoy::http::request* req,std::ostream& argos):os(argos)
	{
		this->real = proc_file;
		this->invoker = invoker;
		this->req = req;
	}

	processer_args::~processer_args()
	{
	}

	const lyramilk::data::string& processer_args::getsid()
	{
		if(!sid.empty()) return sid;

		lyramilk::data::var::map::iterator it = req->cookies.find("TeapoyId");

		if(it!=req->cookies.end()){
			if(it->second.type() == lyramilk::data::var::t_map){
				sid = it->second.at("value").str();
			}
			if(it->second.type_like(lyramilk::data::var::t_str)){
				sid = it->second.str();
			}
		}
		if(sid.empty()){
			sid = lyramilk::teapoy::web::sessions::instance()->newid();
			lyramilk::data::var v;
			v.type(lyramilk::data::var::t_map);
			v["value"] = sid;
			req->cookies["TeapoyId"] = v;
		}
		return sid;
	}

	lyramilk::data::var& processer_args::get(const lyramilk::data::string& key)
	{
		return lyramilk::teapoy::web::sessions::instance()->get(req->dest + getsid(),key);
	}

	void processer_args::set(const lyramilk::data::string& key,const lyramilk::data::var& value)
	{
		lyramilk::teapoy::web::sessions::instance()->set(req->dest + getsid(),key,value);
	}

	/********** processer ***********/
	processer::processer()
	{
		regex = nullptr;
		pauthobj = nullptr;
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
			regexstr = pattern;
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

	bool processer::set_auth(lyramilk::data::string authfiletype,lyramilk::data::string authfile)
	{
		pauthobj = lyramilk::script::engine::createinstance(authfiletype);
		if(pauthobj){
			if(pauthobj->load_file(true,authfile)){
				lyramilk::teapoy::script2native::instance()->fill(true,pauthobj);
				return true;
			}
			lyramilk::script::engine::destoryinstance(authfiletype,pauthobj);
		}
		return false;
	}

	bool processer::preload(const lyramilk::teapoy::strings& loads)
	{
		return false;
	}

	bool processer::auth_check(methodinvoker* invoker,lyramilk::teapoy::http::request* req,std::ostream& os,bool* ret)
	{
		if(pauthobj == nullptr) return true;


		processer_args args("",invoker,req,os);
		lyramilk::data::var v = args.get("http.digest.user");
		if(v.type() == lyramilk::data::var::t_str){
			return true;
		}

		lyramilk::data::var::array ar;
		lyramilk::data::string authorization_str = req->get("authorization");
		lyramilk::teapoy::strings strs = lyramilk::teapoy::split(authorization_str,",");
		lyramilk::data::var::map logininfo;
		lyramilk::teapoy::strings::iterator it = strs.begin();
		for(;it!=strs.end();++it){
			lyramilk::teapoy::strings pairs = lyramilk::teapoy::split(*it,"=");
			if(pairs.size() == 2){
				logininfo[lyramilk::teapoy::trim(pairs[0]," \"'")] = lyramilk::teapoy::trim(pairs[1]," \"'");
			}
		}

		logininfo["method"] = req->method;
		//不使用客户端传过来的nonce，因为这个值有可能是伪造的。
		logininfo["nonce"] = req->nonce;
		ar.push_back(logininfo);
		lyramilk::data::var vret = pauthobj->call("auth",ar);
		if(vret == true){
			args.set("http.digest.user",logininfo["Digest username"]);
			return true;
		}
		if(vret.type() != lyramilk::data::var::t_map){
			os <<	"HTTP/1.1 500 Internal Server Error\r\n"
					"Server: " SERVER_VER "\r\n"
					"\r\n";
			if(ret)*ret = false;
			return false;
		}
		lyramilk::data::string digest = vret["digest"];
		lyramilk::data::string realm = vret["realm"];
		lyramilk::data::string algorithm = vret["algorithm"];
		req->nonce = vret["nonce"].str();
		lyramilk::data::uint64 nc;

		if(logininfo["nc"].type_like(lyramilk::data::var::t_str)){
			nc = logininfo["nc"];
			++nc;
		}else{
			nc = 0;
		}

		lyramilk::data::stringstream ss;
		ss <<	"HTTP/1.1 401 Authorization Required\r\n"
				"Connection: Keep-alive\r\n"
				"Server: " SERVER_VER "\r\n"
				"Content-Length: 0\r\n"
				"WWW-Authenticate: Digest username=\"" << digest << "\",realm=\"" << realm << "\",nonce=\"" << req->nonce << "\", algorithm=\"" << algorithm << "\",qop=\"auth\"\r\n"
				"\r\n";
		os << ss.str();
		if(ret)*ret = true;
		return false;
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
					if(!auth_check(invoker,req,os,ret)){
						return true;
					}
					*ret = invoke(real,invoker,req,os);
				}else{
					if(!auth_check(invoker,req,os,ret)){
						return true;
					}
					invoke(real,invoker,req,os);
				}
				return true;
			}else if(dest.type() == lyramilk::data::var::t_array){
				lyramilk::data::var::array& ar = dest;
				lyramilk::data::string real;
				for(lyramilk::data::var::array::iterator it = ar.begin();it!=ar.end();++it){
					if(it->type() == lyramilk::data::var::t_str){
						real.append(it->str());
					}else if(it->type_like(lyramilk::data::var::t_int32)){
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
					if(!auth_check(invoker,req,os,ret)){
						return true;
					}
					*ret = invoke(real,invoker,req,os);
				}else{
					if(!auth_check(invoker,req,os,ret)){
						return true;
					}
					invoke(real,invoker,req,os);
				}
				return true;
			}
		}
		return false;
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

	void processer_master::mapping(lyramilk::data::string type,lyramilk::data::string pattern,lyramilk::data::string proc_file_pattern,lyramilk::data::string authfiletype,lyramilk::data::string authfile)
	{
		processer_pair pr;
		pr.ptr = create(type);
		pr.type = type;
		if(pr.ptr){
			es.push_back(pr);
			processer* proc = es.back().ptr;
			proc->init(pattern,proc_file_pattern);
			proc->set_auth(authfiletype,authfile);
		}
	}

	bool processer_master::invoke(methodinvoker* invoker,lyramilk::teapoy::http::request* req,std::ostream& os,bool* ret)
	{
		//try{
			std::vector<processer_pair>::iterator it = es.begin();
			for(;it!=es.end();++it){
				if(it->ptr->test(invoker,req,os,ret)){
					return true;
				}
			}/*
		}catch(...){
			os <<	"HTTP/1.1 500 Internal Server Error\r\n"
					"Server: " SERVER_VER "\r\n"
					"\r\n";
			return true;
		}*/

		lyramilk::data::string rawfile = req->root + req->url;
		std::size_t pos = rawfile.find("?");
		if(pos != rawfile.npos){
			rawfile = rawfile.substr(0,pos);
		}

		struct stat st = {0};
		if(0 !=::stat(rawfile.c_str(),&st)){
			if(errno == ENOENT){
				return false;
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

	bool processer_master::preload(lyramilk::data::string type,const lyramilk::data::var::array& loads)
	{
		lyramilk::teapoy::strings& ars = preloads[type];
		{
			lyramilk::data::var::array::const_iterator it = loads.begin();
			for(;it!=loads.end();++it){
				if(it->type_like(lyramilk::data::var::t_str)){
					ars.push_back(it->str());
				}
			}
		}

		std::vector<processer_pair>::iterator it = es.begin();
		for(;it!=es.end();++it){
			if(it->ptr->preload(ars)){
				return true;
			}
		}
		return true;
	}

}}}
