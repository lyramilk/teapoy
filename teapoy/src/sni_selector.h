#ifndef _lyramilk_teapoy_sni_epollselector_h_
#define _lyramilk_teapoy_sni_epollselector_h_

#include <libmilk/var.h>

namespace lyramilk{ namespace teapoy
{

	class epoll_selector:public lyramilk::script::sclass
	{
	  public:
		lyramilk::io::aioselector * selector;
	
	};

}}

#endif
