#ifndef _lyramilk_functional_impl_cavedb_h_
#define _lyramilk_functional_impl_cavedb_h_

#include "functional_master.h"

namespace lyramilk{ namespace teapoy {

	class processer_impl_cavedb:public functional_multi
	{
	  public:
		virtual functional::ptr get_instance() = 0;
	};
}}

#endif
