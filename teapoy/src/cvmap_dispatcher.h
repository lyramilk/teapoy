#ifndef _lyramilk_teapoy_cvmapdispatcher_h_
#define _lyramilk_teapoy_cvmapdispatcher_h_

#include "config.h"
#include "script.h"
#include <libmilk/factory.h>
#include <libmilk/netio.h>
#include <libmilk/gc.h>


namespace lyramilk{ namespace teapoy {
	enum dispatcher_check_status{
		cs_error,
		cs_ok,
		cs_pass,
	};

	class cvmap_selector:public lyramilk::obj
	{
	  public:
		cvmap_selector();
		virtual ~cvmap_selector();

		virtual dispatcher_check_status hittest(const lyramilk::data::string& c,lyramilk::data::int64 v,const lyramilk::data::map& request,lyramilk::data::map* response) = 0;
	};

	class cvmap_script_selector:public cvmap_selector
	{
		struct cvpatterncell
		{
			enum {
				cvc_str,
				cvc_c,
				cvc_v,
			}t;
			lyramilk::data::string s;
		};
		std::vector<cvpatterncell> cells;
		lyramilk::data::string enginetype;
		lyramilk::script::engines* es;
	  public:
		cvmap_script_selector();
		virtual ~cvmap_script_selector();

		virtual bool init(const lyramilk::data::string& enginetype,const lyramilk::data::string& path_pattern);


		virtual dispatcher_check_status test(const lyramilk::data::string& c,lyramilk::data::int64 v,lyramilk::data::string *real) const;

		virtual dispatcher_check_status hittest(const lyramilk::data::string& c,lyramilk::data::int64 v,const lyramilk::data::map& request,lyramilk::data::map* response);
		virtual dispatcher_check_status call(const lyramilk::data::string& real,const lyramilk::data::map& request,lyramilk::data::map* response);
	};

	class cvmap_dispatcher
	{
		std::list<lyramilk::ptr<cvmap_selector> > selectors;
	  public:
		cvmap_dispatcher();
		virtual ~cvmap_dispatcher();

		virtual dispatcher_check_status call(const lyramilk::netio::netaddress& source,const lyramilk::data::string& c,lyramilk::data::int64 v,const lyramilk::data::map& request,lyramilk::data::map* response);
		virtual bool add(lyramilk::ptr<cvmap_selector> selector);
	};

}}

#endif