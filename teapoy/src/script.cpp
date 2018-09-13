#include "script.h"
#include <libmilk/log.h>
#include <libmilk/multilanguage.h>

namespace lyramilk{ namespace teapoy
{
	int script_interface_master::apply(lyramilk::data::string libname,lyramilk::script::engine* p)
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

	int script_interface_master::apply(lyramilk::script::engine* p)
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

	void script_interface_master::regist(lyramilk::data::string libname,define_func func)
	{
		c[libname].push_back(func);
	}

	void script_interface_master::unregist(lyramilk::data::string libname)
	{
		c.erase(libname);
	}

	script_interface_master* script_interface_master::instance()
	{
		static script_interface_master _mm;
		return &_mm;
	}

	engine_pool* engine_pool::instance()
	{
		static engine_pool _mm;
		return &_mm;
	}


	class engine_master_js:public lyramilk::script::engines
	{
	  public:
		engine_master_js()
		{
		}

		virtual ~engine_master_js()
		{
		}

		static engine_master_js* instance()
		{
			static engine_master_js _mm;
			return &_mm;
		}

		virtual lyramilk::script::engine* underflow()
		{
			lyramilk::script::engine* eng_tmp = lyramilk::script::engine::createinstance("js");
			if(!eng_tmp){
				lyramilk::klog(lyramilk::log::error,"teapoy.web.engine_master_js.underflow") << D("创建引擎对象失败(%s)","js") << std::endl;
				return nullptr;
			}

			lyramilk::teapoy::script_interface_master::instance()->apply(eng_tmp);
			return eng_tmp;
		}

		virtual void onfire(lyramilk::script::engine* o)
		{
			o->set("clearonreset",lyramilk::data::var::map());
			o->reset();
		}

		virtual void onremove(lyramilk::script::engine* o)
		{
			lyramilk::script::engine::destoryinstance("js",o);
		}
	};


	class engine_master_jsx:public lyramilk::script::engines
	{
	  public:
		engine_master_jsx()
		{
		}

		virtual ~engine_master_jsx()
		{
		}

		static engine_master_jsx* instance()
		{
			static engine_master_jsx _mm;
			return &_mm;
		}

		virtual lyramilk::script::engine* underflow()
		{
			lyramilk::script::engine* eng_tmp = lyramilk::script::engine::createinstance("jsx");
			if(!eng_tmp){
				lyramilk::klog(lyramilk::log::error,"teapoy.web.engine_master_jsx.underflow") << D("创建引擎对象失败(%s)","jsx") << std::endl;
				return nullptr;
			}

			lyramilk::teapoy::script_interface_master::instance()->apply(eng_tmp);
			return eng_tmp;
		}

		virtual void onfire(lyramilk::script::engine* o)
		{
			o->set("clearonreset",lyramilk::data::var::map());
			o->reset();
		}

		virtual void onremove(lyramilk::script::engine* o)
		{
			lyramilk::script::engine::destoryinstance("jsx",o);
		}
	};

	class engine_master_lua:public lyramilk::script::engines
	{
	  public:
		engine_master_lua()
		{
		}

		virtual ~engine_master_lua()
		{
		}

		static engine_master_lua* instance()
		{
			static engine_master_lua _mm;
			return &_mm;
		}

		virtual lyramilk::script::engine* underflow()
		{
			lyramilk::script::engine* eng_tmp = lyramilk::script::engine::createinstance("lua");
			if(!eng_tmp){
				lyramilk::klog(lyramilk::log::error,"teapoy.web.engine_master_lua.underflow") << D("创建引擎对象失败(%s)","lua") << std::endl;
				return nullptr;
			}

			lyramilk::teapoy::script_interface_master::instance()->apply(eng_tmp);
			return eng_tmp;
		}

		virtual void onfire(lyramilk::script::engine* o)
		{
			o->set("clearonreset",lyramilk::data::var::map());
			o->reset();
		}

		virtual void onremove(lyramilk::script::engine* o)
		{
			lyramilk::script::engine::destoryinstance("lua",o);
		}
	};


	static __attribute__ ((constructor)) void __init()
	{
		engine_pool::instance()->define("js",new engine_master_js);
		engine_pool::instance()->define("jsx",new engine_master_jsx);
		engine_pool::instance()->define("lua",new engine_master_lua);
	}

}}

