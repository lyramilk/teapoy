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

	/**************** session_info ********************/
	session_info::session_info(lyramilk::data::string realfile,lyramilk::teapoy::http::request* req,std::ostream& argos,website_worker& w):os(argos),worker(w)
	{
		this->real = realfile;
		this->req = req;
	}

	session_info::~session_info()
	{
	}

	const lyramilk::data::string& session_info::getsid()
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

	lyramilk::data::var& session_info::get(const lyramilk::data::string& key)
	{
		return lyramilk::teapoy::web::sessions::instance()->get(req->dest + getsid(),key);
	}

	void session_info::set(const lyramilk::data::string& key,const lyramilk::data::var& value)
	{
		lyramilk::teapoy::web::sessions::instance()->set(req->dest + getsid(),key,value);
	}
	/**************** url_worker ********************/
	url_worker::url_worker()
	{
		matcher_regex = nullptr;
	}

	url_worker::~url_worker()
	{}

	lyramilk::data::string url_worker::get_method()
	{
		return method;
	}

	bool url_worker::init(lyramilk::data::string method,lyramilk::data::string pattern,lyramilk::data::string real,lyramilk::data::var::array index)
	{
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
				matcher_dest = ar[0];
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
		return false;
	}

	bool url_worker::check_auth(lyramilk::teapoy::http::request* req,std::ostream& os,website_worker& w,bool* ret) const
	{
		if(authtype.empty()) return true;

		req->parse_cookies();

		session_info si("",req,os,w);
		lyramilk::data::var v = si.get("http.digest.user");
		if(v.type() == lyramilk::data::var::t_str){
			return true;
		}

		lyramilk::data::var::array ar;
		lyramilk::data::string authorization_str = req->get("authorization");
		lyramilk::teapoy::strings strs = lyramilk::teapoy::split(authorization_str,",");
		lyramilk::data::var::map logininfo;
		lyramilk::teapoy::strings::iterator it = strs.begin();
		for(;it!=strs.end();++it){
			std::size_t pos = it->find('=');
			if(pos != it->npos){
				lyramilk::data::string str1 = it->substr(0,pos);
				lyramilk::data::string str2 = it->substr(pos+1);
				logininfo[lyramilk::teapoy::trim(str1," \"'")] = lyramilk::teapoy::trim(str2," \"'");
			}
		}

		logininfo["method"] = req->method;
		//不使用客户端传过来的nonce，因为这个值有可能是伪造的。
		logininfo["nonce"] = si.get("http.digest.nonce");

		ar.push_back(logininfo);
		lyramilk::script::engines::ptr eng = engine_pool::instance()->get("js")->get();
		if(!eng->load_file(authscript)){
			lyramilk::teapoy::http::make_response_header(os,"500 Internal Server Error",true,req->ver.major,req->ver.minor);
			if(ret)*ret = false;
			return false;
		}
		lyramilk::data::var vret = eng->call("auth",ar);
		if(vret == true){
			si.set("http.digest.user",logininfo["Digest username"]);
			return true;
		}
		if(vret.type() != lyramilk::data::var::t_map){
			lyramilk::teapoy::http::make_response_header(os,"500 Internal Server Error",true,req->ver.major,req->ver.minor);
			if(ret)*ret = false;
			return false;
		}

		if(logininfo["nc"].type() == lyramilk::data::var::t_invalid){
			logininfo["nc"] = 0;
		}
		lyramilk::data::string digest = vret["digest"];
		lyramilk::data::string realm = vret["realm"];
		lyramilk::data::string algorithm = vret["algorithm"];
		lyramilk::data::string nonce = vret["nonce"].str();
		lyramilk::data::uint64 nc = logininfo["nc"];
		si.set("http.digest.nonce",nonce);

		lyramilk::data::stringstream ss;

		lyramilk::teapoy::http::make_response_header(ss,"401 Authorization Required",false,req->ver.major,req->ver.minor);
		ss <<	"Connection: Keep-alive\r\n"
				"Content-Length: 0\r\n"
				"WWW-Authenticate: Digest username=\"" << digest << "\", realm=\"" << realm << "\", nonce=\"" << nonce << "\", algorithm=\"" << algorithm << "\", nc=" << nc << ", qop=\"auth\"\r\n"
				"Set-Cookie: TeapoyId=" << si.getsid() << "\r\n"
				"\r\n";
		os << ss.str();
		if(ret)*ret = true;
		return false;
	}

	bool url_worker::test(lyramilk::teapoy::http::request* req,std::ostream& os,lyramilk::data::string& real,website_worker& w) const
	{
		lyramilk::data::string url = req->url_pure;
		if(!matcher_regex) return false;
		int ov[256] = {0};
		int rc = pcre_exec((const pcre*)matcher_regex,nullptr,url.c_str(),url.size(),0,0,ov,256);
		if(rc > 0){
			if(matcher_dest.type() == lyramilk::data::var::t_str){
				real = matcher_dest.str();
				std::size_t pos_args = real.find("?");
				if(pos_args!=real.npos){
					real = real.substr(0,pos_args);
				}
				return true;
			}else if(matcher_dest.type() == lyramilk::data::var::t_array){
				const lyramilk::data::var::array& ar = matcher_dest;
				real.clear();
				for(lyramilk::data::var::array::const_iterator it = ar.begin();it!=ar.end();++it){
					if(it->type() == lyramilk::data::var::t_str){
						real.append(it->str());
					}else if(it->type_like(lyramilk::data::var::t_int)){
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
				return true;
			}
		}
		return false;
	}

	bool url_worker::try_call(lyramilk::teapoy::http::request* req,std::ostream& os,website_worker& w,bool* ret) const
	{
		lyramilk::data::string real;
		if(!test(req,os,real,w)) return false;
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

		if(!check_auth(req,os,w,ret)){
			return true;
		}

		if(ret){	
			*ret = call(req,os,real,w);
		}else{
			call(req,os,real,w);
		}
		return true;
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
			if(method.find(req->method) != method.npos && it[0]->try_call(req,os,w,ret)) return true;
		}
		return false;
	}
	

	/**************** aiohttpsession ********************/

	aiohttpsession::aiohttpsession()
	{
		worker = nullptr;
	}

	aiohttpsession::~aiohttpsession()
	{
	}

	bool aiohttpsession::oninit(std::ostream& os)
	{
		lyramilk::netio::netaddress addr_dest = dest();
		lyramilk::netio::netaddress addr_source = source();

		req.source = addr_source.ip_str();
		req.source_port = addr_dest.port;

		req.dest = addr_dest.ip_str();
		req.dest_port = addr_dest.port;
		return true;
	}

	bool aiohttpsession::onrequest(const char* cache,int size,std::ostream& os)
	{
//std::cout.write(cache,size) << std::endl;
		int remain = 0;
		if(!req.parse(cache,size,&remain)){
			return true;
		}
		/*
		lyramilk::data::string k = D("%s ",req.url.c_str());
		lyramilk::debug::nsecdiff td;
		lyramilk::debug::clocktester _d(td,lyramilk::klog(lyramilk::log::debug),k);
*/
		if(req.bad()){
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
			lyramilk::teapoy::http::make_response_header(os,"404 Not Found",true,req.ver.major,req.ver.minor);
			return false;
		}

		if(bret){
			if(req.ver.major <= 1 && req.ver.minor < 1){
				return false;
			}

			lyramilk::data::var vconnection = req.get("Connection");
			lyramilk::data::var strconnection;
			if(vconnection.type_like(lyramilk::data::var::t_str)){
				strconnection = lyramilk::teapoy::lowercase(vconnection.str());
			}
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
