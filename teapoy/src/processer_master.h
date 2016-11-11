#ifndef _lyramilk_teapoy_jsx_h_
#define _lyramilk_teapoy_jsx_h_

#include "web.h"

namespace lyramilk{ namespace teapoy { namespace web {

	struct processer_args
	{
		lyramilk::teapoy::http::request* req;
		std::ostream& os;
		methodinvoker* invoker;
		lyramilk::data::string real;
		lyramilk::data::var session;
	};


	class processer
	{
		void* regex;
		lyramilk::data::var dest;
		virtual bool invoke(lyramilk::data::string proc_file,methodinvoker* invoker,lyramilk::teapoy::http::request* req,std::ostream& os) = 0;
	  public:
		processer();
		virtual ~processer();
		bool init(lyramilk::data::string pattern,lyramilk::data::string proc_file_pattern);
		virtual bool test(methodinvoker* invoker,lyramilk::teapoy::http::request* req,std::ostream& os,bool* ret);

		typedef processer (*builder)();
	};

	class processer_jsx:public processer
	{
		virtual bool invoke(lyramilk::data::string proc_file,methodinvoker* invoker,lyramilk::teapoy::http::request* req,std::ostream& os);
	  public:
		processer_jsx();
		virtual ~processer_jsx();
		virtual bool test(methodinvoker* invoker,lyramilk::teapoy::http::request* req,std::ostream& os,bool* ret);
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
		virtual bool test(methodinvoker* invoker,lyramilk::teapoy::http::request* req,std::ostream& os,bool* ret);

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
	  public:
		processer_master();
		virtual ~processer_master();

		virtual void mapping(lyramilk::data::string type,lyramilk::data::string pattern,lyramilk::data::string proc_file_pattern);
		virtual bool invoke(methodinvoker* invoker,lyramilk::teapoy::http::request* req,std::ostream& os,bool* ret);
	};
}}}

#endif
