#include "mime.h"
#include "stringutil.h"
#include <strings.h>
#include <libmilk/exception.h>
#include <libmilk/dict.h>
#include <libmilk/log.h>
#include <stdio.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <algorithm>

#define LENGTH_OF_MEMCACHE (10*1024*1024)

namespace lyramilk{ namespace teapoy {
	const static lyramilk::data::string mime_boundary = "boundary=";

	mime::mime()
	{
	}

	mime::~mime()
	{
	}

	void mime::init() const
	{
		if(mimeheaderstr.empty() || !_header.empty()) return;

		lyramilk::data::strings vheader = lyramilk::teapoy::split(mimeheaderstr,"\r\n");
		lyramilk::data::strings::iterator it = vheader.begin();
		lyramilk::data::var* laststr = nullptr;
		for(;it!=vheader.end();++it){
			std::size_t pos = it->find_first_of(":");
			if(pos != it->npos){
				lyramilk::data::string key = lyramilk::teapoy::lowercase(lyramilk::teapoy::trim(it->substr(0,pos)," \t\r\n"));

				lyramilk::data::string value = lyramilk::teapoy::trim(it->substr(pos + 1)," \t\r\n");
				if(!key.empty() && !value.empty()){
					laststr = &(_header[key] = value);
				}
			}else{
				if(laststr){
					(*laststr) += *it;
				}
			}
		}
		//mimeheaderstr.clear();
	}

	bool mime::parse(const char* buf,int size)
	{
		mimeheaderstr.assign(buf,size);
		return true;
	}

	const char* mime::ptr() const
	{
		return mimeheaderstr.c_str();
	}

	lyramilk::data::uint64 mime::size() const
	{
		return mimeheaderstr.size();
	}

	lyramilk::data::string mime::get(const lyramilk::data::string& k) const
	{
		init();
		lyramilk::data::string key = lyramilk::teapoy::lowercase(k);
		lyramilk::data::map::const_iterator it = _header.find(key);
		if(it!=_header.end()){
			return it->second;
		}
		return "";
	}

	void mime::set(const lyramilk::data::string& k,const lyramilk::data::string& v)
	{
		init();
		lyramilk::data::string key = lyramilk::teapoy::lowercase(k);
		_header[key] = v;
	}

	lyramilk::data::map& mime::header()
	{
		init();
		return _header;
	}

}}
