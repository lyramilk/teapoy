#include <libmilk/var.h>
#include <libmilk/log.h>
#include <libmilk/dict.h>
#include <libmilk/codes.h>
#include <libmilk/md5.h>

#include <stdlib.h>

#include "script.h"
#include "stringutil.h"
#include "webservice.h"

#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <fstream>

#include "teapoy_gzip.h"
#include "httplistener.h"
#include "upstream_caller.h"

#include <fcntl.h>
#include "util.h"

namespace lyramilk{ namespace teapoy{ namespace native
{
	class httpsession:public lyramilk::script::sclass
	{
		lyramilk::log::logss log;
	  public:
		httpadapter* adapter;
		lyramilk::teapoy::httpsessionptr session;
		static lyramilk::script::sclass* ctr(const lyramilk::data::array& args)
		{
			if(args.size() > 0 && args[0].type() == lyramilk::data::var::t_user){
				lyramilk::data::datawrapper* urd = args[0].userdata();
				if(urd && urd->name() == lyramilk::teapoy::session_response_datawrapper::class_name()){
					lyramilk::teapoy::session_response_datawrapper* urdp = (lyramilk::teapoy::session_response_datawrapper*)urd;

					if(urdp->adapter == nullptr) return nullptr;
					lyramilk::teapoy::httpsessionptr session = urdp->adapter->get_session();

					if(session == nullptr) return nullptr;
					return new httpsession(urdp->adapter,session);
				}
			}
			return nullptr;
		}
		static void dtr(lyramilk::script::sclass* p)
		{
			delete (httpsession*)p;
		}

		httpsession(httpadapter* adapter,lyramilk::teapoy::httpsessionptr& session):log(lyramilk::klog,"teapoy.native.HttpSession")
		{
			this->adapter = adapter;
			this->session = session;
		}

		~httpsession()
		{
		}

		lyramilk::data::var setAttribute(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_any);
			if(args.size() < 2) return false;
			return session->set(args[0].str(),args[1].str());
		}

		lyramilk::data::var getAttribute(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			return session->get(args[0].str());
		}

		lyramilk::data::var removeAttribute(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			return session->set(args[0].str(),lyramilk::data::var::nil);
		}

		lyramilk::data::var todo(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			TODO();
		}

		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["get"] = lyramilk::script::engine::functional<httpsession,&httpsession::getAttribute>;
			fn["set"] = lyramilk::script::engine::functional<httpsession,&httpsession::setAttribute>;
			fn["remove"] = lyramilk::script::engine::functional<httpsession,&httpsession::removeAttribute>;
			fn["del"] = lyramilk::script::engine::functional<httpsession,&httpsession::removeAttribute>;
			//fn["setMaxInactiveInterval"] = lyramilk::script::engine::functional<httpsession,&httpsession::todo>;
			//fn["getId"] = lyramilk::script::engine::functional<httpsession,&httpsession::todo>;
			//fn["getCreationTime"] = lyramilk::script::engine::functional<httpsession,&httpsession::todo>;
			p->define("HttpSession",fn,httpsession::ctr,httpsession::dtr);
			return 1;
		}
	};


	class httpresponse:public lyramilk::script::sclass
	{

		lyramilk::log::logss log;
		httpadapter* adapter;

		std::ostream& os;
		bool noreply;
	  public:
		static std::map<int,lyramilk::data::string> code_map;
		lyramilk::data::string id;
		lyramilk::data::string sx;

		static lyramilk::script::sclass* ctr(const lyramilk::data::array& args)
		{

			if(args.size() > 0 && args[0].type() == lyramilk::data::var::t_user){
				lyramilk::data::datawrapper* urd = args[0].userdata();
				if(urd && urd->name() == lyramilk::teapoy::session_response_datawrapper::class_name()){
					lyramilk::teapoy::session_response_datawrapper* urdp = (lyramilk::teapoy::session_response_datawrapper*)urd;

					if(urdp->os) return new httpresponse(urdp->adapter,urdp->os);
				}
			}
			return nullptr;
		}

		static void dtr(lyramilk::script::sclass* p)
		{
			delete (httpresponse*)p;
		}

		httpresponse(httpadapter* adapter,std::ostream& _os):log(lyramilk::klog,"teapoy.native.HttpResponse"),os(_os)
		{
			noreply = false;
			this->adapter = adapter;
			this->adapter->response->code = 200;
		}

		~httpresponse()
		{
		}

		lyramilk::data::var setHeader(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_str);
			lyramilk::data::string field = args[0];
			lyramilk::data::string value = args[1];
			adapter->response->set(field,value);
			return true;
		}

