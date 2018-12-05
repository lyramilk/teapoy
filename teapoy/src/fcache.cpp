#include "fcache.h"

#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include <cassert>

namespace lyramilk{ namespace teapoy {
	fcache::fcache()
	{
		memory_mapping = nullptr;
		memory_mapping_length = 0;
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

	int fcache::write(const void* data,int datasize)
	{
		if(datasize == 0) return 0;
		assert(datasize > 0);
		if(memory_mapping){
			munmap(memory_mapping,memory_mapping_length);
			memory_mapping = nullptr;
			memory_mapping_length = 0;
		}

		if(p + datasize > static_cache + sizeof(static_cache)){
			if(fd == -1){
				char filename_template[] = "/tmp/teapoy-XXXXXX";
				fd = mkstemp(filename_template);
				if(fd == -1){
					if(errno == ENOENT){
						memcpy(filename_template,"/teapoy-XXXXXX",sizeof(filename_template));
						fd = mkstemp(filename_template);
						if(fd == -1){
							return 0;
						}
					}
				}

				::write(fd,static_cache,int(p-static_cache));
				unlink(filename_template);
			}
		}

		if(fd != -1){
			int r = ::write(fd,data,datasize);
			if(r < datasize){
				::fdatasync(fd);
				int r2 = ::write(fd,(const char*)data + r,datasize - r);
				if(r2 > 0){
					return r + r2;
				}
				return r;
			}
			return r;
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
