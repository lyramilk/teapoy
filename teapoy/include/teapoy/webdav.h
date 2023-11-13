#ifndef _lyramilk_teapoy_webdav_h_
#define _lyramilk_teapoy_webdav_h_
#include <libmilk/def.h>
#include <libmilk/gc.h>
#include <sys/stat.h>

namespace lyramilk{ namespace webdav{

/*
		virtual void isdir(bool g) = 0;
		virtual void href(const lyramilk::data::string& g) = 0;
		virtual void author(const lyramilk::data::string& g) = 0;
		virtual void length(lyramilk::data::uint64 g) = 0;
		virtual void displayname(const lyramilk::data::string& g) = 0;
*/

	class file:public lyramilk::obj
	{
	  public:
		file();
	  	virtual ~file();

		virtual bool isdir() = 0;
		virtual int code() = 0;
		virtual lyramilk::data::string href() = 0;
		virtual lyramilk::data::string owner() = 0;
		virtual lyramilk::data::uint64 length() = 0;
		virtual int lastmodified(lyramilk::data::uint64 g) = 0;
		virtual lyramilk::data::uint64 lastmodified() = 0;
		virtual lyramilk::data::string displayname() = 0;
		virtual lyramilk::data::string etag();

		virtual bool lockstatus() = 0;
		virtual bool trylock() = 0;
		virtual bool unlock() = 0;
		virtual int prop_patch(const lyramilk::data::string& xmlstr);
	};



	class filelist
	{
		std::vector<lyramilk::ptr<lyramilk::webdav::file> > files;
	  public:
		void append(lyramilk::ptr<lyramilk::webdav::file>& f);
		void insert(std::size_t idx,lyramilk::ptr<lyramilk::webdav::file>& f);

		std::size_t count();
		lyramilk::ptr<lyramilk::webdav::file> at(std::size_t idx);

		void remove(std::size_t idx);
		void clear();

		lyramilk::data::string to_xml();
	};


	class fileemulator:public file
	{
		lyramilk::data::string url;
		lyramilk::data::string real;
		lyramilk::data::string filename;
		struct stat st;
		int retcode;
	  public:
		fileemulator(const lyramilk::data::string& url,const lyramilk::data::string& real,const lyramilk::data::string& filename);
	  	virtual ~fileemulator();

		virtual bool isdir();
		virtual int code();
		virtual lyramilk::data::string href();
		virtual lyramilk::data::string owner();
		virtual lyramilk::data::uint64 length();
		virtual int lastmodified(lyramilk::data::uint64 g);
		virtual lyramilk::data::uint64 lastmodified();
		virtual lyramilk::data::string displayname();

		virtual bool lockstatus();
		virtual bool trylock();
		virtual bool unlock();
	};


}}

#endif