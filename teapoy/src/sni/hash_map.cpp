#include "script.h"
#include "stringutil.h"
#include <libmilk/var.h>
#include <libmilk/log.h>
#include <libmilk/multilanguage.h>
#include <leveldb/db.h>
#include <leveldb/env.h>
#include <leveldb/iterator.h>
#include <leveldb/cache.h>
#include <leveldb/filter_policy.h>
#include <stdlib.h>

namespace lyramilk{ namespace teapoy{ namespace native{
	class leveldb_iterator
	{
		lyramilk::log::logss log;
		leveldb::Iterator* it;
	  public:
		static void* ctr(lyramilk::data::var::array args)
		{
			assert(args.size() > 0);
			const void* p = args[0].userdata("iterator");
			assert(p);
			return new leveldb_iterator((leveldb::Iterator*)p);
		}
		static void dtr(void* p)
		{
			delete (leveldb_iterator*)p;
		}

		leveldb_iterator(leveldb::Iterator* p):log(lyramilk::klog,"teapoy.native.leveldb.iterator")
		{
			it = p;
		}

		~leveldb_iterator()
		{
			if(it)delete it;
		}

		lyramilk::data::var Valid(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			return it && it->Valid();
		}

		lyramilk::data::var SeekToFirst(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			if(it){
				it->SeekToFirst();
				return it->status().ok();
			}
			return false;
		}

		lyramilk::data::var SeekToLast(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			if(it){
				it->SeekToLast();
				return it->status().ok();
			}
			return false;
		}

		lyramilk::data::var Seek(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string target = args[0];
			leveldb::Slice starget(target.c_str(),target.size());
			if(it){
				it->Seek(starget);
				return it->status().ok();
			}
			return false;
		}

		lyramilk::data::var Next(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			if(it){
				it->Next();
				return it->status().ok();
			}
			return false;
		}

		lyramilk::data::var Prev(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			if(it){
				it->Prev();
				return it->status().ok();
			}
			return false;
		}

		lyramilk::data::var key(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			if(it){
				leveldb::Slice str = it->key();
				if(!it->status().ok()){
					return lyramilk::data::var::nil;
				}
				return lyramilk::data::string(str.data(),str.size());
			}
			return lyramilk::data::var::nil;
		}

		lyramilk::data::var value(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			if(it){
				leveldb::Slice str = it->value();
				if(!it->status().ok()){
					return lyramilk::data::var::nil;
				}
				return lyramilk::data::string(str.data(),str.size());
			}
			return lyramilk::data::var::nil;
		}
		
		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["Valid"] = lyramilk::script::engine::functional<leveldb_iterator,&leveldb_iterator::Valid>;
			fn["SeekToFirst"] = lyramilk::script::engine::functional<leveldb_iterator,&leveldb_iterator::SeekToFirst>;
			fn["SeekToLast"] = lyramilk::script::engine::functional<leveldb_iterator,&leveldb_iterator::SeekToLast>;
			fn["Seek"] = lyramilk::script::engine::functional<leveldb_iterator,&leveldb_iterator::Seek>;
			fn["Next"] = lyramilk::script::engine::functional<leveldb_iterator,&leveldb_iterator::Next>;
			fn["Prev"] = lyramilk::script::engine::functional<leveldb_iterator,&leveldb_iterator::Prev>;
			fn["key"] = lyramilk::script::engine::functional<leveldb_iterator,&leveldb_iterator::key>;
			fn["value"] = lyramilk::script::engine::functional<leveldb_iterator,&leveldb_iterator::value>;
			p->define("leveldb.iterator",fn,leveldb_iterator::ctr,leveldb_iterator::dtr);
			return 1;
		}
	};

	class leveldb
	{
		lyramilk::log::logss log;
		::leveldb::DB* db;
	  public:
		static void* ctr(lyramilk::data::var::array args)
		{
			assert(args.size() > 0);
			lyramilk::data::string filename = args[0];
			system(("mkdir -p " + filename).c_str());
			::leveldb::DB* db = instance_of(filename);
			if(!db){
				lyramilk::klog(lyramilk::log::warning,"teapoy.native.leveldb.ctr") << D("启动leveldb[%s]失败",filename.c_str()) << std::endl;
				return nullptr;
			}
			leveldb* pldb = new leveldb(filename);
			pldb->db = db;
			return pldb;
		}
		static void dtr(void* p)
		{
			delete (leveldb*)p;
		}

		static ::leveldb::DB* instance_of(lyramilk::data::string filename)
		{
			static lyramilk::data::map<lyramilk::data::string,::leveldb::DB*> m;
			static lyramilk::threading::mutex_os level_lock;
			lyramilk::threading::mutex_sync s(level_lock);
			lyramilk::data::map<lyramilk::data::string,::leveldb::DB*>::iterator it = m.find(filename);
			if(it == m.end()){
				int cache_size = 500;		//MB	16
				int block_size = 32;		//KB	16
				int write_buffer_size = 64;	//MB	16
				int max_open_files = cache_size / 1024 * 300;
				if(max_open_files < 500){
					max_open_files = 500;
				}
				if(max_open_files > 1000){
					max_open_files = 1000;
				}

				::leveldb::Options opt;
				opt.create_if_missing = true;
				opt.max_open_files = max_open_files;
				opt.filter_policy = ::leveldb::NewBloomFilterPolicy(10);
				opt.block_cache = ::leveldb::NewLRUCache(cache_size * 1024 * 1024);
				opt.block_size = block_size * 1024;
				opt.write_buffer_size = write_buffer_size * 1024 * 1024;
				opt.compression = ::leveldb::kSnappyCompression;
				::leveldb::DB*& db = m[filename];
				::leveldb::Status status = ::leveldb::DB::Open(opt,filename.c_str(),&db);
				if(!status.ok()){
					m.erase(filename);
					return nullptr;
				}
				return db;
			}
			return it->second;
		}

