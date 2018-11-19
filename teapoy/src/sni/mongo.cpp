#include "script.h"
#include "dbconnpool.h"
#include <mongo/client/dbclient.h>
#include <libmilk/log.h>
#include <libmilk/dict.h>
#include <libmilk/json.h>

namespace lyramilk{ namespace teapoy{ namespace native{


	static bool bson2var(const ::mongo::BSONObj& bson,lyramilk::data::var& v)
	{
		std::string resultstr = bson.jsonString();
		lyramilk::data::json j(v);
		j.str(lyramilk::data::string(resultstr.c_str(),resultstr.size()));
		return v.type() != lyramilk::data::var::t_invalid;
	}

	static bool var2bson(const lyramilk::data::map& vm,::mongo::BSONObj& bson)
	{
		lyramilk::data::var v(vm);
		lyramilk::data::json j(v);
		lyramilk::data::string jsonstr = j.str();
		bson = ::mongo::fromjson(std::string(jsonstr.c_str(),jsonstr.size()));
		return bson.isValid();
	}

	class mymongo
	{
		lyramilk::log::logss log;
		::mongo::DBClientConnection mc;
		std::string errmsg;
		lyramilk::teapoy::mongo_clients::ptr ptr;
		lyramilk::data::string user;
		lyramilk::data::string pwd;

		::mongo::DBClientConnection* p;

		static void* ctr(const lyramilk::data::array& args)
		{
			if(args.size() == 4){
				mymongo *p = new mymongo(args[0],args[1],args[2],args[3]);
				if(!p->p){
					delete p;
					return nullptr;
				}
				return p;
			}else if(args.size() == 1){
				const lyramilk::data::var& v = args[0];
				if(v.type() == lyramilk::data::var::t_user){
					const void* p = args[0].userdata("__mongo_client");
					if(p){
						mongo_clients::ptr* pp = (mongo_clients::ptr*)p;
						mongo_clients::ptr ppr = *pp;
						if(ppr){
							return new mymongo(ppr);
						}
					}
					throw lyramilk::exception(D("mongo错误： 无法从池中获取到命名对象。"));
				}else if(v.type() == lyramilk::data::var::t_str){
					mymongo *p = new mymongo(args[0],27017);
					if(!p->p){
						delete p;
						return nullptr;
					}
					return p;
				}else if(v.type() == lyramilk::data::var::t_map){
					lyramilk::data::string host = v["host"];
					lyramilk::data::uint16 port = v["port"];
					mymongo *p = new mymongo(host,port);
					if(!p->p){
						delete p;
						return nullptr;
					}
					return p;
				}
			}else if(args.size() == 2){
				mymongo *p = new mymongo(args[0],args[1]);
				if(!p->p){
					delete p;
					return nullptr;
				}
				return p;
			}
			return nullptr;
		}
		static void dtr(void* p)
		{
			delete (mymongo*)p;
		}

		mymongo(mongo_clients::ptr ptr):log(lyramilk::klog,"teapoy.native.mongo")
		{
			this->ptr = ptr;
			p = &this->ptr->c;
			if(!p->auth("admin",std::string(ptr->user.c_str(),ptr->user.size()),std::string(ptr->pwd.c_str(),ptr->pwd.size()),errmsg)){
				this->user = ptr->user;
				this->pwd = ptr->pwd;
			}
		}

		mymongo(lyramilk::data::string host,lyramilk::data::uint16 port):log(lyramilk::klog,"teapoy.native.mongo")
		{
			::mongo::HostAndPort hap(host.c_str(),port);
			if(mc.connect(hap,errmsg)){
				p = &mc;
				return;
			}
			log(lyramilk::log::error,"underflow") << D("Mongo(%s:%d)初始化失败：%s",host.c_str(),port,errmsg.c_str()) << std::endl;
		}

		mymongo(lyramilk::data::string host,lyramilk::data::uint16 port,lyramilk::data::string user,lyramilk::data::string pwd):log(lyramilk::klog,"teapoy.native.mongo")
		{
			::mongo::HostAndPort hap(host.c_str(),port);
			if(mc.connect(hap,errmsg)){
				p = &mc;
				if(!p->auth("admin",std::string(user.c_str(),user.size()),std::string(pwd.c_str(),pwd.size()),errmsg)){
					this->user = user;
					this->pwd = pwd;
				}
				return;
			}
			log(lyramilk::log::error,"underflow") << D("Mongo(%s:%d)初始化失败：%s",host.c_str(),port,errmsg.c_str()) << std::endl;
		}

