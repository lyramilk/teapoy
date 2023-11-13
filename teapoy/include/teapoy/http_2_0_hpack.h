#ifndef _lyramilk_teapoy_http_2_0_hpack_h_
#define _lyramilk_teapoy_http_2_0_hpack_h_

#include "config.h"
#include <libmilk/var.h>

namespace lyramilk{ namespace teapoy {

	class hpack
	{
		lyramilk::data::chunk cache;
		std::vector<std::pair<lyramilk::data::string,lyramilk::data::string> > dynamic_table;
		const unsigned char* cursor;
	  public:
		unsigned int hard_header_table_size;
	  	unsigned int dynamic_table_size;

		hpack();
		virtual ~hpack();

		virtual void unpack(const unsigned char* p,lyramilk::data::uint64 l);

		virtual bool next(lyramilk::data::string* k,lyramilk::data::string* v);

		static bool pack(std::ostream& os,const lyramilk::data::stringdict& m);
		static bool pack_kv(std::ostream& os,const lyramilk::data::string& k,const lyramilk::data::string& v);
		static bool pack_str(std::ostream& os,const lyramilk::data::string& s);

	  protected:
		virtual bool read_table(lyramilk::data::uint64 idx,lyramilk::data::string* k,lyramilk::data::string* v);
		virtual lyramilk::data::string read_string();
		virtual lyramilk::data::uint64 read_int(unsigned int n);
	};

}}

#endif
