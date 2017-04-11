#ifndef _lyramilk_teapoy_stringutil_h_
#define _lyramilk_teapoy_stringutil_h_

#include <libmilk/var.h>

namespace lyramilk{ namespace teapoy{
	lyramilk::data::strings split(lyramilk::data::string data,lyramilk::data::string sep);
	lyramilk::data::strings pathof(lyramilk::data::string path);
	lyramilk::data::string trim(lyramilk::data::string data,lyramilk::data::string pattern);
	lyramilk::data::string lowercase(lyramilk::data::string src);
}}


#endif
