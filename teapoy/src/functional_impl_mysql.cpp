#include "functional_impl_mysql.h"
#include <libmilk/log.h>
#include <libmilk/dict.h>
#include <libmilk/exception.h>
#ifndef my_bool
	#define my_bool bool
#endif

namespace lyramilk{ namespace teapoy {


	//	functional_impl_mysql_instance
	#define FUNCTIONAL_TYPE	"mysql"

	bool functional_impl_mysql_instance::init(const lyramilk::data::map& cm)
	{
		lyramilk::data::map m = cm;


		_db_ptr = mysql_init(nullptr);

		lyramilk::data::string host = m["host"].str();
		unsigned short port = m["port"].conv(3306);
		lyramilk::data::string db = m["db"].str();
		lyramilk::data::string user = m["user"].str();
		lyramilk::data::string passwd = m["password"].str();
		lyramilk::data::string confiugfile = m["default_file"].str();
		lyramilk::data::string cfggroup = m["read_default_group"].str();


		if(m["options"].type() == lyramilk::data::var::t_array){
			lyramilk::data::array& options = m["options"];

			lyramilk::data::array::iterator it = options.begin();
			for(;it!=options.end();++it){
				int ret = mysql_options(_db_ptr,MYSQL_INIT_COMMAND, it->str().c_str());
				if(ret != 0){
					lyramilk::klog(lyramilk::log::warning,"teapoy.functional_impl_mysql_instance") << D("设置 mysql 配置[%s]失败：%s",it->str().c_str(),mysql_error(_db_ptr)) << std::endl;
					return false;
				}
			
			}
		}

		if(!confiugfile.empty()){
			int ret = mysql_options(_db_ptr,MYSQL_READ_DEFAULT_FILE, confiugfile.c_str());
			if(ret == 0){
				lyramilk::klog(lyramilk::log::debug  ,"teapoy.functional_impl_mysql_instance") << D("设置 mysql 配置文件[%s]成功",confiugfile.c_str()) << std::endl;
			}else{
				lyramilk::klog(lyramilk::log::warning,"teapoy.functional_impl_mysql_instance") << D("设置 mysql 配置文件[%s]失败：%s",confiugfile.c_str(),mysql_error(_db_ptr)) << std::endl;
				return false;
			}
		}
		if(!cfggroup.empty()){
			int ret = mysql_options(_db_ptr,MYSQL_READ_DEFAULT_GROUP, cfggroup.c_str());
			if(ret == 0){
				lyramilk::klog(lyramilk::log::debug  ,"teapoy.functional_impl_mysql_instance") << D("设置 mysql 配置组[%s]成功",cfggroup.c_str()) << std::endl;
			}else{
				lyramilk::klog(lyramilk::log::warning,"teapoy.functional_impl_mysql_instance") << D("设置 mysql 配置组[%s]失败：%s",cfggroup.c_str(),mysql_error(_db_ptr)) << std::endl;
				return false;
			}
		}



		if(nullptr != mysql_real_connect(_db_ptr,(host.empty()?nullptr:host.c_str()),
												(user.empty()?nullptr:user.c_str()),
												(passwd.empty()?nullptr:passwd.c_str()),
												(db.empty()?nullptr:db.c_str()),
												port,nullptr,0)){
			my_bool my_true= true;
			mysql_options(_db_ptr, MYSQL_OPT_RECONNECT, &my_true);
			return true;
		}

		lyramilk::klog(lyramilk::log::error,"teapoy.functional_impl_mysql_instance") << D("数据库错误：%s",mysql_error(_db_ptr)) << std::endl;
		return false;
	}

	lyramilk::data::var functional_impl_mysql_instance::exec(const lyramilk::data::array& ar)
	{
		if(!_db_ptr) return lyramilk::data::var::nil;
		if(ar.empty() || !ar[0].type_like(lyramilk::data::var::t_str)){
			return lyramilk::data::var::nil;
		}
		lyramilk::data::string sql = ar[0];

		struct autorelease_stmt
		{
			MYSQL_STMT* p;
			autorelease_stmt(MYSQL* _db_ptr)
			{
				p = mysql_stmt_init(_db_ptr);
			}
			~autorelease_stmt()
			{
				mysql_stmt_close(p);
			}
		};

		autorelease_stmt stmt(_db_ptr);
		if(!stmt.p){
			throw lyramilk::exception(D("数据库错误：%s",mysql_error(_db_ptr)));
		}
		int mysqlret = mysql_stmt_prepare(stmt.p,sql.c_str(),sql.length());
		if(mysqlret != 0){
			throw lyramilk::exception(D("数据库错误：%s",mysql_error(_db_ptr)));
		}
		int param_count = mysql_stmt_param_count(stmt.p);
		std::vector<MYSQL_BIND> bind(param_count);
		std::vector<std::vector<char> > param_buff(param_count);

		lyramilk::data::array::const_iterator it = ar.begin();
		if(it != ar.end()) ++it;
		for(int i=0;it != ar.end();++it,++i){
			lyramilk::data::string sv = it->str();
			bind[i].buffer_type = MYSQL_TYPE_STRING;
			bind[i].buffer_length = (unsigned long)sv.length();

			param_buff[i].assign(sv.begin(),sv.end());

			bind[i].buffer = param_buff[i].data();
		}
		mysqlret = mysql_stmt_bind_param(stmt.p,bind.data());
		if(mysqlret != 0){
			throw lyramilk::exception(D("数据库错误：%s",mysql_error(_db_ptr)));
		}
		mysqlret = mysql_stmt_execute(stmt.p);
		if(mysqlret != 0){
			throw lyramilk::exception(D("数据库错误：%s",mysql_error(_db_ptr)));
		}
		return mysql_affected_rows(_db_ptr);
	}

	functional_impl_mysql_instance::functional_impl_mysql_instance()
	{
		_db_ptr = nullptr;
	}

	functional_impl_mysql_instance::~functional_impl_mysql_instance()
	{
		if(_db_ptr){
			mysql_close( _db_ptr );
		}
	}


	// functional_impl_mysql
	functional_impl_mysql::functional_impl_mysql()
	{}
	functional_impl_mysql::~functional_impl_mysql()
	{}

	bool functional_impl_mysql::init(const lyramilk::data::map& m)
	{
		info = m;
		return true;
	}

	functional::ptr functional_impl_mysql::get_instance()
	{
		std::list<functional_impl_mysql_instance*>::iterator it = es.begin();
		for(;it!=es.end();++it){
			if((*it)->try_lock()){
				return functional::ptr(*it);
			}
		}
		functional_impl_mysql_instance* tmp = underflow();
		if(tmp){
			functional_impl_mysql_instance* pe = nullptr;
			{
				lyramilk::threading::mutex_sync _(l);
				es.push_back(tmp);
				pe = es.back();
				pe->lock();
			}
			return functional::ptr(pe);

		}
		return functional::ptr(nullptr);
	}

	lyramilk::data::string functional_impl_mysql::name()
	{
		return FUNCTIONAL_TYPE;
	}


	functional_impl_mysql_instance* functional_impl_mysql::underflow()
	{
		functional_impl_mysql_instance* ins = new functional_impl_mysql_instance;
		ins->init(info);
		return ins;
	}


	//

	static functional_multi* ctr(void* args)
	{
		return new functional_impl_mysql;
	}

	static void dtr(functional_multi* p)
	{
		delete (functional_impl_mysql*)p;
	}

	static __attribute__ ((constructor)) void __init()
	{
		functional_type_master::instance()->define(FUNCTIONAL_TYPE,ctr,dtr);
	}
}}
