#ifndef _lyramilk_functional_impl_logfile_h_
#define _lyramilk_functional_impl_logfile_h_

#include "functional_master.h"

namespace lyramilk{ namespace teapoy {

	class processer_impl_logfile:public functional_multi
	{
	  public:
		virtual functional::ptr get_instance() = 0;
	};
}}

#endif
