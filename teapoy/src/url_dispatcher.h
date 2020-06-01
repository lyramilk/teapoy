#ifndef _lyramilk_teapoy_urldispatcher_h_
#define _lyramilk_teapoy_urldispatcher_h_

#include "config.h"
#include <list>
#include <libmilk/var.h>
#include <libmilk/gc.h>
#include <libmilk/factory.h>
#include <libmilk/testing.h>
#include <pcre.h>
#include "webservice.h"
#include "cvmap_dispatcher.h"

namespace lyramilk{ namespace teapoy {
	class httprequest;
	class httpresponse;
	class httpadapter;


	class url_selector_loger
	{
		lyramilk::debug::nsecdiff td;
		httpadapter* adapter;
		lyramilk::data::string prefix;

		bool iscancel;
	  public:
		url_selector_loger(lyramilk::data::string prefix,httpadapter* adapter);
		~url_selector_loger();
		void cancel();
	};

	class url_selector:public lyramilk::obj
	{
	  public:
		url_selector();
		virtual ~url_selector();

		virtual dispatcher_check_status hittest(httprequest* request,httpresponse* response,httpadapter* adapter) = 0;
	};


	struct url_regex_part
	{
		enum url_regex_part_type{
			t_static = 1,
			t_index,
			t_group,
		}type;
		int index;
		lyramilk::data::string str;
	};



	class url_regex_selector:public url_selector
	{
	  protected:
		lyramilk::data::strings default_pages;

	  	std::vector<pcre*> regex_assert;
		pcre* regex_handler;
		pcre_extra *regex_handler_study;
	  	lyramilk::data::string regex_str;

		lyramilk::data::string path_pattern;
		typedef std::vector<url_regex_part> url_parts_type;
		url_parts_type url_to_path_rule;
	  protected:
		lyramilk::data::string authtype;
		lyramilk::data::string authscript;
		lyramilk::data::var extra;
	  protected:
		virtual dispatcher_check_status hittest(httprequest* request,httpresponse* response,httpadapter* adapter);
	  public:
		url_regex_selector();
		virtual ~url_regex_selector();


	  public:
		virtual bool init_selector(const lyramilk::data::strings& default_pages,const lyramilk::data::string& regex_str,const lyramilk::data::string& path_pattern);
		virtual bool init_auth(const lyramilk::data::string& enginetype,const lyramilk::data::string& authscript);
		virtual bool init(lyramilk::data::map& extra);

		virtual dispatcher_check_status test(httprequest* request,lyramilk::data::string *real);

		virtual dispatcher_check_status check_auth(httprequest* request,httpresponse* response,httpadapter* adapter,const lyramilk::data::string& real);
		virtual dispatcher_check_status call(httprequest* request,httpresponse* response,httpadapter* adapter,const lyramilk::data::string& real) = 0;
	};


	class url_selector_factory:public lyramilk::util::factory<url_selector>
	{
	  public:
		static url_selector_factory* instance();
	};

	class url_dispatcher
	{
		std::list<lyramilk::ptr<url_selector> > selectors;
	  public:
		url_dispatcher();
		virtual ~url_dispatcher();

		virtual dispatcher_check_status call(httprequest* request,httpresponse* response,httpadapter* adapter);

		virtual bool add(lyramilk::ptr<url_selector> selector);
	};

}}

#endif
