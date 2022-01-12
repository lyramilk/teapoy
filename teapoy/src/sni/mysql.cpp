#include "script.h"
#include "env.h"
#include "functional_impl_mysql.h"

#include <libmilk/log.h>
#include <libmilk/dict.h>
#include <libmilk/factory.h>
#include <mysql/mysql.h>
#define MAROC_MYSQL MYSQL
#include <cassert>
#ifndef my_bool
	#define my_bool char
#endif

namespace lyramilk{ namespace teapoy{ namespace native{
	static lyramilk::log::logss log(lyramilk::klog,"teapoy.native.mysql");



	struct smysql_stmt_param_holder
	{
		MYSQL_STMT* p;
		MYSQL* _db_ptr;
		std::vector<std::vector<char> > param_buff;
		std::vector<MYSQL_BIND> param_bind;
		smysql_stmt_param_holder(MYSQL* _db_ptr)
		{
			this->_db_ptr = _db_ptr;
			p = mysql_stmt_init(_db_ptr);
		}
		~smysql_stmt_param_holder()
		{
			mysql_stmt_close(p);
		}

		bool execute(const lyramilk::data::string& sql,const lyramilk::data::array& args,lyramilk::log::logss& log)
		{
			if(!p){
				log(lyramilk::log::error,__FUNCTION__) << D("数据库错误：%s",mysql_error(_db_ptr)) << std::endl;
				throw lyramilk::exception(D("数据库错误：%s",mysql_error(_db_ptr)));
			}
			int mysqlret = mysql_stmt_prepare(p,sql.c_str(),sql.length());
			if(mysqlret != 0){
				log(lyramilk::log::error,__FUNCTION__) << D("数据库错误：%s",mysql_error(_db_ptr)) << std::endl;
				throw lyramilk::exception(D("数据库错误：%s",mysql_error(_db_ptr)));
			}
			int param_count = mysql_stmt_param_count(p);
			param_bind.resize(param_count);
			param_buff.resize(param_count);

			lyramilk::data::array::const_iterator it = args.begin();
			if(it != args.end()) ++it;
			for(int i=0;it != args.end() && i < param_bind.size();++it,++i){
				lyramilk::data::string sv = it->str();
				param_bind[i].buffer_type = MYSQL_TYPE_STRING;
				param_bind[i].buffer_length = (unsigned long)sv.length();
				param_buff[i].assign(sv.begin(),sv.end());

				param_bind[i].buffer = param_buff[i].data();
			}
			mysqlret = mysql_stmt_bind_param(p,param_bind.data());
			if(mysqlret != 0){
				log(lyramilk::log::error,__FUNCTION__) << D("数据库错误：%s",mysql_error(_db_ptr)) << std::endl;
				throw lyramilk::exception(D("数据库错误：%s",mysql_error(_db_ptr)));
			}
			mysqlret = mysql_stmt_execute(p);
			if(mysqlret != 0){
				log(lyramilk::log::error,__FUNCTION__) << D("数据库错误：%s",mysql_error(_db_ptr)) << std::endl;
				throw lyramilk::exception(D("数据库错误：%s",mysql_error(_db_ptr)));
			}
			return true;
		}
	};


	struct smysql_stmt_result_holder
	{
		MYSQL_STMT* p;
		MYSQL_RES* res;
		MYSQL* _db_ptr;

		std::vector<std::vector<char> > param_buff;
		std::vector<MYSQL_BIND> param_bind;

		std::vector<std::vector<char> > result_buff;
		std::vector<MYSQL_BIND> result_bind;
		std::vector<my_bool> result_is_null;
		std::vector<unsigned long > result_length;

		lyramilk::data::strings keys2;
		std::map<lyramilk::data::string,unsigned int> keys;

