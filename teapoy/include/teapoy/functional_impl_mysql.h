#ifndef _lyramilk_functional_impl_mysql_h_
#define _lyramilk_functional_impl_mysql_h_

#include "functional_master.h"
#include <libmilk/thread.h>
#include <mysql/mysql.h>

namespace lyramilk{ namespace teapoy {

	class functional_impl_mysql_instance:public functional_nonreentrant
	{
	  public:
		virtual bool init(const lyramilk::data::map& m);
		virtual lyramilk::data::var exec(const lyramilk::data::array& ar);
		MYSQL* _db_ptr;
	  public:
		functional_impl_mysql_instance();
		virtual ~functional_impl_mysql_instance();
	};

	class functional_impl_mysql:public functional_multi
	{
		lyramilk::threading::mutex_spin l;
		lyramilk::data::map info;
	  public:
		functional_impl_mysql();
	  	virtual ~functional_impl_mysql();

		virtual bool init(const lyramilk::data::map& m);
		virtual functional::ptr get_instance();
		virtual lyramilk::data::string name();
	  protected:
		typedef std::list<functional_impl_mysql_instance*> list_type;
		list_type es;
		virtual functional_impl_mysql_instance* underflow();
	};

}}

#endif
