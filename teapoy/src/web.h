#ifndef _lyramilk_teapoy_web_h_
#define _lyramilk_teapoy_web_h_

#include "config.h"
#include "http.h"
#include "session.h"
#include <libmilk/netaio.h>
#include <libmilk/factory.h>
#include <libmilk/testing.h>
#include <map>

namespace lyramilk{ namespace teapoy { namespace web {

	class website_worker;

	class url_worker
	{
		lyramilk::data::string method;

		void* matcher_regex;
		lyramilk::data::var matcher_dest;
		lyramilk::data::string matcher_regexstr;
		lyramilk::data::string authtype;
		lyramilk::data::string authscript;
		lyramilk::data::strings index;
	  protected:
		lyramilk::data::var extra;
	  protected:
		virtual bool call(lyramilk::teapoy::http::request* req,std::ostream& os,lyramilk::data::string real,website_worker& w) const = 0;
		virtual bool test(lyramilk::teapoy::http::request* req,std::ostream& os,lyramilk::data::string& real,website_worker& w) const;
	  public:
		url_worker();
		virtual ~url_worker();
		lyramilk::data::string get_method();
		virtual bool init(lyramilk::data::string method,lyramilk::data::string pattern,lyramilk::data::string real,lyramilk::data::var::array index);
		virtual bool init_auth(lyramilk::data::string enginetype,lyramilk::data::string authscript);
		virtual bool init_extra(const lyramilk::data::var& extra);
		virtual bool check_auth(lyramilk::teapoy::http::request* req,std::ostream& os,lyramilk::data::string real,website_worker& w,bool* ret) const;
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
	  public:
		std::vector<url_worker*> lst;
	  public:
		website_worker();
		virtual ~website_worker();

		virtual bool try_call(lyramilk::teapoy::http::request* req,std::ostream& os,website_worker& w,bool* ret) const;
	};

	class session_info
	{
		lyramilk::data::string sid;
	  public:
		lyramilk::teapoy::http::request* req;
		std::ostream& os;
		website_worker& worker;
		lyramilk::data::string real;

		session_info(lyramilk::data::string realfile,lyramilk::teapoy::http::request* req,std::ostream& os,website_worker& w);
		~session_info();

		lyramilk::data::string getsid();


		lyramilk::data::var& get(const lyramilk::data::string& key);
		void set(const lyramilk::data::string& key,const lyramilk::data::var& value);
	};

	class aiohttpsession:public lyramilk::netio::aiosession2
	{
	  public:
		lyramilk::teapoy::http::request req;
		website_worker* worker;

		aiohttpsession();
		virtual ~aiohttpsession();

		virtual bool oninit(std::ostream& os);
		virtual bool onrequest(const char* cache,int size,std::ostream& os);
	};
}}}

#endif
