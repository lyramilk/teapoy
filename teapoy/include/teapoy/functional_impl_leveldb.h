#ifndef _lyramilk_functional_impl_leveldb_h_
#define _lyramilk_functional_impl_leveldb_h_

#include "functional_master.h"

namespace lyramilk{ namespace teapoy {

	class processer_impl_leveldb:public functional_multi
	{
	  public:
		virtual functional::ptr get_instance() = 0;
	};
}}

#endif
