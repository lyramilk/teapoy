#include <libmilk/var.h>
#include <libmilk/log.h>
#include <libmilk/multilanguage.h>
#include <stdlib.h>
#include <libmilk/codes.h>
#include "script.h"
#include "stringutil.h"
#include "processer_master.h"
#include "http.h"


namespace lyramilk{ namespace teapoy{ namespace native
{
	class SessionMgr
	{
		struct timedmap
		{
			lyramilk::data::var::map m;
			time_t tm;
			timedmap()
			{
				tm = time(0);
			}
		};
		time_t st;
		std::map<lyramilk::data::string,timedmap> k;
	  public:
		SessionMgr()
		{
			st = time(0);
		}
		virtual ~SessionMgr()
		{}
		lyramilk::data::var& get(const lyramilk::data::string& id,const lyramilk::data::string& key)
		{
			gc();
			timedmap& t = k[id];
			t.tm = time(0);
			return t.m[key];
		}

		void gc()
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

		void set(const lyramilk::data::string& id,const lyramilk::data::string& key,const lyramilk::data::var& value)
		{
			gc();
			timedmap& t = k[id];
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


		lyramilk::data::string newid()
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

		static SessionMgr* instance()
		{
			static SessionMgr _mm;
			return &_mm;
		}
	};

	class httpsession
	{
		lyramilk::log::logss log;
		lyramilk::data::string id;
	  public:
		static void* ctr(lyramilk::data::var::array args)
		{
			return new httpsession(args[0]);
		}
		static void dtr(void* p)
		{
			delete (httpsession*)p;
		}

		httpsession(lyramilk::data::string id):log(lyramilk::klog,"teapoy.native.HttpSession")
		{
			this->id = id;
		}

		virtual ~httpsession()
		{
		}

		lyramilk::data::var setAttribute(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			if(args.size() < 2) return false;
			lyramilk::data::string key = args[0];
			lyramilk::data::var value = args[1];
			SessionMgr::instance()->set(id,key,value);
			return true;
		}

		lyramilk::data::var getAttribute(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string key = args[0];
			return SessionMgr::instance()->get(id,key);
		}

		lyramilk::data::var todo(lyramilk::data::var::array args,lyramilk::data::var::map env)
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
		web::processer_args* proc_args;

		lyramilk::data::stringstream ss;
		lyramilk::data::uint32 response_code;
		lyramilk::data::var::map header;
	  public:
		static std::map<int,lyramilk::data::string> code_map;
		lyramilk::data::string id;
		lyramilk::data::string sx;

		static void* ctr(lyramilk::data::var::array args)
		{
			//assert(args.size() > 0);
			return new httpresponse((web::processer_args*)args[0].userdata("__http_processer_args"));
		}
		static void dtr(void* p)
		{
			delete (httpresponse*)p;
		}

		httpresponse(web::processer_args* proc_args):log(lyramilk::klog,"teapoy.HttpResponse")
		{
			this->proc_args = proc_args;
			response_code = 200;
		}

		~httpresponse()
		{
			header["Server"] = SERVER_VER;
			header["Content-Type"] = "text/html;charset=utf-8";

			lyramilk::data::stringstream ss_header;
			lyramilk::data::string str_body;
			if(response_code == 200){
				ss_header << "HTTP/1.1 200 OK\r\n";
				str_body = ss.str();
				header["Content-Length"] = str_body.size();
			}else{
				ss_header << "HTTP/1.1 " << http::get_error_code_desc(response_code) << "\r\n";
			}
			lyramilk::data::var::map::iterator it = header.begin();
			for(;it!=header.end();++it){
				ss_header << it->first << ": " << it->second << "\r\n";
			}

			{
				lyramilk::data::var::map::iterator it = proc_args->req->cookies.begin();
				for(;it!=proc_args->req->cookies.end();++it){
					try{
						lyramilk::data::var::map& m = it->second;
						lyramilk::data::string value = m["value"];
						ss_header << "Set-Cookie:" << it->first << "=" << value << ";" << "\r\n";
					}catch(...){
					}
				}
			}
			ss_header << "\r\n";
			proc_args->os << ss_header.str() << str_body;
			proc_args->os.flush();
		}

		lyramilk::data::var setHeader(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_str);
			lyramilk::data::string field = args[0];
			lyramilk::data::string value = args[1];
			header[field] = value;
			return true;
		}

		lyramilk::data::var sendError(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_uint32);
			response_code = args[0];
			return true;
		}