		leveldb(lyramilk::data::string filename):log(lyramilk::klog,"teapoy.native.leveldb")
		{
		}

		~leveldb()
		{
		}

		lyramilk::data::var Put(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_str);
			lyramilk::data::string key = args[0];
			lyramilk::data::string value = args[1];
			::leveldb::WriteOptions wopt;
			::leveldb::Status status = db->Put(wopt,::leveldb::Slice(key.c_str(),key.size()),::leveldb::Slice(value.c_str(),value.size()));
			return status.ok();
		}

		lyramilk::data::var Get(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string key = args[0];
			::std::string svalue;
			::leveldb::ReadOptions ropt;
			::leveldb::Status status = db->Get(ropt,::leveldb::Slice(key.c_str(),key.size()),&svalue);
			if(status.ok()){
				return lyramilk::data::string(svalue.c_str(),svalue.size());
			}
			return lyramilk::data::var::nil;
		}

		lyramilk::data::var Delete(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string key = args[0];
			::leveldb::WriteOptions wopt;
			::leveldb::Status status = db->Delete(wopt,::leveldb::Slice(key.c_str(),key.size()));
			return status.ok();
		}

		void string_inc(lyramilk::data::string& str)
		{
			if(str.empty()){
				str = "1";
			}else{
				int i = str.size() - 1;
				int cr = 1;
				for(;i>=0;--i){
					char c = str[i] + cr;
					if(c != ('9' + 1)){
						cr = 0;
						str[i] = c;
						break;
					}
					str[i] = '0';
				}
				if(cr){
					str.insert(str.begin(),'1');
				}
			}
		}

		lyramilk::data::var newid(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			::leveldb::ReadOptions ropt;
			::leveldb::WriteOptions wopt;
			std::string value;

			lyramilk::data::string ret;
			::leveldb::Slice skey("_\1autoincream\2_");

			static lyramilk::threading::mutex_os level_lock;
			lyramilk::threading::mutex_sync s(level_lock);

			::leveldb::Status status = db->Get(ropt,skey,&value);
			lyramilk::data::string myval(value.c_str(),value.size());
			string_inc(myval);
			::leveldb::Slice svalue(myval.c_str(),myval.size());
			db->Put(wopt,skey,svalue);
			return myval;
		}

		lyramilk::data::var NewIterator(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			::leveldb::ReadOptions ropt;
			::leveldb::Iterator* it = db->NewIterator(ropt);
			lyramilk::script::engine* e = (lyramilk::script::engine*)env.find(lyramilk::script::engine::s_env_engine())->second.userdata(lyramilk::script::engine::s_user_engineptr());
			lyramilk::data::var::array ar;
			ar.push_back(lyramilk::data::var("iterator",it));
			return e->createobject("leveldb.iterator",ar);
		}

		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["set"] = lyramilk::script::engine::functional<leveldb,&leveldb::Put>;
			fn["get"] = lyramilk::script::engine::functional<leveldb,&leveldb::Get>;
			fn["remove"] = lyramilk::script::engine::functional<leveldb,&leveldb::Delete>;
			fn["newid"] = lyramilk::script::engine::functional<leveldb,&leveldb::newid>;
			fn["scan"] = lyramilk::script::engine::functional<leveldb,&leveldb::NewIterator>;
			p->define("leveldb",fn,leveldb::ctr,leveldb::dtr);
			return 1;
		}
	};

	class memmap
	{
		lyramilk::log::logss log;
		typedef lyramilk::data::unordered_map<lyramilk::data::string,lyramilk::data::string> map_type;
		map_type m;
		lyramilk::threading::mutex_rw lockrw;
	  public:
		static void* ctr(lyramilk::data::var::array args)
		{
			return new memmap();
		}
		static void dtr(void* p)
		{
			delete (memmap*)p;
		}

		memmap():log(lyramilk::klog,"teapoy.native.memmap")
		{
			TODO();
		}

		~memmap()
		{
		}

		lyramilk::data::var Put(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_str);
			lyramilk::data::string key = args[0];
			lyramilk::data::string value = args[1];

			lyramilk::threading::mutex_sync s(lockrw.w());
			m[key] = value;
			return true;
		}

		lyramilk::data::var Get(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string key = args[0];

			lyramilk::threading::mutex_sync s(lockrw.r());
			map_type::iterator it = m.find(key);
			if(it==m.end()) return lyramilk::data::var::nil;
			return it->second;
		}

		lyramilk::data::var Delete(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string key = args[0];

			lyramilk::threading::mutex_sync s(lockrw.w());
			m.erase(key);
			return true;
		}

		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["set"] = lyramilk::script::engine::functional<memmap,&memmap::Put>;
			fn["get"] = lyramilk::script::engine::functional<memmap,&memmap::Get>;
			fn["remove"] = lyramilk::script::engine::functional<memmap,&memmap::Delete>;
			p->define("memmap",fn,memmap::ctr,memmap::dtr);
			return 1;
		}
	};
	static int define(lyramilk::script::engine* p)
	{
		int i = 0;
		i+= leveldb::define(p);
		i+= leveldb_iterator::define(p);
		i+= memmap::define(p);
		return i;
	}


	static __attribute__ ((constructor)) void __init()
	{
		lyramilk::teapoy::script2native::instance()->regist("hashmap",define);
	}
}}}
