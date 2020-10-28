#include "sni_selector.h"

namespace lyramilk{ namespace teapoy
{

	bool epoll_selector::onadd(lyramilk::io::aiopoll_safe* poll)
	{
		return poll->add(selector);
	};

}}
