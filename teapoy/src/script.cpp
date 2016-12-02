#include "script.h"


namespace lyramilk{ namespace teapoy
{
	int script2native::fill(lyramilk::data::string libname,lyramilk::script::engine* p)
	{
		int i = 0;
		std::map<lyramilk::data::string,std::vector<define_func> >::iterator it = c.find(libname);
		if(it!=c.end()){
			std::vector<define_func>::iterator itc = it->second.begin();
			for(;itc!=it->second.end();++itc){
				i += (*itc)(p);
			}
		}
		return i;
	}

	int script2native::fill(lyramilk::script::engine* p)
	{
		int i = 0;
		std::map<lyramilk::data::string,std::vector<define_func> >::iterator it = c.begin();
		for(;it!=c.end();++it){
			std::vector<define_func>::iterator itc = it->second.begin();
			for(;itc!=it->second.end();++itc){
				i += (*itc)(p);
			}
		}
		return i;
	}

	void script2native::regist(lyramilk::data::string libname,define_func func)
	{
		c[libname].push_back(func);
	}

	void script2native::unregist(lyramilk::data::string libname)
	{
		c.erase(libname);
	}

	script2native* script2native::instance()
	{
		static script2native _mm;
		return &_mm;
	}
}}

