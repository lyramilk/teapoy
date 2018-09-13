#ifndef _lyramilk_teapoy_functional_master_h_
#define _lyramilk_teapoy_functional_master_h_

#include "config.h"
#include <libmilk/factory.h>
#include <libmilk/var.h>
#include <libmilk/gc.h>

namespace lyramilk{ namespace teapoy {

	// 具体的动作
	class functional:public lyramilk::obj
	{
	  public:
		virtual bool init(const lyramilk::data::var::map& m) = 0;
		virtual lyramilk::data::var exec(const lyramilk::data::var::array& ar) = 0;
		virtual bool try_del();
	  public:
		typedef lyramilk::ptr<functional> ptr;
	};

	// 包装具体动作的池，多次获取实例应该可以得到线程安全的动作实力，如果动作本身不是线程安全的，池需要做出分配。
	class functional_multi
	{
	  public:
		virtual bool init(const lyramilk::data::var::map& m) = 0;
		virtual functional::ptr get_instance() = 0;
		virtual lyramilk::data::string name() = 0;
	};

	// 可以通过一个路径获得一个动作池对象
	class functional_master:public lyramilk::util::multiton_factory<functional_multi>
	{
	  public:
		static functional_master* instance();
	};

	// 可以通过一个描述类型的字符串管理对象创建
	class functional_type_master:public lyramilk::util::factory<functional_multi>
	{
	  public:
		static functional_type_master* instance();
	};


	
	class functional_mutex:public functional
	{
		bool l;
	  public:
		functional_mutex();
	  	virtual ~functional_mutex();
		virtual void lock();
		virtual void unlock();
		virtual bool try_lock();
		virtual bool try_del();
	};
}}

#endif
