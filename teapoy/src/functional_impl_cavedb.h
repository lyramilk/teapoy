#ifndef _lyramilk_functional_impl_cavedb_h_
#define _lyramilk_functional_impl_cavedb_h_

#include "functional_master.h"
#include <libmilk/thread.h>
#include <libmilk/log.h>
#include <cavedb/slave_ssdb.h>
#include <cavedb/store/leveldb_minimal_adapter.h>


namespace lyramilk{ namespace teapoy {
	class functional_impl_cavedb_instance:public functional
	{
		lyramilk::cave::leveldb_minimal_adapter rsdb;
		lyramilk::cave::slave_ssdb slave;
	  public:
		virtual bool init(const lyramilk::data::map& m);
		virtual lyramilk::data::var exec(const lyramilk::data::array& ar);

		virtual bool hexist(const lyramilk::data::string& key,const lyramilk::data::string& field) const;
		virtual lyramilk::data::string hget(const lyramilk::data::string& key,const lyramilk::data::string& field) const;
		virtual lyramilk::data::stringdict hgetall(const lyramilk::data::string& key) const;
		virtual lyramilk::data::map hgetallv(const lyramilk::data::string& key) const;
	  public:
		functional_impl_cavedb_instance();
		virtual ~functional_impl_cavedb_instance();
	};

	class functional_impl_cavedb:public functional_multi
	{
		lyramilk::threading::mutex_spin l;
		lyramilk::data::map info;
	  public:
		functional_impl_cavedb();
	  	virtual ~functional_impl_cavedb();

		virtual bool init(const lyramilk::data::map& m);
		virtual functional::ptr get_instance();
		virtual lyramilk::data::string name();
	  protected:
		functional_impl_cavedb_instance* ins;
	};
}}

#endif
