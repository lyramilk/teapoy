#ifndef _lyramilk_functional_impl_mysql_h_
#define _lyramilk_functional_impl_mysql_h_

#include "functional_master.h"

namespace lyramilk{ namespace teapoy {

	class functional_multi_impl_mysql:public functional_multi
	{
	  public:
		virtual functional::ptr get_instance() = 0;
	};
}}

#endif
