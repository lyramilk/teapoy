#include "fcache.h"

#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>

namespace lyramilk{ namespace teapoy {
	fcache::fcache()
	{
		memory_mapping = nullptr;
		memory_mapping_length = 0;
		usefile = false;
		p = static_cache;

		fd = -1;
	}

	fcache::~fcache()
	{
		if(fd>=0){
			::close(fd);
		}
		if(memory_mapping){
			munmap(memory_mapping,memory_mapping_length);
		}
	}

	std::size_t fcache::write(const void* data,std::size_t datasize)
	{
		if(memory_mapping){
			munmap(memory_mapping,memory_mapping_length);
			memory_mapping = nullptr;
			memory_mapping_length = 0;
		}
		if(fd == -1 && (p + datasize > static_cache + sizeof(static_cache)) ){
			char filename_template[] = "/tmp/teapoy.XXXXXX";
			fd = mkstemp(filename_template);
			unlink(filename_template);

			::write(fd,p,std::size_t(p-static_cache));
		}
		if(fd > -1){
			return ::write(fd,data,datasize);
		}

		memcpy(p,data,datasize);
		p += datasize;
		return datasize;
	}

	bool fcache::invalid_mapping()
	{
		if(memory_mapping == nullptr) return true;
		if( 0 != munmap(memory_mapping,memory_mapping_length)){
			return false;
		}
		memory_mapping = nullptr;
		memory_mapping_length = 0;
		return true;
	}

	const char* fcache::data()
	{
		if(fd < 0){
			return static_cache;
		}
		if(memory_mapping != nullptr) return (const char*)memory_mapping;

		memory_mapping_length = lseek(fd,0,SEEK_END);
		memory_mapping = mmap(nullptr,memory_mapping_length,PROT_READ,MAP_PRIVATE,fd,0);
		return (const char*)memory_mapping;
	}


	std::size_t fcache::length()
	{
		if(fd > 0){
			memory_mapping_length = lseek(fd,0,SEEK_END);
			return memory_mapping_length;
		}
		return p - static_cache;
	}

}}