		~mymongo()
		{}

		lyramilk::data::var show_dbs(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			std::list<std::string> ret = p->getDatabaseNames();
			lyramilk::data::array ar;
			ar.reserve(ret.size());
			std::list<std::string>::iterator it = ret.begin();
			for(;it!=ret.end();++it){
				lyramilk::data::string str(it->c_str(),it->size());
				ar.push_back(str);
			}
			return ar;
		}

		lyramilk::data::var db(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string dbname = args[0];
			lyramilk::script::engine* e = (lyramilk::script::engine*)env.find(lyramilk::script::engine::s_env_engine())->second.userdata(lyramilk::script::engine::s_env_engine());

			if(!user.empty()){
				if(!p->auth(std::string(dbname.c_str(),dbname.size()),std::string(user.c_str(),user.size()),std::string(pwd.c_str(),pwd.size()),errmsg)){
					throw lyramilk::exception(errmsg.c_str());
				}
			}

			p->resetError();

			lyramilk::data::array ar;
			ar.reserve(2);
			ar.push_back(lyramilk::data::var("mongo",p));
			ar.push_back(dbname);
			return e->createobject("Mongo.Database",ar);
		}

		lyramilk::data::var exists(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string str = args[0];
			return p->exists(std::string(str.c_str(),str.size()));
		}

		lyramilk::data::var isMaster(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			bool b = false;
			if(p->isMaster(b)) return b;
			throw lyramilk::exception(D("isMaster执行失败"));
		}

	  public:
		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["list"] = lyramilk::script::engine::functional<mymongo,&mymongo::show_dbs>;
			fn["get"] = lyramilk::script::engine::functional<mymongo,&mymongo::db>;
			fn["db"] = lyramilk::script::engine::functional<mymongo,&mymongo::db>;
			fn["exists"] = lyramilk::script::engine::functional<mymongo,&mymongo::exists>;
			fn["isMaster"] = lyramilk::script::engine::functional<mymongo,&mymongo::isMaster>;
			p->define("Mongo",fn,mymongo::ctr,mymongo::dtr);
			return 1;
		}
	};

	class mymongo_db
	{
		std::string dbname;
		lyramilk::log::logss log;
		::mongo::DBClientConnection* p;

		static void* ctr(const lyramilk::data::array& args)
		{
			return new mymongo_db((::mongo::DBClientConnection*)args[0].userdata("mongo"),args[1].str());
		}
		static void dtr(void* p)
		{
			delete (mymongo_db*)p;
		}

		mymongo_db(::mongo::DBClientConnection* dbptr,lyramilk::data::string name):log(lyramilk::klog,"teapoy.native.mongo.db")
		{
			p = dbptr;
			dbname.assign(name.c_str(),name.size());
		}

		lyramilk::data::var login(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_str);
			lyramilk::data::string user = args[0].str();
			lyramilk::data::string pwd = args[1].str();
			std::string errmsg;
			if(p->auth(dbname,std::string(user.c_str(),user.size()),std::string(pwd.c_str(),pwd.size()),errmsg)){
				return true;
			}
			throw lyramilk::exception(lyramilk::data::string(errmsg.c_str(),errmsg.size()));
		}

		lyramilk::data::var logout(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			::mongo::BSONObj bsonret;
			p->logout(dbname,bsonret);
			lyramilk::data::var v;
			bson2var(bsonret,v);
			return v;
		}

		lyramilk::data::var create(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			TODO();
		}

		lyramilk::data::var drop(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string str = args[0];
			std::string ns;
			ns.reserve(dbname.size() + str.size() + 1);
			ns.append(dbname.c_str(),dbname.size());
			ns.push_back('.');
			ns.append(str.c_str(),str.size());
			if(!p->dropCollection(ns)){
				return false;
			}
			return true;
		}

		lyramilk::data::var runCommand(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_str);
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,2,lyramilk::data::var::t_map);

			lyramilk::data::string scmd = args[0];
			lyramilk::data::string sarg = args[1];

			::mongo::BSONObjBuilder b;
			b.append(scmd.c_str(),sarg.c_str());