		smysql_stmt_result_holder(MYSQL* _db_ptr)
		{
			this->_db_ptr = _db_ptr;
			p = mysql_stmt_init(_db_ptr);
			res = nullptr;
		}
		~smysql_stmt_result_holder()
		{
			if(res){
				mysql_free_result(res);
			}
			mysql_stmt_free_result(p);

		}
		bool execute(const lyramilk::data::string& sql,const lyramilk::data::array& args,lyramilk::log::logss& log)
		{
			if(!p){
				log(lyramilk::log::error,"query") << D("数据库错误：%s",mysql_error(_db_ptr)) << std::endl;
				throw lyramilk::exception(D("数据库错误：%s",mysql_error(_db_ptr)));
			}
			int mysqlret = mysql_stmt_prepare(p,sql.c_str(),sql.length());
			if(mysqlret != 0){
				log(lyramilk::log::error,"query.mysql_stmt_prepare") << D("数据库错误：%s",mysql_error(_db_ptr)) << std::endl;
				throw lyramilk::exception(D("数据库错误：%s",mysql_error(_db_ptr)));
			}

			// 绑定参数
			{
				int param_count = mysql_stmt_param_count(p);
				param_bind.resize(param_count);
				param_buff.resize(param_count);

				lyramilk::data::array::const_iterator it = args.begin();
				if(it != args.end()) ++it;
				for(int i=0;it != args.end() && i < param_bind.size();++it,++i){
					lyramilk::data::string sv = it->str();
					param_bind[i].buffer_type = MYSQL_TYPE_STRING;
					param_bind[i].buffer_length = (unsigned long)sv.length();

					param_buff[i].assign(sv.begin(),sv.end());

					param_bind[i].buffer = param_buff[i].data();
				}
				mysqlret = mysql_stmt_bind_param(p,param_bind.data());
				if(mysqlret != 0){
					log(lyramilk::log::error,"query.mysql_stmt_bind_param") << D("数据库错误：%s",mysql_error(_db_ptr)) << std::endl;
					throw lyramilk::exception(D("数据库错误：%s",mysql_error(_db_ptr)));
				}
			}


			// 绑定结果
			{

				res = mysql_stmt_result_metadata(p);

				int result_fieid_count = mysql_stmt_field_count(p);
				result_bind.resize(result_fieid_count);
				result_is_null.resize(result_fieid_count);
				result_buff.resize(result_fieid_count);
				keys2.reserve(result_fieid_count);
				result_length.resize(result_fieid_count);

				for(int i=0;i<result_fieid_count;++i){
					MYSQL_FIELD* field = mysql_fetch_field(res);


					if(field == nullptr || field->name == nullptr){
						break;
					}

					lyramilk::data::string fname;
					if(field && field->name){
						fname = field->name;
					}

					lyramilk::data::string tmp;
					
					for(int tmp_i = 1;tmp_i < 4096;++tmp_i){
						std::pair<std::map<lyramilk::data::string,unsigned int>::const_iterator,bool> it = keys.insert(std::map<lyramilk::data::string,unsigned int>::value_type(fname + tmp,i));
						if(it.second){
							keys[fname + tmp] = i;
							break;
						}

						char buf[256];
						int r = snprintf(buf,sizeof(buf),"%u",tmp_i);
						tmp.assign(buf,r);
					}

					keys2.push_back(fname + tmp);

					if(field->max_length > 0){
						result_bind[i].buffer_length = field->max_length;
					}else{
						result_bind[i].buffer_length = 65536;
					}
					result_buff[i].resize(result_bind[i].buffer_length);

					result_bind[i].buffer_type = MYSQL_TYPE_STRING;
					result_bind[i].length = &result_length[i];

					result_bind[i].buffer = result_buff[i].data();

#ifdef MARIADB_BASE_VERSION
					result_bind[i].is_null = (my_bool*)&result_is_null[i];
#else
					result_bind[i].is_null = (bool*)&result_is_null[i];
#endif
				}

				mysqlret = mysql_stmt_bind_result(p,result_bind.data());
				if(mysqlret != 0){
					log(lyramilk::log::error,"query.mysql_stmt_bind_result") << D("数据库错误：%s",mysql_error(_db_ptr)) << std::endl;
					throw lyramilk::exception(D("数据库错误：%s",mysql_error(_db_ptr)));
				}
			}
			mysqlret = mysql_stmt_execute(p);
			if(mysqlret != 0){
				log(lyramilk::log::error,"query.mysql_stmt_execute") << D("数据库错误：%s",mysql_error(_db_ptr)) << std::endl;
				throw lyramilk::exception(D("数据库错误：%s",mysql_error(_db_ptr)));
			}


			mysqlret = mysql_stmt_store_result(p);
			if(mysqlret != 0){
				log(lyramilk::log::error,"query.mysql_stmt_store_result") << D("数据库错误：%s",mysql_error(_db_ptr)) << std::endl;
				throw lyramilk::exception(D("数据库错误：%s",mysql_error(_db_ptr)));
			}
			return true;
		}
	};


