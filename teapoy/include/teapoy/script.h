#ifndef _lyramilk_teapoy_script_h_
#define _lyramilk_teapoy_script_h_

#include <libmilk/scriptengine.h>
#include <libmilk/factory.h>
#include <libmilk/thread.h>

namespace lyramilk{ namespace teapoy
{

	class script_interface_master
	{
	  public:
		typedef int (*define_func)(lyramilk::script::engine* p);

		int apply(lyramilk::script::engine* p);
		int apply(const lyramilk::data::string& libname,lyramilk::script::engine* p);

		void regist(const lyramilk::data::string& libname,define_func func);
		void unregist(const lyramilk::data::string& libname);

		static script_interface_master* instance();
	  protected:
		std::map<lyramilk::data::string,std::vector<define_func> > c;
	};

	class engine_pool:public lyramilk::util::multiton_factory<lyramilk::script::engines>
	{
	  public:
		static engine_pool* instance();

	  	virtual lyramilk::script::engine* create_script_instance(const lyramilk::data::string& libname);
	  	virtual void destory_script_instance(const lyramilk::data::string& libname,lyramilk::script::engine* eng);
	};

	class invoker_map
	{
		lyramilk::data::stringdict m;
		mutable lyramilk::threading::mutex_rw l;
	  public:
		virtual void define(const lyramilk::data::string& name,const lyramilk::data::string& ptr);
		virtual void undef(const lyramilk::data::string& name);
		virtual lyramilk::data::array keys() const;
		virtual lyramilk::data::string get(const lyramilk::data::string& name);

		static invoker_map* instance();
	};
}}

#endif
