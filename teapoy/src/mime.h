#ifndef _lyramilk_teapoy_mime_h_
#define _lyramilk_teapoy_mime_h_

#include <libmilk/var.h>

namespace lyramilk{ namespace teapoy {

	class mime
	{
	  public:
		mutable lyramilk::data::var::map _header;
	  public:
		mime();
		virtual ~mime();

		virtual lyramilk::data::string get(const lyramilk::data::string& k) const;
		virtual void set(const lyramilk::data::string& k,const lyramilk::data::string& v);

		virtual lyramilk::data::var::map& header();

		virtual bool parse(const char* buf,int size);

		virtual const char* ptr() const;
		virtual lyramilk::data::uint64 size() const;
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
