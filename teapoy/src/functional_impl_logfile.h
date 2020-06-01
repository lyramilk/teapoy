#ifndef _lyramilk_functional_impl_logfile_h_
#define _lyramilk_functional_impl_logfile_h_

#include "functional_master.h"
#include <libmilk/thread.h>
#include <libmilk/log.h>

namespace lyramilk{ namespace teapoy {

	class functional_impl_logfile_instance:public functional
	{
		lyramilk::log::logfile lf;
	  public:
		virtual bool init(const lyramilk::data::map& m);
		virtual lyramilk::data::var exec(const lyramilk::data::array& ar);
	  public:
		functional_impl_logfile_instance();
		virtual ~functional_impl_logfile_instance();
	};

	class functional_impl_logfile:public functional_multi
	{
		lyramilk::threading::mutex_spin l;
		lyramilk::data::map info;
	  public:
		functional_impl_logfile();
	  	virtual ~functional_impl_logfile();

		virtual bool init(const lyramilk::data::map& m);
		virtual functional::ptr get_instance();
		virtual lyramilk::data::string name();
	  protected:
		functional_impl_logfile_instance* ins;
	};

}}

#endif