		lyramilk::data::var sendRedirect(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			TODO();
		}

		lyramilk::data::var write(lyramilk::data::var::array args,lyramilk::data::var::map env)
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

		lyramilk::data::var addCookie(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_map);
			/*	name=value;domain,path,expire,secure,httponly	*/
			//proc_args->req->cookies.push_back(args[0]["name"],args[0]);
			proc_args->req->cookies[args[0]["name"]] = args[0];
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
			return 1;
		}
	};

	class httprequest
	{
		lyramilk::log::logss log;
		web::processer_args* proc_args;
		/*
		lyramilk::data::var::map header;
		lyramilk::data::var::map request_args;
		lyramilk::data::string path;
		lyramilk::data::var::map cookies;
		httpresponse* response;*/
	  public:
		static void* ctr(lyramilk::data::var::array args)
		{
			return new httprequest((web::processer_args*)args[0].userdata("__http_processer_args"));
		}
		static void dtr(void* p)
		{
			delete (httprequest*)p;
		}

		httprequest(web::processer_args* proc_args):log(lyramilk::klog,"teapoy.HttpRequest")
		{
			this->proc_args = proc_args;
		}

		virtual ~httprequest()
		{
		}

		lyramilk::data::var getHeader(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string field = args[0];
			return this->proc_args->req->get(field);
		}

		lyramilk::data::var getHeaders(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			return this->proc_args->req->header;
		}

		lyramilk::data::var getParameter(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string urlkey = args[0];

			lyramilk::data::var::map::iterator it = this->proc_args->req->parameter.find(urlkey);
			if(it!=this->proc_args->req->parameter.end()){
				lyramilk::data::var::array& ar0 = it->second;
				for(lyramilk::data::var::array::iterator it0 = ar0.begin();it0!=ar0.end();++it0){
					return it0->str();
				}
			}
			return lyramilk::data::var::nil;
		}

		lyramilk::data::var getParameterRaw(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string urlkey = args[0];

			lyramilk::data::var::map::iterator it = this->proc_args->req->parameter.find(urlkey);
			if(it!=this->proc_args->req->parameter.end()){
				return it->second;
			}
			return lyramilk::data::var::nil;
		}

		lyramilk::data::var getParameterValues(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			return this->proc_args->req->parameter;
		}

		lyramilk::data::var getRequestURL(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			return lyramilk::data::codes::instance()->decode(proc_args->req->url,"urlcomponent");
		}

		lyramilk::data::var getRealPath(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			return proc_args->real;
		}

		lyramilk::data::var getSession(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			if(proc_args->session.type() == lyramilk::data::var::t_user) return proc_args->session;
			lyramilk::data::var::map::iterator it = proc_args->req->cookies.find("TeapoyId");
			lyramilk::data::string id;

			if(it!=proc_args->req->cookies.end()){
				if(it->second.type() == lyramilk::data::var::t_map){
					id = it->second.at("value").str();
				}
				if(it->second.type_compat(lyramilk::data::var::t_str)){
					id = it->second.str();
				}
			}
			if(id.empty()){
				id = SessionMgr::instance()->newid();
				lyramilk::data::var v;
				v.type(lyramilk::data::var::t_map);
				v["value"] = id;

				proc_args->req->cookies["TeapoyId"] = v;
			}

			lyramilk::data::var::array ar;
			ar.push_back(id);
			lyramilk::script::engine* e = (lyramilk::script::engine*)env[lyramilk::script::engine::s_env_engine()].userdata(lyramilk::script::engine::s_user_engineptr());
			proc_args->session = e->createobject("HttpSession",ar);
			return proc_args->session;
		}

		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["getHeader"] = lyramilk::script::engine::functional<httprequest,&httprequest::getHeader>;
			fn["getHeaders"] = lyramilk::script::engine::functional<httprequest,&httprequest::getHeaders>;
			fn["getParameter"] = lyramilk::script::engine::functional<httprequest,&httprequest::getParameter>;
			fn["getParameterRaw"] = lyramilk::script::engine::functional<httprequest,&httprequest::getParameterRaw>;
			fn["getParameterValues"] = lyramilk::script::engine::functional<httprequest,&httprequest::getParameterValues>;
			fn["getRequestURL"] = lyramilk::script::engine::functional<httprequest,&httprequest::getRequestURL>;
			fn["getRealPath"] = lyramilk::script::engine::functional<httprequest,&httprequest::getRealPath>;
			fn["getSession"] = lyramilk::script::engine::functional<httprequest,&httprequest::getSession>;
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