#include "functional_impl_cavedb.h"
#include <stdlib.h>

namespace lyramilk{ namespace teapoy {
	#define FUNCTIONAL_TYPE	"cavedb"

	bool functional_impl_cavedb_instance::init(const lyramilk::data::map& cm)
	{
		lyramilk::data::map m = cm;

		lyramilk::data::string leveldbpath = m["leveldbpath"].str();
		system(("mkdir -p " + leveldbpath).c_str());
		lyramilk::data::string masterid = m["masterid"].str();
		lyramilk::data::string host = m["host"].str();
		unsigned short port = m["port"];
		lyramilk::data::string masterauth = m["password"].str();
		unsigned long long cache_MB = m["cache_MB"];

		receiver.chd.isreadonly = false;


		if(!store.open_leveldb(leveldbpath,cache_MB,true)){
			return false;
		}

		lyramilk::data::string replid = "";
		lyramilk::data::uint64 offset = 0;
		if(!store.get_sync_info(masterid,&replid,&offset)){
			return false;
		}

		receiver.init(host,port,masterauth,masterid,replid,offset,&store);
		receiver.active(1);
		return true;
	}

	lyramilk::data::var functional_impl_cavedb_instance::exec(const lyramilk::data::array& ar)
	{
		TODO();
		return true;
	}

	bool functional_impl_cavedb_instance::hexist(const lyramilk::data::string& key,const lyramilk::data::string& field) const
	{
		lyramilk::data::string ret;
		return store.hget(key,field,&ret) == lyramilk::cave::cs_data;
	}

	lyramilk::data::string functional_impl_cavedb_instance::hget(const lyramilk::data::string& key,const lyramilk::data::string& field) const
	{
		lyramilk::data::string ret;
		store.hget(key,field,&ret);
		return ret;
	}

	lyramilk::data::stringdict functional_impl_cavedb_instance::hgetall(const lyramilk::data::string& key) const
	{
		lyramilk::data::stringdict ret;
		store.hgetall(key,&ret);
		return ret;
	}

	lyramilk::data::map functional_impl_cavedb_instance::hgetallv(const lyramilk::data::string& key) const
	{
		lyramilk::data::stringdict values;
		store.hgetall(key,&values);

		lyramilk::data::map ret;

		lyramilk::data::stringdict::const_iterator it = values.begin();
		for(;it!=values.end();++it){
			ret[it->first] = it->second;
		}

		return ret;
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
