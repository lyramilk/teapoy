#ifndef _lyramilk_teapoy_fcache_h_
#define _lyramilk_teapoy_fcache_h_

#include "config.h"
#include <libmilk/netaio.h>

namespace lyramilk{ namespace teapoy {

	class fcache
	{
	  protected:
		char *p;
		char static_cache[65536];
		int fd;
	  	void *memory_mapping;
		std::size_t memory_mapping_length;

	  public:
		fcache();
	  	virtual ~fcache();
		virtual int write(const void* data,int datasize);

		virtual bool invalid_mapping();
		virtual const char* data();
		virtual std::size_t length();
	};
}}

#endif
