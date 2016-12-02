#ifndef _lyramilk_teapoy_script_h_
#define _lyramilk_teapoy_script_h_

#include <libmilk/scriptengine.h>

namespace lyramilk{ namespace teapoy
{

	class script2native
	{
	  public:
		typedef int (*define_func)(lyramilk::script::engine* p);

		int fill(lyramilk::script::engine* p);
		int fill(lyramilk::data::string libname,lyramilk::script::engine* p);

		void regist(lyramilk::data::string libname,define_func func);
		void unregist(lyramilk::data::string libname);

		static script2native* instance();
	  protected:
		std::map<lyramilk::data::string,std::vector<define_func> > c;
	};
}}

#endif
