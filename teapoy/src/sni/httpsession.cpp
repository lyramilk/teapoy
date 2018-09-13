#include <libmilk/var.h>
#include <libmilk/log.h>
#include <libmilk/multilanguage.h>
#include <libmilk/codes.h>
#include <libmilk/md5.h>

#include <stdlib.h>

#include "script.h"
#include "stringutil.h"
#include "webservice.h"

#include <sys/stat.h>
#include <errno.h>
#include <fstream>

#include "teapoy_gzip.h"
#include "httplistener.h"
#include "upstream_caller.h"

namespace lyramilk{ namespace teapoy{ namespace native
{
	class httpsession
	{
		lyramilk::log::logss log;
	  public:
		httpadapter* adapter;
		lyramilk::teapoy::httpsession* session;
		static void* ctr(const lyramilk::data::var::array& args)
		{
			httpadapter* adapter = (httpadapter*)args[0].userdata("..http.session.adapter");
			if(adapter == nullptr) return nullptr;
			lyramilk::teapoy::httpsession* session = nullptr;
			
			lyramilk::data::string sid = adapter->get_cookie("TeapoyId");

			//为了含义明确
			if(sid.empty()){
				session = adapter->service->session_mgr->get_session(sid);
			}else{
				session = adapter->service->session_mgr->create_session();
			}
			
			if(session == nullptr) return nullptr;

			if(sid.empty()){
				httpcookie c;
				c.key = "TeapoyId";
				c.value = session->get_session_id();
				c.httponly = true;
				adapter->set_cookie_obj(c);
			}

			httpsession* snisession = new httpsession(adapter,session);
			return snisession;
		}
		static void dtr(void* p)
		{
			delete (httpsession*)p;
		}

		httpsession(httpadapter* adapter,lyramilk::teapoy::httpsession* session):log(lyramilk::klog,"teapoy.native.HttpSession")
		{
			this->adapter = adapter;
			this->session = session;
		}

		virtual ~httpsession()
		{
		}

		lyramilk::data::var setAttribute(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_str);
			if(args.size() < 2) return false;
			return session->set(args[0].str(),args[1].str());
		}

		lyramilk::data::var getAttribute(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			return session->get(args[0].str());
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
		lyramilk::data::uint32 response_code;
		httpadapter* adapter;

		lyramilk::data::stringstream ss;
		bool noreply;
	  public:
		static std::map<int,lyramilk::data::string> code_map;
		lyramilk::data::string id;
		lyramilk::data::string sx;

		static void* ctr(const lyramilk::data::var::array& args)
		{
			return new httpresponse((httpadapter*)args[0].userdata("..http.session.adapter"),200);
		}

		static void* ctr2(const lyramilk::data::var::array& args)
		{
			return new httpresponse((httpadapter*)args[0].userdata("..http.session.adapter"),0);
		}
		static void dtr(void* p)
		{
			delete (httpresponse*)p;
		}

		httpresponse(httpadapter* adapter,lyramilk::data::uint32 default_response_code):log(lyramilk::klog,"teapoy.native.HttpResponse")
		{
			noreply = false;
			this->adapter = adapter;
			response_code = default_response_code;
			this->adapter->response->set("Content-Type","text/html;charset=utf-8");
		}

		~httpresponse()
		{
			if(response_code > 0){
				bool usedgzip = false;
#ifdef ZLIB_FOUND
				{
					std::size_t datasize = ss.str().size();
					lyramilk::data::string sacceptencoding = adapter->request->get("Accept-Encoding");
					if(sacceptencoding.find("gzip") != sacceptencoding.npos && datasize > 512){
						adapter->response->set("Content-Encoding","gzip");
						adapter->send_header_with_chunk(adapter->response,response_code);
						lyramilk::teapoy::http_chunked_gzip(ss,datasize,adapter);
						usedgzip = true;
					}
				}
#endif
				if(!usedgzip){
					lyramilk::data::string str_body = ss.str();
					adapter->send_header_with_length(adapter->response,response_code,str_body.size());
					adapter->send_bodydata(adapter->response,str_body.c_str(),str_body.size());
				}
			}
		}

		lyramilk::data::var setHeader(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_str);
			lyramilk::data::string field = args[0];
			lyramilk::data::string value = args[1];
			adapter->request->set(field,value);
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
			TODO();
		}