	class mysqldb_datawrapper:public lyramilk::data::datawrapper
	{

	  public:
		MYSQL* _db_ptr;
	  public:
		mysqldb_datawrapper(MYSQL* __db_ptr):_db_ptr(__db_ptr)
		{}

	  	virtual ~mysqldb_datawrapper()
		{}

		static lyramilk::data::string class_name()
		{
			return "lyramilk.teapoy.mysqldb";
		}

		virtual lyramilk::data::string name() const
		{
			return class_name();
		}

		virtual lyramilk::data::datawrapper* clone() const
		{
			return new mysqldb_datawrapper(_db_ptr);
		}

		virtual void destory()
		{
			delete this;
		}

		virtual bool type_like(lyramilk::data::var::vt nt) const
		{
			return false;
		}
	};


	class smysql_iterator:public lyramilk::script::sclass
	{
		smysql_stmt_result_holder* holder;
		lyramilk::data::string sql;
		lyramilk::data::array sqlargs;
	  public:
		static lyramilk::script::sclass* ctr(const lyramilk::data::array& args)
		{
		  	if(args.size() < 3) throw lyramilk::exception(D("数据库错误：%s","mysql查询迭代器未初始化"));
			if(args[0].type() != lyramilk::data::var::t_user) throw lyramilk::exception(D("数据库错误：%s","mysql查询迭代器未初始化"));
			if(!args[1].type_like(lyramilk::data::var::t_str)) throw lyramilk::exception(D("数据库错误：%s","mysql查询迭代器未初始化"));
			if(args[2].type() != lyramilk::data::var::t_array) throw lyramilk::exception(D("数据库错误：%s","mysql查询迭代器未初始化"));



			MYSQL* _db_ptr = nullptr;
			if(args.size() > 0 && args[0].type() == lyramilk::data::var::t_user){
				lyramilk::data::datawrapper* urd = args[0].userdata();
				if(urd && urd->name() == mysqldb_datawrapper::class_name()){
					mysqldb_datawrapper* urdp = (mysqldb_datawrapper*)urd;

					_db_ptr = urdp->_db_ptr;
				}
			}
			if(_db_ptr == nullptr)  throw lyramilk::exception(D("数据库错误：%s","mysql查询迭代器未初始化"));


			smysql_stmt_result_holder* h = new smysql_stmt_result_holder(_db_ptr);
			if(h == nullptr) throw lyramilk::exception(D("数据库错误：%s","mysql查询迭代器未初始化"));

			const lyramilk::data::string& sql = args[1];
			const lyramilk::data::array& sqlargs = args[2];

			if(h->execute(sql,sqlargs,log)){
				smysql_iterator* r = new smysql_iterator();
				if(r == nullptr){
					delete h;
					throw lyramilk::exception(D("数据库错误：%s","mysql查询迭代器未初始化"));
				}
				r->holder = h;
				r->sql = sql;
				r->sqlargs = sqlargs;

				return r;
			}
			delete h;
			return nullptr;
		}
		static void dtr(lyramilk::script::sclass* p)
		{
			delete (smysql_iterator*)p;
		}

