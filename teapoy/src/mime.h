#ifndef _lyramilk_teapoy_mime_h_
#define _lyramilk_teapoy_mime_h_

#include <libmilk/var.h>

namespace lyramilk{ namespace teapoy {

	class mime
	{
	  public:
		typedef lyramilk::data::unordered_map<lyramilk::data::string,lyramilk::data::string> header_type;
		mutable header_type _header;
	  public:
		mime();
		virtual ~mime();

		virtual lyramilk::data::string get(const lyramilk::data::string& k) const;
		virtual void set(const lyramilk::data::string& k,const lyramilk::data::string& v);

		virtual header_type& header();

		virtual bool parse(const char* buf,int size);
	  private:
		mutable lyramilk::data::string mimeheaderstr;
		void init() const;
	  public:
		static lyramilk::data::string getmimetype_byname(lyramilk::data::string filename);
		static lyramilk::data::string getmimetype_bydata(lyramilk::data::string filedata);
		static lyramilk::data::string getmimetype_byfile(lyramilk::data::string filepathname);
		static void define_mimetype_fileextname(lyramilk::data::string extname,lyramilk::data::string mimetype);
	};

}}

#endif