			lyramilk::data::var v = args[2];
			::mongo::BSONObj bsoninfo;
			var2bson(v,bsoninfo);


			b.appendElements(bsoninfo);

			::mongo::BSONObj bsoncmd = b.obj();

			::mongo::BSONObj bsonresult;
			p->runCommand(dbname,bsoncmd,bsonresult);

			std::string str = p->getLastError(dbname);
			if(!str.empty()){
				throw lyramilk::exception(lyramilk::data::string(str.c_str(),str.size()));
			}
			lyramilk::data::var ret;
			bson2var(bsonresult,ret);
			return ret;
		}

		lyramilk::data::var show_collections(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			std::list<std::string> ret = p->getCollectionNames(dbname);
			lyramilk::data::array ar;
			ar.reserve(ret.size());
			std::list<std::string>::iterator it = ret.begin();
			for(;it!=ret.end();++it){
				lyramilk::data::string str(it->c_str(),it->size());
				ar.push_back(str);
			}
			return ar;
		}

		lyramilk::data::var collection(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string collection = args[0];
			lyramilk::script::engine* e = (lyramilk::script::engine*)env.find(lyramilk::script::engine::s_env_engine())->second.userdata(lyramilk::script::engine::s_env_engine());

			lyramilk::data::array ar;
			ar.reserve(3);
			ar.push_back(lyramilk::data::var("mongo",p));
			ar.push_back(lyramilk::data::string(dbname.c_str(),dbname.size()));
			ar.push_back(collection);
			return e->createobject("Mongo.Collection",ar);
		}

		lyramilk::data::var eval(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string collection = args[0];
			std::string s_collection(collection.c_str(),collection.size());


			::mongo::BSONElement retValue;
			if(args.size() > 1){
				::mongo::BSONObj info;
				::mongo::BSONObj bsonargs;
				var2bson(args[1],bsonargs);
				if(!p->eval(dbname,s_collection,info,retValue,&bsonargs)) return lyramilk::data::var::nil;

			}else{
				::mongo::BSONObj info;
				if(!p->eval(dbname,s_collection,info,retValue,nullptr)) return lyramilk::data::var::nil;
			}
			lyramilk::data::var ret;
			bson2var(retValue.Obj(),ret);
			return ret;
		}
	  public:
		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["login"] = lyramilk::script::engine::functional<mymongo_db,&mymongo_db::login>;
			fn["logout"] = lyramilk::script::engine::functional<mymongo_db,&mymongo_db::logout>;
			fn["remove"] = lyramilk::script::engine::functional<mymongo_db,&mymongo_db::drop>;
			fn["command"] = lyramilk::script::engine::functional<mymongo_db,&mymongo_db::runCommand>;
			fn["list"] = lyramilk::script::engine::functional<mymongo_db,&mymongo_db::show_collections>;
			fn["get"] = lyramilk::script::engine::functional<mymongo_db,&mymongo_db::collection>;
			fn["collection"] = lyramilk::script::engine::functional<mymongo_db,&mymongo_db::collection>;
			fn["eval"] = lyramilk::script::engine::functional<mymongo_db,&mymongo_db::eval>;
			p->define("Mongo.Database",fn,mymongo_db::ctr,mymongo_db::dtr);
			return 1;
		}
	};

	class mymongo_collection
	{
		std::string dbname;
		std::string ns;
		lyramilk::log::logss log;
		::mongo::DBClientConnection* p;

		static void* ctr(const lyramilk::data::array& args)
		{
			return new mymongo_collection((::mongo::DBClientConnection*)args[0].userdata("mongo"),args[1].str(),args[2].str());
		}
		static void dtr(void* p)
		{
			delete (mymongo_collection*)p;
		}

		mymongo_collection(::mongo::DBClientConnection* dbptr,lyramilk::data::string dbname,lyramilk::data::string collectionname):log(lyramilk::klog,"teapoy.native.mongo.co")
		{
			this->p = dbptr;
			this->dbname.assign(dbname.c_str(),dbname.size());
			ns.reserve(this->dbname.size() + 1 + collectionname.size());
			ns.append(this->dbname);
			ns.push_back('.');
			ns.append(collectionname.c_str(),collectionname.size());
		}

		virtual ~mymongo_collection()
		{
		}

		lyramilk::data::var query(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			lyramilk::data::map querycond;
			if(args.size() > 0){
				MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_map);
				querycond = args[0];
			}

			lyramilk::data::string jsonstr;
			if(querycond.empty()){
				jsonstr = "{}";
			}else{
				lyramilk::data::var v = querycond;
				lyramilk::data::json j(v);
				jsonstr = j.str();
			}

			lyramilk::script::engine* e = (lyramilk::script::engine*)env.find(lyramilk::script::engine::s_env_engine())->second.userdata(lyramilk::script::engine::s_env_engine());
			lyramilk::data::array ar;
			ar.reserve(3);
			ar.push_back(lyramilk::data::var("mongo",p));
			ar.push_back(lyramilk::data::string(dbname.c_str(),dbname.size()));
			ar.push_back(lyramilk::data::string(ns.c_str(),ns.size()));
			ar.push_back(jsonstr);
			return e->createobject("Mongo.Iterator",ar);
		}

		lyramilk::data::var insert(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_map);
			lyramilk::data::var v = args[0];
			::mongo::BSONObj bsonobj;
			var2bson(v,bsonobj);
			p->insert(ns,bsonobj);

			std::string str = p->getLastError(dbname);
			if(!str.empty()){
				throw lyramilk::exception(lyramilk::data::string(str.c_str(),str.size()));
			}
			return true;
		}

		lyramilk::data::var insert_many(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_array);
			const lyramilk::data::array& ar = args[0];

			std::vector< ::mongo::BSONObj > bsons;

			lyramilk::data::array::const_iterator it = ar.begin();
			for(;it!=ar.end();++it){
				lyramilk::data::var v = *it;
				lyramilk::data::json j(v);
				lyramilk::data::string jsonstr = j.str();
				bsons.push_back(::mongo::fromjson(jsonstr.c_str()));
			}
			p->insert(ns,bsons);
			std::string str = p->getLastError(dbname);
			if(!str.empty()){
				throw lyramilk::exception(lyramilk::data::string(str.c_str(),str.size()));
			}
			return true;
		}

		lyramilk::data::var mapreduce(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_str);
			if(args.size() == 2){
				lyramilk::data::string mapf = args[0];
				std::string s_mapf(mapf.c_str(),mapf.size());
				lyramilk::data::string reducef = args[1];
				std::string s_reducef(reducef.c_str(),reducef.size());

				::mongo::BSONObj bsonret = p->mapreduce(ns,s_mapf,s_reducef);
				lyramilk::data::var ret;
				bson2var(bsonret,ret);
				return ret;

			}else if(args.size() > 3){
				MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,2,lyramilk::data::var::t_map);
				MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,3,lyramilk::data::var::t_map);
				lyramilk::data::string mapf = args[0];
				std::string s_mapf(mapf.c_str(),mapf.size());
				lyramilk::data::string reducef = args[1];
				std::string s_reducef(reducef.c_str(),reducef.size());

				lyramilk::data::var v2 = args[2];
				lyramilk::data::json j2(v2);
				lyramilk::data::string jsonstr2 = j2.str();
				::mongo::BSONObj query = ::mongo::fromjson(jsonstr2.c_str());

				lyramilk::data::var v3 = args[3];
				lyramilk::data::json j3(v3);
				lyramilk::data::string jsonstr3 = j3.str();
				::mongo::BSONObj bsonoutput = ::mongo::fromjson(jsonstr3.c_str());
				::mongo::DBClientWithCommands::MROutput output(bsonoutput);

				::mongo::BSONObj bsonret = p->mapreduce(ns,s_mapf,s_reducef,query,output);
				lyramilk::data::var ret;
				bson2var(bsonret,ret);
				return ret;
			}else if(args.size() > 2){
				MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,2,lyramilk::data::var::t_map);
				lyramilk::data::string mapf = args[0];
				std::string s_mapf(mapf.c_str(),mapf.size());
				lyramilk::data::string reducef = args[1];
				std::string s_reducef(reducef.c_str(),reducef.size());

				lyramilk::data::var v2 = args[2];
				lyramilk::data::json j2(v2);
				lyramilk::data::string jsonstr2 = j2.str();
				::mongo::BSONObj query = ::mongo::fromjson(jsonstr2.c_str());

				::mongo::BSONObj bsonret = p->mapreduce(ns,s_mapf,s_reducef,query);
				lyramilk::data::var ret;
				bson2var(bsonret,ret);
				return ret;
			}
			return lyramilk::data::var::nil;
		}

	  public:
		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["query"] = lyramilk::script::engine::functional<mymongo_collection,&mymongo_collection::query>;
			fn["add"] = lyramilk::script::engine::functional<mymongo_collection,&mymongo_collection::insert>;
			fn["adds"] = lyramilk::script::engine::functional<mymongo_collection,&mymongo_collection::insert_many>;
			fn["mapreduce"] = lyramilk::script::engine::functional<mymongo_collection,&mymongo_collection::mapreduce>;
			p->define("Mongo.Collection",fn,mymongo_collection::ctr,mymongo_collection::dtr);
			return 1;
		}
	};

	class mymongo_iterator
	{
		lyramilk::log::logss log;
		::mongo::DBClientConnection* p;
		std::auto_ptr< ::mongo::DBClientCursor > cursor;
		std::string ns;
		::mongo::Query mq;
		std::string dbname;

		lyramilk::data::var currdata;
		bool bof;

		static void* ctr(const lyramilk::data::array& args)
		{
			const lyramilk::data::var& v3 = args[3];
			if(v3.type() == lyramilk::data::var::t_user){
				return new mymongo_iterator((::mongo::DBClientConnection*)args[0].userdata("mongo"),args[1].str(),args[2].str(),(::mongo::Query*)v3.userdata("query"));
			}
			return new mymongo_iterator((::mongo::DBClientConnection*)args[0].userdata("mongo"),args[1].str(),args[2].str(),v3.str());
		}
		static void dtr(void* p)
		{
			delete (mymongo_iterator*)p;
		}

		mymongo_iterator(::mongo::DBClientConnection* dbptr,lyramilk::data::string dbname,lyramilk::data::string ns,lyramilk::data::string querycond):log(lyramilk::klog,"teapoy.native.mongo.iterator"),mq(std::string(querycond.c_str(),querycond.size()))
		{
			p = dbptr;
			this->ns.assign(ns.c_str(),ns.size());
			this->dbname.assign(dbname.c_str(),dbname.size());
			bof = true;
		}

		mymongo_iterator(::mongo::DBClientConnection* dbptr,lyramilk::data::string dbname,lyramilk::data::string ns,::mongo::Query* qptr):log(lyramilk::klog,"teapoy.native.mongo.iterator"),mq(*qptr)
		{
			p = dbptr;
			this->ns.assign(ns.c_str(),ns.size());
			this->dbname.assign(dbname.c_str(),dbname.size());
			bof = true;
		}

		virtual ~mymongo_iterator()
		{}

		bool init()
		{
			bof = false;
			cursor = p->query(std::string(ns.c_str(),ns.size()),mq);
			if(!cursor.get()){
				throw lyramilk::exception(D("mongodb获取游标失败"));
			}
			if(cursor->more()){
				currdata.clear();
				::mongo::BSONObj bsonresult = cursor->nextSafe();
				return bson2var(bsonresult,currdata);
			}
			return false;
		}

		lyramilk::data::var query_where(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string wherestr = args[0].str();
			if(args.size() > 1){
				MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_map);
				::mongo::BSONObj scope;
				var2bson(args[1],scope);

				::mongo::Query myquery = mq.where(std::string(wherestr.c_str(),wherestr.size()),scope);

				lyramilk::script::engine* e = (lyramilk::script::engine*)env.find(lyramilk::script::engine::s_env_engine())->second.userdata(lyramilk::script::engine::s_env_engine());
				lyramilk::data::array ar;
				ar.reserve(3);
				ar.push_back(lyramilk::data::var("mongo",p));
				ar.push_back(lyramilk::data::string(dbname.c_str(),dbname.size()));
				ar.push_back(lyramilk::data::string(ns.c_str(),ns.size()));
				ar.push_back(lyramilk::data::var("query",&myquery));
				return e->createobject("Mongo.Iterator",ar);
			}
			::mongo::Query myquery = mq.where(std::string(wherestr.c_str(),wherestr.size()));

			lyramilk::script::engine* e = (lyramilk::script::engine*)env.find(lyramilk::script::engine::s_env_engine())->second.userdata(lyramilk::script::engine::s_env_engine());
			lyramilk::data::array ar;
			ar.reserve(3);
			ar.push_back(lyramilk::data::var("mongo",p));
			ar.push_back(lyramilk::data::string(dbname.c_str(),dbname.size()));
			ar.push_back(lyramilk::data::string(ns.c_str(),ns.size()));
			ar.push_back(lyramilk::data::var("query",&myquery));
			return e->createobject("Mongo.Iterator",ar);
		}

		lyramilk::data::var sort(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_map);
			::mongo::BSONObj sortjson;
			var2bson(args[0],sortjson);
			::mongo::Query myquery = mq.sort(sortjson);

			lyramilk::script::engine* e = (lyramilk::script::engine*)env.find(lyramilk::script::engine::s_env_engine())->second.userdata(lyramilk::script::engine::s_env_engine());
			lyramilk::data::array ar;
			ar.reserve(3);
			ar.push_back(lyramilk::data::var("mongo",p));
			ar.push_back(lyramilk::data::string(dbname.c_str(),dbname.size()));
			ar.push_back(lyramilk::data::string(ns.c_str(),ns.size()));
			ar.push_back(lyramilk::data::var("query",&myquery));
			return e->createobject("Mongo.Iterator",ar);
		}

		lyramilk::data::var update(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_map);
			::mongo::BSONObj newdatabson;
			var2bson(args[0],newdatabson);
			p->update(ns,mq,newdatabson);
			std::string str = p->getLastError(dbname);
			if(!str.empty()){
				throw lyramilk::exception(lyramilk::data::string(str.c_str(),str.size()));
			}
			return true;
		}

		lyramilk::data::var remove(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			p->remove(ns,mq);
			std::string str = p->getLastError(dbname);
			if(!str.empty()){
				throw lyramilk::exception(lyramilk::data::string(str.c_str(),str.size()));
			}
			return true;
		}


		lyramilk::data::var size(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			unsigned long long sz = p->count(ns,mq.getFilter());
			if(sz == 0){
				std::string str = p->getLastError(dbname);
				if(!str.empty()){
					throw lyramilk::exception(lyramilk::data::string(str.c_str(),str.size()));
				}
			}
			return sz;
		}

		lyramilk::data::var ok(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			if(bof && init()){
				bof = false;
				next(args,env);
				return true;
			}
			return cursor->more();
		}

		lyramilk::data::var next(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			if(bof) init();
			if(cursor->more()){
				currdata.clear();
				::mongo::BSONObj bsonresult = cursor->nextSafe();
				return bson2var(bsonresult,currdata);
			}
			return false;
		}

		lyramilk::data::var to_object(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			return currdata;
		}

		lyramilk::data::var value(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			return currdata.path(args[0].str());
		}

	  public:
		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["where"] = lyramilk::script::engine::functional<mymongo_iterator,&mymongo_iterator::query_where>;
			fn["sort"] = lyramilk::script::engine::functional<mymongo_iterator,&mymongo_iterator::sort>;
			fn["update"] = lyramilk::script::engine::functional<mymongo_iterator,&mymongo_iterator::update>;
			fn["remove"] = lyramilk::script::engine::functional<mymongo_iterator,&mymongo_iterator::remove>;
			fn["size"] = lyramilk::script::engine::functional<mymongo_iterator,&mymongo_iterator::size>;

			fn["ok"] = lyramilk::script::engine::functional<mymongo_iterator,&mymongo_iterator::ok>;
			fn["next"] = lyramilk::script::engine::functional<mymongo_iterator,&mymongo_iterator::next>;
			fn["to_object"] = lyramilk::script::engine::functional<mymongo_iterator,&mymongo_iterator::to_object>;
			fn["value"] = lyramilk::script::engine::functional<mymongo_iterator,&mymongo_iterator::value>;

			p->define("Mongo.Iterator",fn,mymongo_iterator::ctr,mymongo_iterator::dtr);
			return 1;
		}
	};

	static int define(lyramilk::script::engine* p)
	{
		int i = 0;
		i+= mymongo::define(p);
		i+= mymongo_db::define(p);
		i+= mymongo_collection::define(p);
		i+= mymongo_iterator::define(p);
		return i;
	}

	static __attribute__ ((constructor)) void __init()
	{
#ifdef Z_HAVE_MONGODB
		::mongo::client::initialize();
#endif
		lyramilk::teapoy::script2native::instance()->regist("mongodb",define);
	}
}}}
