#include "web.h"
#include "stringutil.h"
#include "script.h"
#include "env.h"
#include <fstream>
#include <sys/stat.h>
#include <libmilk/log.h>
#include <libmilk/multilanguage.h>
#include <libmilk/testing.h>
#include <pcre.h>
#include <string.h>

namespace lyramilk{ namespace teapoy { namespace web {
	/**************** session_info ********************/
	session_info::session_info(lyramilk::data::string realfile,lyramilk::teapoy::http::request* req,std::ostream& os,website_worker& w,webhook_helper* h):worker(w)
	{
		this->real = realfile;
		this->req = req;
		hook = h;
		rep.init(&os,&req->header->cookies());
	}

	session_info::~session_info()
	{
	}

	lyramilk::data::string session_info::getsid()
	{
		lyramilk::data::var::map& cookies = req->header->cookies();
		lyramilk::data::var::map::iterator it = cookies.find("TeapoyId");

		if(it!=cookies.end()){
			if(it->second.type() == lyramilk::data::var::t_map){
				sid = it->second.at("value").str();
			}
			if(it->second.type_like(lyramilk::data::var::t_str)){
				sid = it->second.str();
			}
		}
		if(sid.empty()){
			sid = req->sessionmgr->newid();
			lyramilk::data::var v;
			v.type(lyramilk::data::var::t_map);
			v["value"] = sid;
			cookies["TeapoyId"] = v;
		}
		return sid;
	}

	lyramilk::data::var& session_info::get(const lyramilk::data::string& key)
	{
		return req->sessionmgr->get(sid,key);
	}

	void session_info::set(const lyramilk::data::string& key,const lyramilk::data::var& value)
	{
		return req->sessionmgr->set(sid,key,value);
	}

	/**************** url_worker ********************/
	url_worker::url_worker()
	{
		matcher_regex = nullptr;
		static webhook default_hook;
		this->hook = &default_hook;
	}

	url_worker::~url_worker()
	{}

	lyramilk::data::string url_worker::get_method()
	{
		return method;
	}