		smysql_iterator()
		{
			holder = nullptr;
		}

		virtual ~smysql_iterator()
		{
			if(holder){
				delete holder;
			}
		}


	  	virtual bool iterator_begin()
		{
			mysql_stmt_data_seek(holder->p,0);
			return true;
		}

	  	virtual bool iterator_next(std::size_t idx,lyramilk::data::var* v)
		{
			int mysqlret = mysql_stmt_fetch(holder->p);
			if(mysqlret != 0){
				if(MYSQL_NO_DATA != mysqlret){
					log(lyramilk::log::error,"query.iterator_next") << D("数据库错误：[%d]%s",mysqlret,mysql_error(holder->_db_ptr)) << std::endl;
				}
				return false;
			}

			v->type(lyramilk::data::var::t_map);
			lyramilk::data::map& m = *v;

			for(std::size_t i = 0;i<holder->keys2.size();i++){
				if(*holder->result_bind[i].is_null){
					m[holder->keys2[i]].clear();
				}else{
					m[holder->keys2[i]] = lyramilk::data::string(holder->result_buff[i].data(),holder->result_length[i]);
				}
			}
			return true;
		}

	  	virtual void iterator_end()
		{
		}

		lyramilk::data::var init(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			mysql_stmt_data_seek(holder->p,0);
			return true;
		}

		lyramilk::data::var key(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_uint);
			lyramilk::data::uint32 idx = args[0];
			if(idx >= holder->keys2.size()) throw lyramilk::exception(D("索引数值过大：%u(表宽为%u)",idx,holder->keys2.size()));
			return holder->keys2[idx];
		}

		lyramilk::data::var value(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_uint);
			lyramilk::data::uint32 idx = args[0];
			if(idx >= holder->keys2.size()) throw lyramilk::exception(D("索引数值过大：%u(表宽为%u)",idx,holder->keys2.size()));
			return lyramilk::data::string(holder->result_buff[idx].data(),holder->result_length[idx]);
		}

		lyramilk::data::var next(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			int mysqlret = mysql_stmt_fetch(holder->p);
			if(mysqlret != 0){
				return false;
			}
			return true;
		}

		lyramilk::data::var size(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			return holder->res->row_count;
		}

		lyramilk::data::var columncount(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			return holder->keys2.size();
		}

		lyramilk::data::var seek(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_int);
			mysql_stmt_data_seek(holder->p,args[0]);
			int mysqlret = mysql_stmt_fetch(holder->p);
			if(mysqlret != 0){
				return false;
			}
			return true;
		}

