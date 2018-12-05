#ifndef _lyramilk_functional_impl_redis_h_
#define _lyramilk_functional_impl_redis_h_

#include "functional_master.h"
#include "redis.h"
#include <list>
#include <libmilk/thread.h>

namespace lyramilk{ namespace teapoy {

	class functional_impl_redis_instance:public functional_nonreentrant
	{
	  public:
		virtual bool init(const lyramilk::data::map& m);
		virtual lyramilk::data::var exec(const lyramilk::data::array& ar);
		redis::redis_client* c;
	  public:
		functional_impl_redis_instance();
		virtual ~functional_impl_redis_instance();
	};

	class functional_impl_redis:public functional_multi
	{
		lyramilk::threading::mutex_spin l;
		lyramilk::data::map info;
	  public:
		functional_impl_redis();
	  	virtual ~functional_impl_redis();

		virtual bool init(const lyramilk::data::map& m);
		virtual functional::ptr get_instance();
		virtual lyramilk::data::string name();
	  protected:
		typedef std::list<functional_impl_redis_instance*> list_type;
		list_type es;
		virtual functional_impl_redis_instance* underflow();
	};
}}

#endif
