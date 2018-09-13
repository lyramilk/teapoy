#ifndef _lyramilk_teapoy_mimetype_h_
#define _lyramilk_teapoy_mimetype_h_

#include "config.h"
#include <libmilk/var.h>

namespace lyramilk{ namespace teapoy {
	class mime
	{
	  public:
		static lyramilk::data::string getmimetype_byname(lyramilk::data::string filename);
		static lyramilk::data::string getmimetype_bydata(const unsigned char* data,std::size_t size);
		static lyramilk::data::string getmimetype_byfile(lyramilk::data::string filepathname);

		static void define_mimetype_fileextname(lyramilk::data::string extname,lyramilk::data::string mimetype);
	};

}}

#endif
