#ifndef _lyramilk_teapoy_stringutil_h_
#define _lyramilk_teapoy_stringutil_h_

#include <libmilk/var.h>

namespace lyramilk{ namespace teapoy{
	lyramilk::data::strings split(const lyramilk::data::string& data,const lyramilk::data::string& sep);
	lyramilk::data::strings pathof(const lyramilk::data::string& path);
	lyramilk::data::string trim(const lyramilk::data::string& data,const lyramilk::data::string& pattern);
	lyramilk::data::string lowercase(const lyramilk::data::string& src);
}}


#endif
