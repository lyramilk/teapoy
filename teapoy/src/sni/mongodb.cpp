#include "script.h"
#include <mongo/client/dbclient.h>
#include <libmilk/log.h>
#include <libmilk/multilanguage.h>
#include <libmilk/json.h>

namespace lyramilk{ namespace teapoy{ namespace native{

	class mymongodb_iterator
	{
		static void* ctr(const lyramilk::data::var::array& args)
		{
			return nullptr;
		}
		static void dtr(void* p)
		{
			delete (mymongodb_iterator*)p;
		}
	  public:
		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			p->define("Mongo.iterator",fn,mymongodb_iterator::ctr,mymongodb_iterator::dtr);
			return 1;
		}

		std::auto_ptr< ::mongo::DBClientCursor> cursor;
	};


	class mymongodb
	{
		lyramilk::log::logss log;
		::mongo::DBClientConnection c;
		std::string errmsg;

		static void* ctr(const lyramilk::data::var::array& args)
		{
			if(args.size() == 2){
				return new mymongodb(args[0],args[1]);
			}else if(args.size() == 1){
				const lyramilk::data::var& v = args[0];
				lyramilk::data::string host = v["host"];
				lyramilk::data::uint16 port = v["port"];
				return new mymongodb(host,port);
			}
			return nullptr;
		}
		static void dtr(void* p)
		{
			delete (mymongodb*)p;
		}

		mymongodb(lyramilk::data::string host,lyramilk::data::uint16 port):log(lyramilk::klog,"teapoy.native.mongo")
		{
			::mongo::HostAndPort hap(host.c_str(),port);
			if(!c.connect(hap,errmsg)){
				log(lyramilk::log::error,"ctr") << D("初始化失败：%s",errmsg.c_str()) << std::endl;
			}
		}

		~mymongodb()
		{}

		lyramilk::data::var insert(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_map);

			lyramilk::data::string str = args[0];
			lyramilk::data::var v = args[1];
			lyramilk::data::json j(v);
			lyramilk::data::string jsonstr = j.str();

			::mongo::BSONObj bson = ::mongo::fromjson(jsonstr.c_str());
			if(!bson.isValid()) return false;
			c.insert(str.c_str(),bson);
			return true;
		}
		
		lyramilk::data::var query(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_map);

			lyramilk::data::string str = args[0];
			lyramilk::data::var v = args[1];
			lyramilk::data::json j(v);
			lyramilk::data::string jsonstr = j.str();

			::mongo::BSONObj bson = ::mongo::fromjson(jsonstr.c_str());
			if(!bson.isValid()) return false;
			std::auto_ptr< ::mongo::DBClientCursor> cursor = c.query(str.c_str(),bson);
			return true;
		}
	  public:
		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["insert"] = lyramilk::script::engine::functional<mymongodb,&mymongodb::insert>;
			p->define("Mongo",fn,mymongodb::ctr,mymongodb::dtr);
			return 1;
		}
	};

	static int define(lyramilk::script::engine* p)
	{
		int i = 0;
		i+= mymongodb::define(p);
		return i;
	}


	static __attribute__ ((constructor)) void __init()
	{
		::mongo::client::initialize();
		lyramilk::teapoy::script2native::instance()->regist("mongodb",define);
	}
}}}
