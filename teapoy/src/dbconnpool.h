#ifndef _lyramilk_teapoy_dbconnpool_h_
#define _lyramilk_teapoy_dbconnpool_h_

#include "config.h"
#include "redis.h"
#include <libmilk/factory.h>
#include <libmilk/atom.h>
#include <mongo/client/dbclient.h>
#include <map>

#ifndef MAROC_MYSQL
	#define MAROC_MYSQL void
#endif

namespace lyramilk{ namespace teapoy {
	struct mysql_client
	{
		MAROC_MYSQL* _db_ptr;
		mysql_client();
		~mysql_client();
	};

	class mysql_clients:public lyramilk::threading::rentlist<lyramilk::teapoy::mysql_client>
	{
		lyramilk::data::var::array opts;
		lyramilk::data::string cnf;
		lyramilk::data::var group;
	  public:
		mysql_clients(const lyramilk::data::var& cfg);
		virtual ~mysql_clients();
		virtual lyramilk::teapoy::mysql_client* underflow();
	};

	class mysql_clients_multiton:public lyramilk::util::multiton_factory<mysql_clients>
	{
		lyramilk::threading::mutex_spin lock;
		lyramilk::data::var::map cfgmap;
	  public:
		static mysql_clients_multiton* instance();
		virtual mysql_clients::ptr getobj(lyramilk::data::string id);

		virtual void add_config(lyramilk::data::string id,const lyramilk::data::var& cfg);
		virtual const lyramilk::data::var& get_config(lyramilk::data::string id);
	};

	///////////////////////////////////////////////////////////

	class redis_clients:public lyramilk::threading::rentlist<lyramilk::teapoy::redis::redis_client>
	{
		lyramilk::data::string host;
		lyramilk::data::uint16 port;
		lyramilk::data::string pwd;
		bool readonly;
		bool withlistener;
	  public:
		redis_clients(const lyramilk::data::var& cfg);
		virtual ~redis_clients();
		virtual lyramilk::teapoy::redis::redis_client* underflow();
	};

	class redis_clients_multiton:public lyramilk::util::multiton_factory<redis_clients>
	{
		lyramilk::threading::mutex_spin lock;

		lyramilk::data::var::map cfgmap;
	  public:
		static redis_clients_multiton* instance();
		virtual redis_clients::ptr getobj(lyramilk::data::string id);
		virtual void add_config(lyramilk::data::string id,const lyramilk::data::var& cfg);
		virtual const lyramilk::data::var& get_config(lyramilk::data::string id);
	};
	///////////////////////////////////////////////////////////
	class mongo_client
	{
	  public:
		lyramilk::data::string user;
		lyramilk::data::string pwd;
		::mongo::DBClientConnection c;
		mongo_client();
		virtual ~mongo_client();
	};


	class mongo_clients:public lyramilk::threading::rentlist<lyramilk::teapoy::mongo_client>
	{
		lyramilk::data::string host;
		lyramilk::data::uint16 port;
		lyramilk::data::string user;
		lyramilk::data::string pwd;
	  public:
		mongo_clients(const lyramilk::data::var& cfg);
		virtual ~mongo_clients();
		virtual lyramilk::teapoy::mongo_client* underflow();
	};

	class mongo_clients_multiton:public lyramilk::util::multiton_factory<mongo_clients>
	{
		lyramilk::threading::mutex_spin lock;

		lyramilk::data::var::map cfgmap;
	  public:
		static mongo_clients_multiton* instance();
		virtual mongo_clients::ptr getobj(lyramilk::data::string id);
		virtual void add_config(lyramilk::data::string id,const lyramilk::data::var& cfg);
		virtual const lyramilk::data::var& get_config(lyramilk::data::string id);
	};
	///////////////////////////////////////////////////////////
	class filelogers
	{
		lyramilk::data::string filepath;
		FILE* fp;
		mutable tm daytime;
		mutable lyramilk::threading::mutex_os lock;
	  public:
		filelogers(const lyramilk::data::var& cfg);
		virtual ~filelogers();

		bool good();
		bool write(const char* str,std::size_t sz);
	};

	class filelogers_multiton:public lyramilk::util::multiton_factory<filelogers>
	{
		lyramilk::threading::mutex_spin lock;

		lyramilk::data::var::map cfgmap;
	  public:
		static filelogers_multiton* instance();
		virtual filelogers* getobj(lyramilk::data::string id);
		virtual void add_config(lyramilk::data::string id,const lyramilk::data::var& cfg);
		virtual const lyramilk::data::var& get_config(lyramilk::data::string id);
	};
}}

#endif