		lyramilk::data::var sendError(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_uint);
			adapter->response->code = args[0];
			return true;
		}

		lyramilk::data::var sendRedirect(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			TODO();
		}

		lyramilk::data::var write(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);

			if(args[0].type() == lyramilk::data::var::t_bin){
				lyramilk::data::chunk cb = args[0];
				os.write((const char*)cb.c_str(),cb.size());
			}else{
				lyramilk::data::string str = args[0];
				os.write(str.c_str(),str.size());
			}
			return true;
		}

		lyramilk::data::var addCookie(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_map);
			TODO();
		}

		lyramilk::data::var async_url_get(const lyramilk::data::array& args,const lyramilk::data::map& env)
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
			adapter->response->code = 0;
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
			return 2;
		}
	};

	class httppostedfile_datawrapper:public lyramilk::data::datawrapper
	{

	  public:
		httpadapter* adapter;
		mime* me;
	  public:
		httppostedfile_datawrapper(httpadapter* _adapter,mime* _me):adapter(_adapter),me(_me)
		{}

	  	virtual ~httppostedfile_datawrapper()
		{}

		static lyramilk::data::string class_name()
		{
			return "lyramilk.teapoy.httppostedfile";
		}

		virtual lyramilk::data::string name() const
		{
			return class_name();
		}

		virtual lyramilk::data::datawrapper* clone() const
		{
			return new httppostedfile_datawrapper(adapter,me);
		}

		virtual void destory()
		{
			delete this;
		}

		virtual bool type_like(lyramilk::data::var::vt nt) const
		{
			return false;
		}
	};

	static const char ramtable[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
	class httppostedfile:public lyramilk::script::sclass
	{
		lyramilk::log::logss log;
		httpadapter* adapter;
		mime* me;
	  public:
		static lyramilk::script::sclass* ctr(const lyramilk::data::array& args)
		{
			if(args.size() > 0 && args[0].type() == lyramilk::data::var::t_user){
				lyramilk::data::datawrapper* urd = args[0].userdata();
				if(urd && urd->name() == httppostedfile_datawrapper::class_name()){
					httppostedfile_datawrapper* urdp = (httppostedfile_datawrapper*)urd;

					if(urdp->adapter == nullptr) return nullptr;
					if(urdp->me == nullptr) return nullptr;
					return new httppostedfile(urdp->adapter,urdp->me);
				}
			}
			return nullptr;
		}
		static void dtr(lyramilk::script::sclass* p)
		{
			delete (httppostedfile*)p;
		}



		httppostedfile(httpadapter* adapter,mime* me):log(lyramilk::klog,"teapoy.native.HttpPostedFile")
		{
			this->adapter = adapter;
			this->me = me;
		}
		
		~httppostedfile()
		{}

		lyramilk::data::var getHeader(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string key = args[0].str();
			return me->get(key);
		}

		lyramilk::data::var getHeaders(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			return me->get_header_obj();
		}


		lyramilk::data::var save(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string path = args[0].str();
			lyramilk::data::string filepath;
			filepath.reserve(path.size());
			srand(time(nullptr) + (unsigned long long)me);


			std::size_t sep = path.find_last_of('/');
			lyramilk::data::string pathdir = path.substr(0,sep);
			if(!mkdir_p(pathdir)){
				throw lyramilk::exception(D("创建目录%s失败错误：%s",pathdir.c_str(),strerror(errno)));
				return lyramilk::data::var::nil;
			}

			int fd = -1;
			for(int i=0;i<10 && fd == -1;++i){
				filepath.clear();
				for(std::size_t i =0;i<path.size();++i){
					char c = path[i];
					if(c == '?'){
						filepath.push_back(ramtable[rand()%(sizeof(ramtable) - 1)]);
					}else{
						filepath.push_back(c);
					}
				}

				int fd = ::open(filepath.c_str(),O_WRONLY | O_CREAT | O_EXCL,0444);
				if(fd == -1){
					continue;
				}

				::write(fd,me->get_body_ptr(),me->get_body_size());
				
				::close(fd);
				return filepath;
			}
			return lyramilk::data::var::nil;
		}



		lyramilk::data::var getRequestBody(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			return lyramilk::data::chunk(me->get_body_ptr(),me->get_body_size());
		}

		lyramilk::data::var getRequestBodyLength(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			return me->get_body_size();
		}

		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["get"] = lyramilk::script::engine::functional<httppostedfile,&httppostedfile::getHeader>;
			fn["getHeader"] = lyramilk::script::engine::functional<httppostedfile,&httppostedfile::getHeader>;
			fn["getHeaders"] = lyramilk::script::engine::functional<httppostedfile,&httppostedfile::getHeaders>;
			fn["save"] = lyramilk::script::engine::functional<httppostedfile,&httppostedfile::save>;
			fn["getRequestBody"] = lyramilk::script::engine::functional<httppostedfile,&httppostedfile::getRequestBody>;
			fn["getRequestBodyLength"] = lyramilk::script::engine::functional<httppostedfile,&httppostedfile::getRequestBodyLength>;
			p->define("HttpPostedFile",fn,httppostedfile::ctr,httppostedfile::dtr);
			return 1;
		}
	};

	class httprequest:public lyramilk::script::sclass
	{
		lyramilk::log::logss log;
		httpadapter* adapter;
		lyramilk::data::var sessionobj;
	  public:
		static lyramilk::script::sclass* ctr(const lyramilk::data::array& args)
		{
			if(args.size() > 0 && args[0].type() == lyramilk::data::var::t_user){
				lyramilk::data::datawrapper* urd = args[0].userdata();
				if(urd && urd->name() == lyramilk::teapoy::session_response_datawrapper::class_name()){
					lyramilk::teapoy::session_response_datawrapper* urdp = (lyramilk::teapoy::session_response_datawrapper*)urd;

					if(urdp->adapter == nullptr) return nullptr;
					return new httprequest(urdp->adapter);
				}
			}
			return nullptr;
		}
		static void dtr(lyramilk::script::sclass* p)
		{
			delete (httprequest*)p;
		}

		httprequest(httpadapter* adapter):log(lyramilk::klog,"teapoy.native.HttpRequest")
		{
			this->adapter = adapter;
		}

		~httprequest()
		{
		}

		lyramilk::data::var getHeader(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string key = args[0].str();
			return adapter->request->get(key);
		}

		lyramilk::data::var getHeaders(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			return adapter->request->get_header_obj();
		}

		lyramilk::data::var getScheme(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			return adapter->request->scheme();
		}

		lyramilk::data::var getParameter(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string key = args[0].str();
			const lyramilk::data::map& m = adapter->request->params();
			lyramilk::data::map::const_iterator it = m.find(key);
			if(it == m.end()) return lyramilk::data::var::nil;
			if(it->second.type() != lyramilk::data::var::t_array) return lyramilk::data::var::nil;
			const lyramilk::data::array& ar = it->second;
			if(ar.size() == 0) return lyramilk::data::var::nil;
			return ar[0].str();
		}

		lyramilk::data::var getParameterRaw(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			TODO();
		}
		lyramilk::data::var getParameterValues(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			// TODO
			return adapter->request->params();
		}

		lyramilk::data::var getFile(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			TODO();
		}

		lyramilk::data::var getFiles(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			lyramilk::data::array fileobjs;


			std::size_t c = adapter->request->get_childs_count();
			for(std::size_t i=0;i<c;++i){
				mime* m = adapter->request->at(i);
				if(m->get("Content-Type") == "") continue;
				lyramilk::data::array ar;
				ar.push_back(httppostedfile_datawrapper(adapter,m));


				lyramilk::data::map::const_iterator it_env_eng = env.find(lyramilk::script::engine::s_env_engine());
				if(it_env_eng != env.end()){
					lyramilk::data::datawrapper* urd = it_env_eng->second.userdata();
					if(urd && urd->name() == lyramilk::script::engine_datawrapper::class_name()){
						lyramilk::script::engine_datawrapper* urdp = (lyramilk::script::engine_datawrapper*)urd;
						if(urdp->eng){
							lyramilk::data::var fileobj = urdp->eng->createobject("HttpPostedFile",ar);
							fileobjs.push_back(fileobj);
						}
					}
				}
			}
			return fileobjs;
		}

		lyramilk::data::var getURI(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			return adapter->request->url();
		}

		lyramilk::data::var getMethod(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			return adapter->request->get(":method");
		}

		lyramilk::data::var getRequestBody(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			TODO();
		}

		lyramilk::data::var getRealPath(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			TODO();
		}

		lyramilk::data::var getSession(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			if(sessionobj.type() == lyramilk::data::var::t_user){
				return sessionobj;
			}
			std::stringstream oss;
			lyramilk::data::array ar;
			ar.push_back(lyramilk::teapoy::session_response_datawrapper(adapter,oss));

			lyramilk::data::map::const_iterator it_env_eng = env.find(lyramilk::script::engine::s_env_engine());
			if(it_env_eng != env.end()){
				lyramilk::data::datawrapper* urd = it_env_eng->second.userdata();
				if(urd && urd->name() == lyramilk::script::engine_datawrapper::class_name()){
					lyramilk::script::engine_datawrapper* urdp = (lyramilk::script::engine_datawrapper*)urd;
					if(urdp->eng){
						sessionobj = urdp->eng->createobject("HttpSession",ar);
					}
				}
			}
			return sessionobj;
		}

		lyramilk::data::var getPeerCertificateInfo(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			return adapter->channel->ssl_peer_certificate_info;
		}

		lyramilk::data::var getRemoteAddr(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			TODO();
		}

		lyramilk::data::var getLocalAddr(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			TODO();
		}

		lyramilk::data::var wget(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			TODO();
		}

		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["get"] = lyramilk::script::engine::functional<httprequest,&httprequest::getHeader>;
			fn["getHeader"] = lyramilk::script::engine::functional<httprequest,&httprequest::getHeader>;
			fn["getHeaders"] = lyramilk::script::engine::functional<httprequest,&httprequest::getHeaders>;
			fn["getScheme"] = lyramilk::script::engine::functional<httprequest,&httprequest::getScheme>;
			fn["getParameter"] = lyramilk::script::engine::functional<httprequest,&httprequest::getParameter>;
			fn["getParameterRaw"] = lyramilk::script::engine::functional<httprequest,&httprequest::getParameterRaw>;
			fn["getParameterValues"] = lyramilk::script::engine::functional<httprequest,&httprequest::getParameterValues>;
			fn["getFile"] = lyramilk::script::engine::functional<httprequest,&httprequest::getFile>;
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
		i+= httppostedfile::define(p);
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