		lyramilk::data::var async_url_get(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_str);

			lyramilk::data::string url = args[0].str();
			if(url.compare(0,2,"//") == 0){
				url = adapter->request->scheme() + ":"+ url;
			}else if(url.compare(0,1,"/") == 0){
				url = adapter->request->scheme() + "://" +  adapter->request->get("host") + url;
			}
			upstream_caller* caller = upstream_caller::ctr();
			lyramilk::data::stringdict params;
			caller->rcall(url,params,adapter);

			adapter->service->get_aio_pool()->add(caller);

			adapter->channel->lock();
			response_code = 0;
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
			fn["async_url_get"] = lyramilk::script::engine::functional<httpresponse,&httpresponse::async_url_get>;
			p->define("HttpResponse",fn,httpresponse::ctr,httpresponse::dtr);
			p->define("HttpAuthResponse",fn,httpresponse::ctr2,httpresponse::dtr);
			return 2;
		}
	};

	class httprequest
	{
		lyramilk::log::logss log;
		httpadapter* adapter;
		lyramilk::data::var sessionobj;
	  public:
		static void* ctr(const lyramilk::data::var::array& args)
		{
			httpadapter* adapter = (httpadapter*)args[0].userdata("..http.session.adapter");
			if(adapter == nullptr) return nullptr;
			return new httprequest(adapter);
		}
		static void dtr(void* p)
		{
			delete (httprequest*)p;
		}

		httprequest(httpadapter* adapter):log(lyramilk::klog,"teapoy.native.HttpRequest")
		{
			this->adapter = adapter;
		}

		virtual ~httprequest()
		{
		}

		lyramilk::data::var getHeader(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string key = args[0].str();
			return adapter->request->get(key);
		}

		lyramilk::data::var getHeaders(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			return adapter->request->header;
		}

		lyramilk::data::var getParameter(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string key = args[0].str();
			const lyramilk::data::var::map& m = adapter->request->params();
			lyramilk::data::var::map::const_iterator it = m.find(key);
			if(it == m.end()) return lyramilk::data::var::nil;
			if(it->second.type() != lyramilk::data::var::t_array) return lyramilk::data::var::nil;
			const lyramilk::data::var::array& ar = it->second;
			if(ar.size() == 0) return lyramilk::data::var::nil;
			return ar[0].str();
		}

		lyramilk::data::var getParameterRaw(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			TODO();
		}
		lyramilk::data::var getParameterValues(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			TODO();
		}

		lyramilk::data::var getFiles(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			TODO();
		}

		lyramilk::data::var getURI(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			TODO();
		}

		lyramilk::data::var getMethod(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			TODO();
		}

		lyramilk::data::var getRequestBody(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			TODO();
		}

		lyramilk::data::var getRealPath(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			TODO();
		}

		lyramilk::data::var getSession(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			if(sessionobj.type() == lyramilk::data::var::t_user){
				return sessionobj;
			}

			lyramilk::data::var::array ar;
			lyramilk::data::var v("..http.session.adapter",adapter);
			ar.push_back(v);
			lyramilk::script::engine* e = (lyramilk::script::engine*)env.find(lyramilk::script::engine::s_env_engine())->second.userdata(lyramilk::script::engine::s_env_engine());
			sessionobj = e->createobject("HttpSession",ar);
			return sessionobj;
		}

		lyramilk::data::var getPeerCertificateInfo(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			TODO();
		}

		lyramilk::data::var getRemoteAddr(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			TODO();
		}

		lyramilk::data::var getLocalAddr(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			TODO();
		}

		lyramilk::data::var wget(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			TODO();
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
		lyramilk::teapoy::script_interface_master::instance()->regist("http",define);
	}

}}}