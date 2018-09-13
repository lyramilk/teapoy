#ifndef _lyramilk_teapoy_fcache_h_
#define _lyramilk_teapoy_fcache_h_

#include "config.h"
#include <libmilk/netaio.h>

namespace lyramilk{ namespace teapoy {

	class fcache
	{
	  protected:
		char *p;
		char static_cache[4096];
		bool usefile;
		int fd;
	  	void *memory_mapping;
		std::size_t memory_mapping_length;
	  public:
		fcache();
	  	virtual ~fcache();
		virtual std::size_t write(const void* data,std::size_t datasize);

		virtual bool invalid_mapping();
		virtual const char* data();
		virtual std::size_t length();
	};
}}

#endif
