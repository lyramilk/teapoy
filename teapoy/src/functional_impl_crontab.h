#ifndef _lyramilk_functional_impl_crontab_h_
#define _lyramilk_functional_impl_crontab_h_

#include "functional_master.h"
#include <libmilk/thread.h>

namespace lyramilk{ namespace teapoy {

	class functional_impl_crontab_instance:public functional_nonreentrant
	{
	  public:
		virtual bool init(const lyramilk::data::map& m);
		virtual lyramilk::data::var exec(const lyramilk::data::array& ar);
	  public:
		functional_impl_crontab_instance();
		virtual ~functional_impl_crontab_instance();
	};

	class functional_impl_crontab:public functional_multi
	{
		lyramilk::threading::mutex_spin l;
		lyramilk::data::map info;
	  public:
		functional_impl_crontab();
	  	virtual ~functional_impl_crontab();

		virtual bool init(const lyramilk::data::map& m);
		virtual functional::ptr get_instance();
		virtual lyramilk::data::string name();
	  protected:
		typedef std::list<functional_impl_crontab_instance*> list_type;
		list_type es;
		virtual functional_impl_crontab_instance* underflow();
	};

}}

#endif
