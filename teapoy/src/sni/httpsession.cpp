#include <libmilk/var.h>
#include <libmilk/log.h>
#include <libmilk/multilanguage.h>
#include <libmilk/codes.h>
#include <libmilk/md5.h>

#include <stdlib.h>

#include "script.h"
#include "stringutil.h"
#include "http.h"
#include "web.h"

#include <sys/stat.h>
#include <errno.h>
#include <fstream>


namespace lyramilk{ namespace teapoy{ namespace native
{
	class httpsession
	{
		lyramilk::log::logss log;
		web::session_info* si;
	  public:
		static void* ctr(const lyramilk::data::var::array& args)
		{
			return new httpsession((web::session_info*)args[0].userdata("__http_session_info"));
		}
		static void dtr(void* p)
		{
			delete (httpsession*)p;
		}

		httpsession(web::session_info* si):log(lyramilk::klog,"teapoy.native.HttpSession")
		{
			this->si = si;
		}

		virtual ~httpsession()
		{
		}

		lyramilk::data::var setAttribute(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			if(args.size() < 2) return false;
			lyramilk::data::string key = args[0];
			lyramilk::data::var value = args[1];
			si->set(key,value);
			return true;
		}

		lyramilk::data::var getAttribute(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string key = args[0];
			return si->get(key);
		}

		lyramilk::data::var todo(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			TODO();
		}

		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["get"] = lyramilk::script::engine::functional<httpsession,&httpsession::getAttribute>;
			fn["set"] = lyramilk::script::engine::functional<httpsession,&httpsession::setAttribute>;
			fn["remove"] = lyramilk::script::engine::functional<httpsession,&httpsession::todo>;
			fn["setMaxInactiveInterval"] = lyramilk::script::engine::functional<httpsession,&httpsession::todo>;
			fn["getId"] = lyramilk::script::engine::functional<httpsession,&httpsession::todo>;
			fn["getCreationTime"] = lyramilk::script::engine::functional<httpsession,&httpsession::todo>;
			p->define("HttpSession",fn,httpsession::ctr,httpsession::dtr);
			return 1;
		}
	};


	class httpresponse
	{

		lyramilk::log::logss log;
		web::session_info* si;
		lyramilk::data::uint32 response_code;

		lyramilk::data::stringstream ss;
	  public:
		static std::map<int,lyramilk::data::string> code_map;
		lyramilk::data::string id;
		lyramilk::data::string sx;

		static void* ctr(const lyramilk::data::var::array& args)
		{
			return new httpresponse((web::session_info*)args[0].userdata("__http_session_info"),200);
		}

		static void* ctr2(const lyramilk::data::var::array& args)
		{
			return new httpresponse((web::session_info*)args[0].userdata("__http_session_info"),0);
		}
		static void dtr(void* p)
		{
			delete (httpresponse*)p;
		}

		httpresponse(web::session_info* si,lyramilk::data::uint32 default_response_code):log(lyramilk::klog,"teapoy.HttpResponse")
		{
			this->si = si;
			si->rep->set("Content-Type","text/html;charset=utf-8");
			response_code = default_response_code;
			/*
			si->rep->set("Access-Control-Allow-Origin","*");
			si->rep->set("Access-Control-Allow-Methods","*");*/
		}

		~httpresponse()
		{
			if(response_code > 0){
				lyramilk::data::string str_body = ss.str();
				lyramilk::data::stringstream ss2;
				if(si->hook->result_encrypt(si,str_body.c_str(),str_body.size(),ss2)){
					lyramilk::data::string str_body = ss2.str();
					si->rep->send_header_and_body(response_code,str_body.c_str(),str_body.size());
				}else{
					si->rep->send_header_and_body(response_code,str_body.c_str(),str_body.size());
				}
			}
		}

		lyramilk::data::var setHeader(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_str);
			lyramilk::data::string field = args[0];
			lyramilk::data::string value = args[1];
			si->rep->set(field,value);
			return true;
		}

		lyramilk::data::var sendError(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_uint);
			response_code = args[0];
			return true;
		}

		lyramilk::data::var sendRedirect(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			TODO();
		}

		lyramilk::data::var write(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			if(args[0].type() == lyramilk::data::var::t_bin){
				lyramilk::data::chunk cb = args[0];
				ss.write((const char*)cb.c_str(),cb.size());
			}else{
				lyramilk::data::string str = args[0];
				ss.write(str.c_str(),str.size());
			}
			return true;
		}

