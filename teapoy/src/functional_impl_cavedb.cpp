#include "functional_impl_cavedb.h"
#include <stdlib.h>

namespace lyramilk{ namespace teapoy {
	#define FUNCTIONAL_TYPE	"cavedb"

		lyramilk::cave::leveldb_minimal_adapter rsdb;
		lyramilk::cave::slave_ssdb slave;

	bool functional_impl_cavedb_instance::init(const lyramilk::data::map& cm)
	{
		lyramilk::data::map m = cm;

		lyramilk::data::string leveldbpath = m["leveldbpath"].str();
		system(("mkdir -p " + leveldbpath).c_str());
		lyramilk::data::string ssdbhost = m["host"].str();
		unsigned short ssdbport = m["port"];
		lyramilk::data::string ssdbpwd = m["password"].str();
		unsigned long long cache_MB = m["cache_MB"];
		if(rsdb.open_leveldb(leveldbpath,cache_MB,true)){
			lyramilk::data::string replid = "";
			lyramilk::data::uint64 offset = 0;
			if(rsdb.get_sync_info(&replid,&offset)){
				slave.slaveof(ssdbhost,ssdbport,ssdbpwd,replid,offset,&rsdb);
				return true;
			}
		}
		return false;
	}

	lyramilk::data::var functional_impl_cavedb_instance::exec(const lyramilk::data::array& ar)
	{
		TODO();
		return true;
	}

	bool functional_impl_cavedb_instance::hexist(const lyramilk::data::string& key,const lyramilk::data::string& field) const
	{
		return rsdb.hexist(key,field);
	}

	lyramilk::data::string functional_impl_cavedb_instance::hget(const lyramilk::data::string& key,const lyramilk::data::string& field) const
	{
		return rsdb.hget(key,field);
	}

	lyramilk::data::stringdict functional_impl_cavedb_instance::hgetall(const lyramilk::data::string& key) const
	{
		return rsdb.hgetall(key);
	}

	lyramilk::data::map functional_impl_cavedb_instance::hgetallv(const lyramilk::data::string& key) const
	{
		return rsdb.hgetallv(key);
	}

	functional_impl_cavedb_instance::functional_impl_cavedb_instance()
	{
	}

	functional_impl_cavedb_instance::~functional_impl_cavedb_instance()
	{
	}

	// functional_impl_cavedb
	functional_impl_cavedb::functional_impl_cavedb()
	{
		ins = nullptr;
	}
	functional_impl_cavedb::~functional_impl_cavedb()
	{
		if(ins) delete ins;
	}

	bool functional_impl_cavedb::init(const lyramilk::data::map& m)
	{
		info = m;
		ins = new functional_impl_cavedb_instance;
		ins->init(info);
		return true;
	}

	functional::ptr functional_impl_cavedb::get_instance()
	{

		return ins;
	}

	lyramilk::data::string functional_impl_cavedb::name()
	{
		return FUNCTIONAL_TYPE;
	}

	//
	static functional_multi* ctr(void* args)
	{
		return new functional_impl_cavedb;
	}

	static void dtr(functional_multi* p)
	{
		delete (functional_impl_cavedb*)p;
	}

	static __attribute__ ((constructor)) void __init()
	{
		functional_type_master::instance()->define(FUNCTIONAL_TYPE,ctr,dtr);
	}
}}
