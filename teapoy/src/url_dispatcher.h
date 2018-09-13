#ifndef _lyramilk_teapoy_urldispatcher_h_
#define _lyramilk_teapoy_urldispatcher_h_

#include "config.h"
#include <list>
#include <libmilk/var.h>
#include <libmilk/gc.h>
#include <libmilk/factory.h>
#include <pcre.h>
#include "webservice.h"

namespace lyramilk{ namespace teapoy {
	class httprequest;
	class httpresponse;
	class httpadapter;

	class url_selector:public lyramilk::obj
	{
	  public:
		url_selector();
		virtual ~url_selector();

		virtual bool hittest(httprequest* request,httpresponse* response,httpadapter* adapter) = 0;
	};

	class url_regex_selector:public url_selector
	{
	  protected:
		lyramilk::data::strings default_pages;

		pcre* regex_handler;
	  	lyramilk::data::string regex_str;

		lyramilk::data::string path_pattern;
		lyramilk::data::var::array url_to_path_rule;
	  protected:
		virtual bool hittest(httprequest* request,httpresponse* response,httpadapter* adapter);
	  public:
		url_regex_selector();
		virtual ~url_regex_selector();

	  public:
		virtual bool init(const lyramilk::data::strings& default_pages,const lyramilk::data::string& regex_str,const lyramilk::data::string& path_pattern);
		virtual bool test(httprequest* request,lyramilk::data::string *real);
		virtual bool call(httprequest* request,httpresponse* response,httpadapter* adapter,const lyramilk::data::string& real) = 0;
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

		virtual bool call(httprequest* request,httpresponse* response,httpadapter* adapter);

		virtual bool add(lyramilk::ptr<url_selector> selector);
	};

}}

#endif
