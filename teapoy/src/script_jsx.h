#ifndef _lyramilk_script_jsx_engine_h_
#define _lyramilk_script_jsx_engine_h_
#include <libmilk/def.h>
#include <libmilk/script_js.h>

namespace lyramilk{namespace script{namespace js
{
	class script_jsx : public lyramilk::script::js::script_js
	{
	  public:
		script_jsx();
		virtual ~script_jsx();
		virtual bool load_string(lyramilk::data::string script);
		virtual bool load_file(lyramilk::data::string scriptfile);
		virtual lyramilk::data::string name();
	};

}}}

#endif
