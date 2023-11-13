#ifndef _lyramilk_functional_impl_mongodb_h_
#define _lyramilk_functional_impl_mongodb_h_

#include "functional_master.h"

namespace lyramilk{ namespace teapoy {

	class processer_impl_mongodb:public functional_multi
	{
	  public:
		virtual functional* get_instance(lyramilk::data::string identity) = 0;
	};
}}

#endif
