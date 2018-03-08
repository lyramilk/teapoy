#ifndef _teapoyd_js_extend_h_
#define _teapoyd_js_extend_h_

#include <libmilk/script_js.h>

///	namespace lyramilk::teapoy::sni

namespace lyramilk{ namespace teapoy{ namespace sni{
	class js_extend:public lyramilk::script::js::script_js
	{
	  public:
		js_extend();
		virtual ~js_extend();
		static void engine_load(const char* scripttypename);
		virtual void inite();
	};
}}}

#endif
