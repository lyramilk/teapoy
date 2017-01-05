#ifndef _lyramilk_teapoy_repeatersession_h_
#define _lyramilk_teapoy_repeatersession_h_

#include <libmilk/var.h>
#include <libmilk/netaio.h>

namespace lyramilk{ namespace teapoy {
	class repeatersession:public lyramilk::netio::aiosession2
	{
	  public:
		repeatersession();
		virtual ~repeatersession();
		virtual bool oninit(std::ostream& os);
		virtual bool onrequest(const char* cache,int size,std::ostream& os);
	};

}}

#endif
