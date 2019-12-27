#ifndef _lyramilk_teapoy_web_h_
#define _lyramilk_teapoy_web_h_

#include "config.h"
#include "http.h"
#include "session.h"
#include "webhook.h"
#include <libmilk/netaio.h>
#include <libmilk/factory.h>
#include <libmilk/testing.h>
#include <libmilk/datawrapper.h>
#include <map>

namespace lyramilk{ namespace teapoy { namespace web {

	class website_worker;
	class session_info;

	class url_worker
	{
		lyramilk::data::string method;

		std::vector<void*> premise_regex;
		void* matcher_regex;
		lyramilk::data::var matcher_dest;
		lyramilk::data::string matcher_regexstr;
		lyramilk::data::string authtype;
		lyramilk::data::string authscript;
		lyramilk::data::strings index;
	  protected:
		lyramilk::data::var extra;
		webhook* hook;
	  protected:
		virtual bool call(session_info* si) const = 0;
		virtual bool test(lyramilk::data::string uri,lyramilk::data::string* real) const;
	  public:
		url_worker();
		virtual ~url_worker();
		lyramilk::data::string get_method();
		virtual bool init(const lyramilk::data::string& method,const lyramilk::data::strings& pattern_premise,const lyramilk::data::string& pattern,const lyramilk::data::string& real,const lyramilk::data::strings& index,webhook* h);
		virtual bool init_auth(const lyramilk::data::string& enginetype,const lyramilk::data::string& authscript);
		virtual bool init_extra(const lyramilk::data::var& extra);
		virtual bool check_auth(session_info* si,bool* ret) const;
		virtual bool try_call(lyramilk::teapoy::http::request* req,std::ostream& os,website_worker& w,bool* ret) const;
	};

	class url_worker_loger
	{
		lyramilk::debug::nsecdiff td;
		lyramilk::teapoy::http::request* req;
		lyramilk::data::string prefix;
	  public:
		url_worker_loger(lyramilk::data::string prefix,lyramilk::teapoy::http::request* req);
		~url_worker_loger();
	};

	class url_worker_master:public lyramilk::util::factory<url_worker>
	{
	  public:
		static url_worker_master* instance();
	};

	class website_worker
	{
		lyramilk::data::string urlhooktype;
	  public:
		std::vector<url_worker*> lst;
	  public:
		website_worker();
		virtual ~website_worker();

		virtual void set_urlhook(lyramilk::data::string urlhooktype);
		virtual bool try_call(lyramilk::teapoy::http::request* req,std::ostream& os,website_worker& w,bool* ret) const;
	};

	class session_info
	{
		lyramilk::data::string sid;
	  public:
		lyramilk::teapoy::http::request* req;
		lyramilk::teapoy::http::response* rep;
		website_worker& worker;
		lyramilk::data::string real;
		webhook_helper* hook;
		lyramilk::data::uint32 response_code;

		session_info(lyramilk::data::string realfile,lyramilk::teapoy::http::request* req,std::ostream& os,website_worker& w,webhook_helper* h);
		~session_info();

		lyramilk::data::string getsid();


		lyramilk::data::var& get(const lyramilk::data::string& key);
		void set(const lyramilk::data::string& key,const lyramilk::data::var& value);
	};

	class aiohttpsession;
	class aiohttpsession_http:public lyramilk::netio::aiosession_sync
	{
	  protected:
		aiohttpsession* root;
		friend class aiohttpsession;
	  public:
		aiohttpsession_http(aiohttpsession* root);
		virtual ~aiohttpsession_http();
		virtual lyramilk::netio::native_socket_type fd() const;
	};

	class aiohttpsession_http_1_0:public aiohttpsession_http
	{
		lyramilk::teapoy::http::request_http_1_0 req;
	  public:
		aiohttpsession_http_1_0(aiohttpsession* root);
		virtual ~aiohttpsession_http_1_0();
	  protected:
		virtual bool oninit(std::ostream& os);
		virtual bool onrequest(const char* cache,int size,std::ostream& os);
	};

	class aiohttpsession_http_1_1:public aiohttpsession_http
	{
		lyramilk::teapoy::http::request_http_1_1 req;
	  public:
		aiohttpsession_http_1_1(aiohttpsession* root);
		virtual ~aiohttpsession_http_1_1();
	  protected:
		virtual bool oninit(std::ostream& os);
		virtual bool onrequest(const char* cache,int size,std::ostream& os);
	};

	class aiohttpsession_http_2_0:public aiohttpsession_http
	{
		lyramilk::teapoy::http::request_http_2_0 req;
	  public:
		aiohttpsession_http_2_0(aiohttpsession* root);
		virtual ~aiohttpsession_http_2_0();
	  protected:
		virtual bool oninit(std::ostream& os);
		virtual bool onrequest(const char* cache,int size,std::ostream& os);
	};

	class aiohttpsession:public lyramilk::netio::aiosession_sync
	{
	  public:
		aiohttpsession_http* handler;
		website_worker* worker;
		lyramilk::teapoy::web::sessions* sessionmgr;
	  public:
		aiohttpsession();
		virtual ~aiohttpsession();

		virtual bool oninit(std::ostream& os);
		virtual bool onrequest(const char* cache,int size,std::ostream& os);
	};



	class session_info_datawrapper:public lyramilk::data::datawrapper
	{

	  public:
		session_info* si;
	  public:
		session_info_datawrapper(session_info* _si);
	  	virtual ~session_info_datawrapper();

		static lyramilk::data::string class_name();
		virtual lyramilk::data::string name() const;
		virtual lyramilk::data::datawrapper* clone() const;
		virtual void destory();
		virtual bool type_like(lyramilk::data::var::vt nt) const;
	};


}}}

#endif
