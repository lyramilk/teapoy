#ifndef _teapoyd_js_extend_h_
#define _teapoyd_js_extend_h_

#include <libmilk/script_js.h>

///	namespace lyramilk::teapoy::sni

namespace lyramilk{ namespace teapoy{ namespace sni{
	using lyramilk::script::js::script_js;

	class js_extend:public lyramilk::script::js::script_js
	{
	  public:
		js_extend();
		virtual ~js_extend();
		static void engine_load(const char* scripttypename);
		virtual void inite();
	};

	class jshtml:public js_extend
	{
	  public:
		jshtml();
		virtual ~jshtml();
		static void engine_load(const char* scripttypename);
		virtual lyramilk::data::string code_convert(const lyramilk::data::string& code,std::vector<int>* prlines);
		virtual bool load_string(const lyramilk::data::string& scriptstring);
		virtual bool load_file(const lyramilk::data::string& scriptfile);
	};
}}}

#endif
