#ifndef _lyramilk_teapoy_session_h_
#define _lyramilk_teapoy_session_h_

#include "config.h"
#include <libmilk/factory.h>

namespace lyramilk{ namespace teapoy { namespace web {
	class sessions
	{
	  public:
		virtual lyramilk::data::string newid() = 0;
		virtual lyramilk::data::var& get(const lyramilk::data::string& sid,const lyramilk::data::string& key) = 0;
		virtual void set(const lyramilk::data::string& sid,const lyramilk::data::string& key,const lyramilk::data::var& value) = 0;

		static sessions* defaultinstance();
	};

	class sessions_factory:public lyramilk::util::multiton_factory<sessions>
	{
	  public:
		static sessions_factory* instance();
	};
}}}

#endif