	bool url_worker::init(lyramilk::data::string method,lyramilk::data::string pattern,lyramilk::data::string real,lyramilk::data::var::array index,webhook* h)
	{
		if(h) this->hook = h;
		this->method = method;
		lyramilk::data::var::array::iterator it = index.begin();
		this->index.reserve(index.size());
		for(;it!=index.end();++it){
			this->index.push_back(*it);
		}
		assert(matcher_regex == nullptr);
		if(matcher_regex == nullptr){
			int  erroffset = 0;
			const char *error = "";
			matcher_regex = pcre_compile(pattern.c_str(),0,&error,&erroffset,nullptr);
		}
		if(matcher_regex){
			matcher_regexstr = pattern;
			lyramilk::data::var::array ar;
			lyramilk::data::string ret;
			int sz = real.size();
			for(int i=0;i<sz;++i){
				char &c = real[i];
				if(c == '$'){
					if(real[i + 1] == '{'){
						std::size_t d = real.find('}',i+1);
						if(d != real.npos){
							lyramilk::data::string su = real.substr(i+2,d - i - 2);
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
				lyramilk::data::var& v = ar[0];
				if(v.type() == lyramilk::data::var::t_str){
					lyramilk::data::string s = v.str();
					if(!s.empty()){
						matcher_dest = ar[0];
					}
				}
			}else if(ar.size() > 1){
				matcher_dest = ar;
			}
		}else{
			lyramilk::klog(lyramilk::log::warning,"teapoy.web.processer.init") << D("编译正则表达式%s失败",pattern.c_str()) << std::endl;
		}
		return true;
	}

	bool url_worker::init_auth(lyramilk::data::string enginetype,lyramilk::data::string authscript)
	{
		authtype = enginetype;
		this->authscript = authscript;
		return true;
	}

	bool url_worker::init_extra(const lyramilk::data::var& extra)
	{
		this->extra = extra;
		return true;
	}

	bool url_worker::check_auth(session_info* si,bool* ret) const
	{
		if(authtype.empty()) return true; 

		{
			lyramilk::script::engines::ptr eng = engine_pool::instance()->get("js")->get();
			if(!eng) return false;
			if(!eng->load_file(authscript)){
				lyramilk::klog(lyramilk::log::warning,"teapoy.web.processer.auth") << D("加载文件%s失败",authscript.c_str()) << std::endl;

				si->rep.send_header_and_length("500 Internal Server Error",strlen("500 Internal Server Error"),0);
				if(ret)*ret = false;
				return false;
			}
			lyramilk::data::var::array ar;
			{
				lyramilk::data::var var_processer_args("__http_session_info",si);

				lyramilk::data::var::array args;
				args.push_back(var_processer_args);

				ar.push_back(eng->createobject("HttpRequest",args));
				ar.push_back(eng->createobject("HttpAuthResponse",args));
			}

			lyramilk::data::var vret = eng->call("auth",ar);
			if(vret.type_like(lyramilk::data::var::t_bool) && (bool)vret){
				return true;
			}
		}
		if(ret)*ret = true;
		return false;
	}

	bool url_worker::test(lyramilk::data::string uri,lyramilk::data::string* real) const
	{
		if(!matcher_regex) return false;
		int ov[256] = {0};
		int rc = pcre_exec((const pcre*)matcher_regex,nullptr,uri.c_str(),uri.size(),0,0,ov,256);
		if(rc > 0){
			if(matcher_dest.type() == lyramilk::data::var::t_str){
				*real = matcher_dest.str();
				std::size_t pos_args = real->find("?");
				if(pos_args!=real->npos){
					*real = real->substr(0,pos_args);
				}
				return true;
			}else if(matcher_dest.type() == lyramilk::data::var::t_array){
				const lyramilk::data::var::array& ar = matcher_dest;
				real->clear();
				for(lyramilk::data::var::array::const_iterator it = ar.begin();it!=ar.end();++it){
					if(it->type() == lyramilk::data::var::t_str){
						real->append(it->str());
					}else if(it->type_like(lyramilk::data::var::t_int)){
						int qi = *it;
						int bof = ov[(qi<<1)];
						int eof = ov[(qi<<1)|1];
						if(eof - bof > 0 && bof < (int)uri.size()){
							real->append(uri.substr(bof,eof-bof));
						}
					}
				}
				std::size_t pos_args = real->find("?");
				if(pos_args!=real->npos){
					*real = real->substr(0,pos_args);
				}
				return true;
			}else if(matcher_dest.type() == lyramilk::data::var::t_invalid){
				real->clear();
				return true;
			}
		}
		return false;
	}

	bool url_worker::try_call(lyramilk::teapoy::http::request* req,std::ostream& os,website_worker& w,bool* ret) const
	{
		lyramilk::data::string real;
		lyramilk::data::string uri;

		webhook_helper hh(*hook);
		if(hh.url_decryption(req,&uri)){
			if(uri.empty()) uri = req->header->uri;
		}else{
			uri = req->header->uri;
		}

		if(!test(uri,&real)) return false;
		if(matcher_dest.type() != lyramilk::data::var::t_invalid){
			struct stat st = {0};
			if(0 !=::stat(real.c_str(),&st)){
				return false;
			}

			if(st.st_mode&S_IFDIR){
				lyramilk::data::string rawdir;
				std::size_t pos_end = real.find_last_not_of("/");
				rawdir = real.substr(0,pos_end + 1);
				rawdir.push_back('/');
				lyramilk::data::strings::const_iterator it = index.begin();
				for(;it!=index.end();++it){
					real = rawdir + *it;
					if(0 == ::stat(real.c_str(),&st) && !(st.st_mode&S_IFDIR)){
						break;
					}
				}
				if (it == index.end()){
					return false;
				}
			}
		}

		req->header->uri = uri;

		session_info si(real,req,os,w,&hh);
		si.rep.set_http_version(req->header->major,req->header->minor);

		if(!check_auth(&si,ret)){
			return true;
		}

		if(ret){	
			*ret = call(&si);
		}else{
			call(&si);
		}
		return true;
	}

	url_worker_loger::url_worker_loger(lyramilk::data::string prefix,lyramilk::teapoy::http::request* req)
	{
		td.mark();
		this->prefix = prefix;
		this->req = req;
	}

	url_worker_loger::~url_worker_loger()
	{
		lyramilk::klog(lyramilk::log::trace,prefix) << D("%s:%u-->%s 耗时%.3f(毫秒)",req->dest().c_str(),req->dest_port(),req->header->uri.c_str(),double(td.diff()) / 1000000) << std::endl;
	}

	/**************** url_worker_master ********************/

	url_worker_master* url_worker_master::instance()
	{
		static url_worker_master _mm;
		return &_mm;
	}

	/**************** website_worker ********************/
	website_worker::website_worker()
	{
	}
	website_worker::~website_worker()
	{
	}

	bool website_worker::try_call(lyramilk::teapoy::http::request* req,std::ostream& os,website_worker& w,bool* ret) const
	{
		std::vector<url_worker*>::const_iterator it = lst.begin();
		for(;it!=lst.end();++it){
			lyramilk::data::string method = it[0]->get_method();
			if(method.find(req->header->method) != method.npos && it[0]->try_call(req,os,w,ret)) return true;
		}
		return false;
	}
	

	/**************** aiohttpsession ********************/

	aiohttpsession::aiohttpsession()
	{
		worker = nullptr;
		//hook_ptr = nullptr;
	}

	aiohttpsession::~aiohttpsession()
	{
	}

	bool aiohttpsession::oninit(std::ostream& os)
	{
		//hook_userdata = hook_ptr?hook_ptr->create_env():nullptr;
		req.init(getfd());
		return true;
	}

	bool aiohttpsession::onrequest(const char* cache,int size,std::ostream& os)
	{
/*
		lyramilk::data::string k = D(" ");
		lyramilk::debug::nsecdiff td;
		lyramilk::debug::clocktester _d(td,lyramilk::klog(lyramilk::log::debug),k);
*/

//std::cout.write(cache,size) << std::endl;

		unsigned int remain = 0;
		if(!req.parse(cache,(unsigned int)size,&remain)){
			return true;
		}

		if(!req.ok()){
			lyramilk::teapoy::http::make_response_header(os,"400 Bad Request",true);
			return false;
		}
		req.ssl_peer_certificate_info = ssl_get_peer_certificate_info();

		if(worker == nullptr){
			lyramilk::teapoy::http::make_response_header(os,"503 Service Temporarily Unavailable",true);
			return false;
		}

		bool bret = false;
		if(!worker->try_call(&req,os,*worker,&bret)){
			lyramilk::teapoy::http::make_response_header(os,"404 Not Found",true,req.header->major,req.header->minor);
			return false;
		}

		if(bret){
			if(req.header->major <= 1 && req.header->minor < 1){
				return false;
			}

			lyramilk::data::string strconnection = lyramilk::teapoy::lowercase(req.header->get("Connection"));
			if(strconnection == "keep-alive"){
				req.reset();
				if(remain != 0){
					//如果多个请求的数据在一个包中，未测试。
					return onrequest(cache+size-remain,remain,os);
				}
				return true;
			}
			return false;
		}
		return false;
	}

}}}
