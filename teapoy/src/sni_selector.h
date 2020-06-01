#ifndef _lyramilk_teapoy_sni_epollselector_h_
#define _lyramilk_teapoy_sni_epollselector_h_

#include <libmilk/var.h>
#include <libmilk/aio.h>
#include "script.h"

namespace lyramilk{ namespace teapoy
{

	class epoll_selector:public lyramilk::script::sclass
	{
	  protected:
		lyramilk::io::aioselector * selector;
	  public:
	  	virtual bool onadd(lyramilk::io::aiopoll_safe* poll);
	
	};

}}

#endif
