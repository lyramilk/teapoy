#ifndef _lyramilk_teapoy_stringutil_h_
#define _lyramilk_teapoy_stringutil_h_

#include <libmilk/var.h>

namespace lyramilk{ namespace teapoy{
	//typedef lyramilk::data::vector<lyramilk::data::string,lyramilk::data::allocator<lyramilk::data::string> > strings;
	using lyramilk::data::strings;

	strings split(lyramilk::data::string data,lyramilk::data::string sep ="\r\n");
	strings pathof(lyramilk::data::string path);
	lyramilk::data::string trim(lyramilk::data::string data,lyramilk::data::string pattern =" \t\r\n");
	lyramilk::data::string lowercase(lyramilk::data::string src);
}}


#endif