		lyramilk::data::var to_object(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			lyramilk::data::var v;
			v.type(lyramilk::data::var::t_map);
			lyramilk::data::map& m = v;

			for(std::size_t i = 0;i<holder->keys2.size();i++){
				m[holder->keys2[i]] = lyramilk::data::string(holder->result_buff[i].data(),holder->result_length[i]);
			}

			return v;
		}

		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["key"] = lyramilk::script::engine::functional<smysql_iterator,&smysql_iterator::key>;
			fn["value"] = lyramilk::script::engine::functional<smysql_iterator,&smysql_iterator::value>;
			fn["next"] = lyramilk::script::engine::functional<smysql_iterator,&smysql_iterator::next>;
			fn["init"] = lyramilk::script::engine::functional<smysql_iterator,&smysql_iterator::init>;
			fn["size"] = lyramilk::script::engine::functional<smysql_iterator,&smysql_iterator::size>;
			fn["columncount"] = lyramilk::script::engine::functional<smysql_iterator,&smysql_iterator::columncount>;
			fn["seek"] = lyramilk::script::engine::functional<smysql_iterator,&smysql_iterator::seek>;
			fn["to_object"] = lyramilk::script::engine::functional<smysql_iterator,&smysql_iterator::to_object>;
			p->define("mysql.iterator",fn,smysql_iterator::ctr,smysql_iterator::dtr);
			return 1;
		}
	};

	class smysql:public lyramilk::script::sclass
	{
		lyramilk::log::logss log;
		MYSQL* _db_ptr;
		lyramilk::data::string host;
		unsigned short port;
		lyramilk::data::string db;
		lyramilk::data::string user;
		lyramilk::data::string passwd;

		functional_impl_mysql_instance::ptr pp;
	  public:
		static lyramilk::script::sclass* ctr(const lyramilk::data::array& args)
		{

			lyramilk::teapoy::functional_multi* f = functional_master::instance()->get(args[0].str());
			if(f){
				functional::ptr p = f->get_instance();
				if(p){
					smysql* ins = new smysql();
					if(ins){
						ins->pp = p;
						ins->_db_ptr = p.as<functional_impl_mysql_instance>()->_db_ptr;
						return ins;
					}
				}
			}
			return nullptr;
		}
		static void dtr(lyramilk::script::sclass* p)
		{
			delete (smysql*)p;
		}

		smysql():log(lyramilk::klog,"teapoy.native.mysql")
		{
			pp = nullptr;
		}

		virtual ~smysql()
		{
		}

		lyramilk::data::var ok(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			return 0 == mysql_ping(_db_ptr);
		}

		lyramilk::data::var query(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string sql = args[0];

			lyramilk::data::map::const_iterator it_env_eng = env.find(lyramilk::script::engine::s_env_engine());
			if(it_env_eng != env.end()){
				lyramilk::data::datawrapper* urd = it_env_eng->second.userdata();
				if(urd && urd->name() == lyramilk::script::engine_datawrapper::class_name()){
					lyramilk::script::engine_datawrapper* urdp = (lyramilk::script::engine_datawrapper*)urd;
					if(urdp->eng){
						lyramilk::data::array ar;
						ar.push_back(mysqldb_datawrapper(_db_ptr));
						ar.push_back(sql);
						ar.push_back(args);
						return urdp->eng->createobject("mysql.iterator",ar);
					}
				}
			}
			return lyramilk::data::var::nil;
		}


		lyramilk::data::var exec(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string sql = args[0];

			smysql_stmt_param_holder stmt(_db_ptr);
			if(!stmt.execute(sql,args,log)){
				return lyramilk::data::var::nil;
			}
			if(lyramilk::teapoy::env::is_debug()){
				log(lyramilk::log::debug,__FUNCTION__) << D("执行SQL命令%s",sql.c_str()) << std::endl;
			}
			return mysql_stmt_affected_rows(stmt.p);
		}

		lyramilk::data::var insert(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string sql = args[0];

			smysql_stmt_param_holder stmt(_db_ptr);
			if(!stmt.execute(sql,args,log)){
				return lyramilk::data::var::nil;
			}
			if(lyramilk::teapoy::env::is_debug()){
				log(lyramilk::log::debug,__FUNCTION__) << D("执行SQL命令%s",sql.c_str()) << std::endl;
			}
			return mysql_stmt_insert_id(stmt.p);
		}

		static int define(lyramilk::script::engine* p)
		{
			{
				lyramilk::script::engine::functional_map fn;
				fn["ok"] = lyramilk::script::engine::functional<smysql,&smysql::ok>;
				fn["exec"] = lyramilk::script::engine::functional<smysql,&smysql::exec>;
				fn["insert"] = lyramilk::script::engine::functional<smysql,&smysql::insert>;
				fn["query"] = lyramilk::script::engine::functional<smysql,&smysql::query>;
				p->define("..invoker.instance.mysql",fn,smysql::ctr,smysql::dtr);
			}
			return 2;
		}
	};

	static int define(lyramilk::script::engine* p)
	{
		int i = 0;
		i+= smysql::define(p);
		i+= smysql_iterator::define(p);
		return i;
	}


	static __attribute__ ((constructor)) void __init()
	{
		invoker_map::instance()->define("mysql","..invoker.instance.mysql");
		lyramilk::teapoy::script_interface_master::instance()->regist("mysql",define);
	}
}}}
