#ifndef _lyramilk_teapoy_env_h_
#define _lyramilk_teapoy_env_h_

#include <libmilk/var.h>

namespace lyramilk{ namespace teapoy
{
	class env
	{
	  public:
		static const lyramilk::data::var& get_config(lyramilk::data::string cfgname);
		static void set_config(lyramilk::data::string cfgname,const lyramilk::data::var& cfgvalue);

		static bool is_debug();
		static void set_debug(bool b);
	};
}}

#endif
