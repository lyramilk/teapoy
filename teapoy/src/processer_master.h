#ifndef _lyramilk_teapoy_jsx_h_
#define _lyramilk_teapoy_jsx_h_

#include "web.h"
#include "stringutil.h"
#include <libmilk/scriptengine.h>

namespace lyramilk{ namespace teapoy { namespace web {

	class processer_args
	{
	  public:
		lyramilk::teapoy::http::request* req;
		std::ostream& os;
		methodinvoker* invoker;
		lyramilk::data::string real;

		processer_args(lyramilk::data::string proc_file,methodinvoker* invoker,lyramilk::teapoy::http::request* req,std::ostream& os);
		~processer_args();
		const lyramilk::data::string& getsid();
		lyramilk::data::var& get(const lyramilk::data::string& key);
		void set(const lyramilk::data::string& key,const lyramilk::data::var& value);
	  protected:
		lyramilk::data::string sid;
	};


	class processer
	{
		void* regex;
		lyramilk::data::var dest;
		lyramilk::data::string regexstr;
		virtual bool invoke(lyramilk::data::string proc_file,methodinvoker* invoker,lyramilk::teapoy::http::request* req,std::ostream& os) = 0;
		lyramilk::script::engine* pauthobj;
	  public:
		processer();
		virtual ~processer();
		bool init(lyramilk::data::string pattern,lyramilk::data::string proc_file_pattern);
		bool set_auth(lyramilk::data::string authfiletype,lyramilk::data::string authfile);
		virtual bool preload(const lyramilk::teapoy::strings& loads);
		virtual bool test(methodinvoker* invoker,lyramilk::teapoy::http::request* req,std::ostream& os,bool* ret);
		virtual bool auth_check(methodinvoker* invoker,lyramilk::teapoy::http::request* req,std::ostream& os,bool* ret);

		typedef processer (*builder)();
	};

	class processer_jsx:public processer
	{
		virtual bool invoke(lyramilk::data::string proc_file,methodinvoker* invoker,lyramilk::teapoy::http::request* req,std::ostream& os);
	  public:
		processer_jsx();
		virtual ~processer_jsx();
		static bool exec_jsx(lyramilk::data::string proc_file,methodinvoker* invoker,lyramilk::teapoy::http::request* req,std::ostream& os);

		static processer* ctr(void* args);
		static void dtr(processer* ptr);
	};

	class processer_js:public processer
	{
		virtual bool invoke(lyramilk::data::string proc_file,methodinvoker* invoker,lyramilk::teapoy::http::request* req,std::ostream& os);
	  public:
		processer_js();
		virtual ~processer_js();
		virtual bool preload(const lyramilk::teapoy::strings& loads);

		static processer* ctr(void* args);
		static void dtr(processer* ptr);
	};

	class processer_php:public processer
	{
		virtual bool invoke(lyramilk::data::string proc_file,methodinvoker* invoker,lyramilk::teapoy::http::request* req,std::ostream& os);
	  public:
		processer_php();
		virtual ~processer_php();

		static processer* ctr(void* args);
		static void dtr(processer* ptr);
	};

	class processer_lua:public processer
	{
		virtual bool invoke(lyramilk::data::string proc_file,methodinvoker* invoker,lyramilk::teapoy::http::request* req,std::ostream& os);
	  public:
		processer_lua();
		virtual ~processer_lua();

		static processer* ctr(void* args);
		static void dtr(processer* ptr);
	};

	class processer_master:public processer_selector,public lyramilk::util::factory<processer>
	{
		struct processer_pair
		{
			lyramilk::data::string type;
			processer* ptr;
		};
		std::vector<processer_pair> es;
		std::map<lyramilk::data::string,lyramilk::teapoy::strings> preloads;
	  public:
		processer_master();
		virtual ~processer_master();

		virtual void mapping(lyramilk::data::string type,lyramilk::data::string pattern,lyramilk::data::string proc_file_pattern);
		virtual void mapping(lyramilk::data::string type,lyramilk::data::string pattern,lyramilk::data::string proc_file_pattern,lyramilk::data::string authfiletype,lyramilk::data::string authfile);
		virtual bool invoke(methodinvoker* invoker,lyramilk::teapoy::http::request* req,std::ostream& os,bool* ret);
		virtual bool preload(lyramilk::data::string type,const lyramilk::data::var::array& loads);
	};
}}}

#endif