		lyramilk::data::var addCookie(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_map);
			/*	name=value;domain,path,expire,secure,httponly	*/
			//si->req->cookies.push_back(args[0]["name"],args[0]);
			si->req->entityframe->cookies()[args.at(0).at("name")] = args.at(0);
			return true;
		}

		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["setHeader"] = lyramilk::script::engine::functional<httpresponse,&httpresponse::setHeader>;
			fn["sendError"] = lyramilk::script::engine::functional<httpresponse,&httpresponse::sendError>;
			fn["sendRedirect"] = lyramilk::script::engine::functional<httpresponse,&httpresponse::sendRedirect>;
			fn["write"] = lyramilk::script::engine::functional<httpresponse,&httpresponse::write>;
			fn["addCookie"] = lyramilk::script::engine::functional<httpresponse,&httpresponse::addCookie>;
			p->define("HttpResponse",fn,httpresponse::ctr,httpresponse::dtr);
			p->define("HttpAuthResponse",fn,httpresponse::ctr2,httpresponse::dtr);
			return 2;
		}
	};

	class httprequest
	{
		lyramilk::log::logss log;
		web::session_info* si;
		lyramilk::data::var sessionobj;
	  public:
		static void* ctr(const lyramilk::data::var::array& args)
		{
			return new httprequest((web::session_info*)args[0].userdata("__http_session_info"));
		}
		static void dtr(void* p)
		{
			delete (httprequest*)p;
		}

		httprequest(web::session_info* si):log(lyramilk::klog,"teapoy.HttpRequest")
		{
			this->si = si;
		}

		virtual ~httprequest()
		{
		}

		lyramilk::data::var getHeader(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string field = args[0];
			return this->si->req->entityframe->get(field);
		}

		lyramilk::data::var getHeaders(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			return this->si->req->entityframe->header();
		}

		lyramilk::data::var getParameter(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string urlkey = args[0];

			lyramilk::data::var::map& params = this->si->req->entityframe->params();
			lyramilk::data::var::map::iterator it = params.find(urlkey);
			if(it!=params.end()){
				lyramilk::data::var::array& ar0 = it->second;
				for(lyramilk::data::var::array::iterator it0 = ar0.begin();it0!=ar0.end();++it0){
					return it0->str();
				}
			}
			return lyramilk::data::var::nil;
		}

		lyramilk::data::var getParameterRaw(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string urlkey = args[0];

			lyramilk::data::var::map& params = this->si->req->entityframe->params();
			lyramilk::data::var::map::iterator it = params.find(urlkey);
			if(it!=params.end()){
				return it->second;
			}
			return lyramilk::data::var::nil;
		}
		lyramilk::data::var getParameterValues(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			return this->si->req->entityframe->params();
		}

		lyramilk::data::var getFiles(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string str = args[0];
			lyramilk::data::string rawdir;
			std::size_t pos_end = str.find_last_not_of("/");
			rawdir = str.substr(0,pos_end + 1);
			rawdir.push_back('/');

			lyramilk::data::var::array ret;

			long max = this->si->req->entityframe->childcount();

			srand(time(0));

			for(long i=0;i<max;++i){
				lyramilk::teapoy::http::http_resource* res = this->si->req->entityframe->child(i);

				lyramilk::data::string disposition = res->get("content-disposition");
				lyramilk::data::strings fields = lyramilk::teapoy::split(disposition,";");
				lyramilk::data::strings::iterator it = fields.begin();

				lyramilk::data::var::map fileiteminfo;
				for(;it!=fields.end();++it){
					std::size_t pos_eq = it->find("=");
					if(pos_eq == it->npos || pos_eq == it->size()) continue;
					lyramilk::data::string k = lyramilk::teapoy::trim(it->substr(0,pos_eq)," \t'\"");
					lyramilk::data::string v = lyramilk::teapoy::trim(it->substr(pos_eq + 1)," \t'\"");
					if(k.size() | v.size()){
						fileiteminfo[k] = v;
					}
				}

				lyramilk::data::var::map::iterator eit = fileiteminfo.find("filename");
				if(eit != fileiteminfo.end()){
					lyramilk::data::string str = eit->second;
					if(str.empty()) continue;
				}else{
					continue;
				}


				lyramilk::data::string rawname = fileiteminfo["filename"].str();
				lyramilk::data::string extname;
				{
					std::size_t pos = rawname.rfind(".");
					if(pos != rawname.npos){
						if(rawname.find('/',pos) == rawname.npos){
							extname = rawname.substr(pos);
						}
					}
				}
				lyramilk::data::string filepath;
				lyramilk::data::string filename;
				int r = 0;
				do{
					struct stat st = {0};
					lyramilk::data::stringstream ss;
					ss << rand() << rand() << rand();
					filename = ss.str();
					if(filename.size() < 18) continue;
					lyramilk::cryptology::md5 c1;
					c1 << filename;

					filename = c1.get_key().str16() + filename + extname;

					filepath = rawdir + filename;
					r = stat(filepath.c_str(),&st);
				}while(r == 0);
				if(r == -1 && errno != ENOENT) return lyramilk::data::var::nil;

				std::ofstream ofs;
				ofs.open(filepath.c_str(),std::ofstream::binary|std::ofstream::out);
				if(ofs.is_open()){
					ofs.write(res->body->ptr(),res->body->size());
					ofs.close();
					lyramilk::data::var::map fileitem;
					fileitem["path"] = filepath;
					fileitem["filename"] = filename;
					fileitem["header"].type(lyramilk::data::var::t_map);
					lyramilk::data::var::map& m = fileitem["header"];

					lyramilk::data::var::map requestheader = res->header();
					lyramilk::data::var::map::const_iterator it = requestheader.begin();
					for(;it!=requestheader.end();++it){
						m[it->first] = it->second;
					}
					fileitem["fieldinfo"] =  fileiteminfo;
					fileitem["filesize"] = res->body->size();
					ret.push_back(fileitem);
				}
			}
			return ret;
		}

		lyramilk::data::var getURI(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			return lyramilk::data::codes::instance()->decode("urlcomponent",si->req->entityframe->uri);
		}

		lyramilk::data::var getMethod(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			return si->req->entityframe->method;
		}

		lyramilk::data::var getRequestBody(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			return lyramilk::data::chunk((const unsigned char*)si->req->entityframe->body->ptr(),si->req->entityframe->body->size());
		}

		lyramilk::data::var getRealPath(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			return si->real;
		}

		lyramilk::data::var getSession(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			if(sessionobj.type() == lyramilk::data::var::t_user){
				return sessionobj;
			}

			lyramilk::data::string sid = si->getsid();
			if(sid.empty()) return lyramilk::data::var::nil;

			lyramilk::data::var::array ar;
			lyramilk::data::var v("__http_session_info",si);
			ar.push_back(v);
			lyramilk::script::engine* e = (lyramilk::script::engine*)env.find(lyramilk::script::engine::s_env_engine())->second.userdata(lyramilk::script::engine::s_env_engine());
			sessionobj = e->createobject("HttpSession",ar);
			return sessionobj;
		}

		lyramilk::data::var getPeerCertificateInfo(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			return si->req->ssl_peer_certificate_info;
		}

		lyramilk::data::var getRemoteAddr(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			lyramilk::data::var::map m;
			m["host"] = si->req->dest();
			m["port"] = si->req->dest_port();
			return m;
		}

		lyramilk::data::var getLocalAddr(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			lyramilk::data::var::map m;
			m["host"] = si->req->source();
			m["port"] = si->req->source_port();
			return m;
		}

		lyramilk::data::var wget(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string url = args[0].str();
			lyramilk::data::stringstream ssrequest;
			ssrequest << "GET " << url << " HTTP/1.1\r\n";
			ssrequest << "\r\n";

			lyramilk::data::string strrequest = ssrequest.str();
			lyramilk::teapoy::web::aiohttpsession sns;
			sns.worker = &si->worker;
			lyramilk::data::stringstream ssresponse;

			sns.ssl_set_peer_certificate_info(si->req->ssl_peer_certificate_info);
			sns.onrequest(strrequest.c_str(),strrequest.size(),ssresponse);
			lyramilk::data::string ret = ssresponse.str();
			std::size_t sz = ret.find("\r\n\r\n");
			if(sz == ret.npos){
				return lyramilk::data::var::nil;
			}
			return ret.substr(sz);
		}

		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["getHeader"] = lyramilk::script::engine::functional<httprequest,&httprequest::getHeader>;
			fn["getHeaders"] = lyramilk::script::engine::functional<httprequest,&httprequest::getHeaders>;
			fn["getParameter"] = lyramilk::script::engine::functional<httprequest,&httprequest::getParameter>;
			fn["getParameterRaw"] = lyramilk::script::engine::functional<httprequest,&httprequest::getParameterRaw>;
			fn["getParameterValues"] = lyramilk::script::engine::functional<httprequest,&httprequest::getParameterValues>;
			fn["getFiles"] = lyramilk::script::engine::functional<httprequest,&httprequest::getFiles>;
			fn["getURI"] = lyramilk::script::engine::functional<httprequest,&httprequest::getURI>;
			fn["getMethod"] = lyramilk::script::engine::functional<httprequest,&httprequest::getMethod>;
			fn["getRequestBody"] = lyramilk::script::engine::functional<httprequest,&httprequest::getRequestBody>;
			fn["getRealPath"] = lyramilk::script::engine::functional<httprequest,&httprequest::getRealPath>;
			fn["getSession"] = lyramilk::script::engine::functional<httprequest,&httprequest::getSession>;
			fn["getPeerCertificateInfo"] = lyramilk::script::engine::functional<httprequest,&httprequest::getPeerCertificateInfo>;
			fn["getRemoteAddr"] = lyramilk::script::engine::functional<httprequest,&httprequest::getRemoteAddr>;
			fn["getLocalAddr"] = lyramilk::script::engine::functional<httprequest,&httprequest::getLocalAddr>;
			fn["wget"] = lyramilk::script::engine::functional<httprequest,&httprequest::wget>;
			p->define("HttpRequest",fn,httprequest::ctr,httprequest::dtr);
			return 1;
		}
	};

	static int define(lyramilk::script::engine* p)
	{
		int i = 0;
		i+= httprequest::define(p);
		i+= httpresponse::define(p);
		i+= httpsession::define(p);
		return i;
	}


	static __attribute__ ((constructor)) void __init()
	{
		lyramilk::teapoy::script2native::instance()->regist("http",define);
	}

}}}