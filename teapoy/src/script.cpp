#include "script.h"
#include <libmilk/log.h>
#include <libmilk/dict.h>

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


	// invoker_map
	void invoker_map::define(lyramilk::data::string name,lyramilk::data::string ptr)
	{
		lyramilk::threading::mutex_sync _(l.w());
		m[name] = ptr;
	}

	void invoker_map::undef(lyramilk::data::string name)
	{
		lyramilk::threading::mutex_sync _(l.w());
		m.erase(name);
	}

	lyramilk::data::array invoker_map::keys() const
	{
		lyramilk::threading::mutex_sync _(l.r());
		lyramilk::data::array ar;
		lyramilk::data::stringdict::const_iterator it = m.begin();
		for(;it!=m.end();++it){
			ar.push_back(it->first);
		}
		return ar;
	}

	lyramilk::data::string invoker_map::get(lyramilk::data::string name)
	{
		lyramilk::threading::mutex_sync _(l.r());
		lyramilk::data::stringdict::iterator it = m.find(name);
		if(it == m.end()){
			return "";
		}
		return it->second;
	}

	invoker_map* invoker_map::instance()
	{
		static invoker_map _mm;
		return &_mm;
	}

	// engine_master_js
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
			o->reset();
		}

		virtual void onremove(lyramilk::script::engine* o)
		{
			lyramilk::script::engine::destoryinstance("js",o);
		}
	};


	// engine_master_jshtml
	class engine_master_jshtml:public lyramilk::script::engines
	{
	  public:
		engine_master_jshtml()
		{
		}

		virtual ~engine_master_jshtml()
		{
		}

		static engine_master_jshtml* instance()
		{
			static engine_master_jshtml _mm;
			return &_mm;
		}

		virtual lyramilk::script::engine* underflow()
		{
			lyramilk::script::engine* eng_tmp = lyramilk::script::engine::createinstance("jshtml");
			if(!eng_tmp){
				lyramilk::klog(lyramilk::log::error,"teapoy.web.engine_master_jshtml.underflow") << D("创建引擎对象失败(%s)","engine_master_jshtml") << std::endl;
				return nullptr;
			}

			lyramilk::teapoy::script_interface_master::instance()->apply(eng_tmp);
			return eng_tmp;
		}

		virtual void onfire(lyramilk::script::engine* o)
		{
			o->reset();
		}

		virtual void onremove(lyramilk::script::engine* o)
		{
			lyramilk::script::engine::destoryinstance("jshtml",o);
		}
	};

	// engine_master_lua
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
		engine_pool::instance()->define("jshtml",new engine_master_jshtml);
		engine_pool::instance()->define("lua",new engine_master_lua);
	}

}}

