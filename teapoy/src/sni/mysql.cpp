#include "script.h"
#include "env.h"
#include <libmilk/log.h>
#include <libmilk/multilanguage.h>
#include <libmilk/factory.h>
#include <mysql/mysql.h>

namespace lyramilk{ namespace teapoy{ namespace native{


	static lyramilk::log::logss log(lyramilk::klog,"teapoy.native.mysql");


	struct mysql_client
	{
		MYSQL* _db_ptr;
		mysql_client()
		{
			_db_ptr = nullptr;
		}

		~mysql_client()
		{
			mysql_close(_db_ptr);
		}
	};

	class mysql_clients:public lyramilk::threading::exclusive::list<lyramilk::teapoy::native::mysql_client>
	{
		lyramilk::data::var cfg;
	  public:
		mysql_clients(const lyramilk::data::var& cfg)
		{
			this->cfg = cfg;
		}

		virtual ~mysql_clients()
		{
		}

		virtual lyramilk::teapoy::native::mysql_client* underflow()
		{
			lyramilk::teapoy::native::mysql_client* p = new lyramilk::teapoy::native::mysql_client();
			if(!p) return nullptr;

			p->_db_ptr = mysql_init(nullptr);
			my_bool my_true= true;
			mysql_options(p->_db_ptr, MYSQL_OPT_RECONNECT, &my_true);

			lyramilk::data::var::array& ar = cfg["opt"];
			for(lyramilk::data::var::array::iterator it = ar.begin();it!=ar.end();++it){
				lyramilk::data::string opt = it->str();
				int ret = mysql_options(p->_db_ptr,MYSQL_INIT_COMMAND, opt.c_str());
				if(ret != 0){
					log(lyramilk::log::warning,__FUNCTION__) << D("设置Mysql配置(%s)失败：%s",opt.c_str(),mysql_error(p->_db_ptr)) << std::endl;
					delete p;
					return nullptr;
				}
			}

			lyramilk::data::string cnf = cfg.path("/cnf/file");
			
			int ret = mysql_options(p->_db_ptr,MYSQL_READ_DEFAULT_FILE, cnf.c_str());
			if(ret == 0){
				log(__FUNCTION__) << D("设置Mysql配置文件[%s]成功",cnf.c_str()) << std::endl;
			}else{
				log(lyramilk::log::warning,__FUNCTION__) << D("设置Mysql配置文件[%s]失败：%s",cnf.c_str(),mysql_error(p->_db_ptr)) << std::endl;
				delete p;
				return nullptr;
			}

			lyramilk::data::var group = cfg.path("/cnf/group");
			if(group.type_like(lyramilk::data::var::t_str)){
				lyramilk::data::string strgroup = group.str();
				ret = mysql_options(p->_db_ptr,MYSQL_READ_DEFAULT_GROUP, strgroup.c_str());
				if(ret == 0){
					log(__FUNCTION__) << D("设置Mysql配置组[%s]成功",strgroup.c_str()) << std::endl;
				}else{
					log(lyramilk::log::warning,__FUNCTION__) << D("设置Mysql配置组[%s]失败：%s",strgroup.c_str(),mysql_error(p->_db_ptr)) << std::endl;
					return false;
				}
			}

			if(nullptr == mysql_real_connect(p->_db_ptr,nullptr,nullptr,nullptr,nullptr,0,nullptr,0)){
				return nullptr;
			}
			return p;
		}
	};

	class mysql_clients_multiton:public lyramilk::util::multiton_factory<mysql_clients>
	{
		lyramilk::threading::mutex_spin lock;
	  public:
		static mysql_clients_multiton* instance()
		{
			static mysql_clients_multiton _mm;
			return &_mm;
		}

		virtual mysql_clients::ptr getobj(lyramilk::data::string id,const lyramilk::data::var& cfg)
		{
			lyramilk::data::string token = id;
			mysql_clients* c = get(token);
			if(c == nullptr){
				lyramilk::threading::mutex_sync _(lock);
				define(token,new mysql_clients(cfg));
				c = get(token);
			}
			if(c == nullptr) return nullptr;
			return c->get();
		}
	};


	class smysql_iterator
	{
		lyramilk::log::logss log;
		MYSQL_RES* res;
		MYSQL* _db_ptr;
		lyramilk::data::var::array values;
		lyramilk::data::var::map keys;
		std::map<unsigned int,lyramilk::data::var> keys2;
	  public:
		static void* ctr(const lyramilk::data::var::array& args)
		{
			assert(args.size() > 0);
			return new smysql_iterator((MYSQL_RES*)args[0].userdata("mysql.res"),(MYSQL*)args[0].userdata("mysql.db"));
		}
		static void dtr(void* p)
		{
			delete (smysql_iterator*)p;
		}

		smysql_iterator(MYSQL_RES* res,MYSQL* _db_ptr):log(lyramilk::klog,"teapoy.native.mysql.iterator")
		{
			this->res = res;
			unsigned int cc = mysql_num_fields(res);
			values.reserve(cc);
			for(unsigned int i=0;i<cc;i++){
				MYSQL_FIELD* field = mysql_fetch_field(res);
				lyramilk::data::var fname;
				if(field && field->name){
					fname = field->name;
				}
				keys[fname] = i;
				keys2[i] = fname;
			}
			this->_db_ptr = _db_ptr;
		}

		virtual ~smysql_iterator()
		{
			mysql_free_result(res);
		}
		lyramilk::data::var ok(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			if(values.empty()){
				MYSQL_ROW row = mysql_fetch_row(res);
				if(row == nullptr){
					return false;
				}
				for(unsigned int i = 0;i<values.capacity();i++){
					lyramilk::data::var v;
					if(row[i] != nullptr){
						v = row[i];
					}
					values.push_back(v);
				}
			}
			return values.size() > 0;
		}

		lyramilk::data::var key(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_uint);
			lyramilk::data::uint32 u = args[0];
			std::map<unsigned int,lyramilk::data::var>::iterator it = keys2.find(u);
			if(it!=keys2.end()){
				return it->second;
			}
			return lyramilk::data::var::nil;
		}

		lyramilk::data::var value(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::var v = args[0];

			if(values.empty()){
				MYSQL_ROW row = mysql_fetch_row(res);
				if(row == nullptr){
					log(lyramilk::log::error,__FUNCTION__) << D("数据库错误：%s",mysql_error(_db_ptr)) << std::endl;
					throw lyramilk::exception(D("数据库错误：%s",mysql_error(_db_ptr)));
				}
				for(unsigned int i = 0;i<values.capacity();i++){
					lyramilk::data::var v;
					if(row[i] != nullptr){
						v = row[i];
					}
					values.push_back(v);
				}
			}

			if(v.type_like(lyramilk::data::var::t_int)){
				unsigned int index = v;
				if(index < values.size()){
					return values[index];
				}
				return lyramilk::data::var::nil;
			}else{
				lyramilk::data::var::map::iterator it = keys.find(v.str());
				if(it == keys.end()) return lyramilk::data::var::nil;
				unsigned int index = it->second;
				if(index < values.size()){
					return values[index];
				}
				return lyramilk::data::var::nil;
			}
		}

		lyramilk::data::var next(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			values.clear();
			MYSQL_ROW row = mysql_fetch_row(res);
			if(row == nullptr){
				return false;
			}
			for(unsigned int i = 0;i<values.capacity();i++){
				lyramilk::data::var v;
				if(row[i] != nullptr){
					v = row[i];
				}
				values.push_back(v);
			}
			return true;
		}

		lyramilk::data::var size(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			return res->row_count;
		}

		lyramilk::data::var columncount(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			return keys2.size();
		}

		lyramilk::data::var seek(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_int);
			mysql_data_seek(res,args[0]);
			values.clear();
			MYSQL_ROW row = mysql_fetch_row(res);
			if(row == nullptr){
				return false;
			}
			for(unsigned int i = 0;i<values.capacity();i++){
				lyramilk::data::var v;
				if(row[i] != nullptr){
					v = row[i];
				}
				values.push_back(v);
			}
			return true;
		}

		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["key"] = lyramilk::script::engine::functional<smysql_iterator,&smysql_iterator::key>;
			fn["value"] = lyramilk::script::engine::functional<smysql_iterator,&smysql_iterator::value>;
			fn["ok"] = lyramilk::script::engine::functional<smysql_iterator,&smysql_iterator::ok>;
			fn["next"] = lyramilk::script::engine::functional<smysql_iterator,&smysql_iterator::next>;
			fn["size"] = lyramilk::script::engine::functional<smysql_iterator,&smysql_iterator::size>;
			fn["columncount"] = lyramilk::script::engine::functional<smysql_iterator,&smysql_iterator::columncount>;
			fn["seek"] = lyramilk::script::engine::functional<smysql_iterator,&smysql_iterator::seek>;
			p->define("mysql.iterator",fn,smysql_iterator::ctr,smysql_iterator::dtr);
			return 1;
		}
	};

	class smysql
	{
		lyramilk::log::logss log;
		MYSQL* _db_ptr;
		lyramilk::data::string host;
		unsigned short port;
		lyramilk::data::string db;
		lyramilk::data::string user;
		lyramilk::data::string passwd;

		mysql_clients::ptr ptrdb;
	  public:
		static void* ctr(const lyramilk::data::var::array& args)
		{
			if(args.size() == 1){
				const void* p = args[0].userdata("__mysql_client");
				if(p) return new smysql((mysql_clients::ptr*)p);
			}
			return new smysql;
		}
		static void dtr(void* p)
		{
			delete (smysql*)p;
		}

		smysql():log(lyramilk::klog,"teapoy.native.mysql")
		{
			_db_ptr = mysql_init(nullptr);
			my_bool my_true= true;
			mysql_options(_db_ptr, MYSQL_OPT_RECONNECT, &my_true);
		}

		smysql(mysql_clients::ptr* ptr):log(lyramilk::klog,"teapoy.native.mysql")
		{
			ptrdb = *ptr;
			_db_ptr = ptrdb->_db_ptr;
		}

		virtual ~smysql()
		{
			if(!ptrdb){
				mysql_close( _db_ptr );
				_db_ptr = nullptr;
			}
		}

		lyramilk::data::var setopt(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			int ret = mysql_options(_db_ptr,MYSQL_INIT_COMMAND, args[0].str().c_str());
			if(ret != 0){
				log(lyramilk::log::warning,__FUNCTION__) << D("设置Mysql配置(%s)失败：%s",args[0].str().c_str(),mysql_error(_db_ptr)) << std::endl;
				return false;
			}
			return true;
		}
		lyramilk::data::var use(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string cfgfile = args[0];

			int ret = mysql_options(_db_ptr,MYSQL_READ_DEFAULT_FILE, cfgfile.c_str());
			if(ret == 0){
				log(__FUNCTION__) << D("设置Mysql配置文件[%s]成功",cfgfile.c_str()) << std::endl;
			}else{
				log(lyramilk::log::warning,__FUNCTION__) << D("设置Mysql配置文件[%s]失败：%s",cfgfile.c_str(),mysql_error(_db_ptr)) << std::endl;
				return false;
			}

			if(args.size() > 1){
				lyramilk::data::string cfggroup = args[1];
				ret = mysql_options(_db_ptr,MYSQL_READ_DEFAULT_GROUP, cfggroup.c_str());
				if(ret == 0){
					log(__FUNCTION__) << D("设置Mysql配置组[%s]成功",cfggroup.c_str()) << std::endl;
				}else{
					log(lyramilk::log::warning,__FUNCTION__) << D("设置Mysql配置组[%s]失败：%s",cfggroup.c_str(),mysql_error(_db_ptr)) << std::endl;
					return false;
				}
			}

			return true;
		}

		lyramilk::data::var open(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			lyramilk::data::string host;
			unsigned short port = 0;
			lyramilk::data::string db;
			lyramilk::data::string user;
			lyramilk::data::string passwd;


			if(args.size() == 1 && args[0].type() == lyramilk::data::var::t_map){
				const lyramilk::data::var& v = args[0];
				host = v.at("host").str();
				if(v.at("port").type() != lyramilk::data::var::t_invalid){
					port = v["port"];
				}
				db = v.at("db").str();
				user = v.at("user").str();
				passwd = v.at("password").str();
			}else{
				if(args.size() > 0){
					MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
					host = args[0].str();
				}
				if(args.size() > 1){
					MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_uint);
					port = args[1];
				}
				if(args.size() > 2){
					MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,2,lyramilk::data::var::t_str);
					db = args[2].str();
				}
				if(args.size() > 3){
					MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,3,lyramilk::data::var::t_str);
					user = args[3].str();
				}
				if(args.size() > 4){
					MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,4,lyramilk::data::var::t_str);
					passwd = args[4].str();
				}
			}
			if(nullptr != mysql_real_connect(_db_ptr,(host.empty()?nullptr:host.c_str()),
													(user.empty()?nullptr:user.c_str()),
													(passwd.empty()?nullptr:passwd.c_str()),
													(db.empty()?nullptr:db.c_str()),
													port,nullptr,0)){
				this->host = host;
				this->user = user;
				this->passwd = passwd;
				this->db = db;
				this->port = port;
				return true;
			}
			return false;
		}


		lyramilk::data::var ok(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			return 0 == mysql_ping(_db_ptr);
		}

		lyramilk::data::var query(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string sql = args[0];

			int ret = mysql_real_query( _db_ptr,sql.c_str(),(unsigned long)sql.length());
			if(ret != 0){
				log(lyramilk::log::error,__FUNCTION__) << D("数据库错误：%s",mysql_error(_db_ptr)) << std::endl;
				throw lyramilk::exception(D("数据库错误：%s",mysql_error(_db_ptr)));
			}

			MYSQL_RES* res = mysql_store_result(_db_ptr);
			
			if(res == nullptr){
				log(lyramilk::log::error,__FUNCTION__) << D("数据库错误：%s",mysql_error(_db_ptr)) << std::endl;
				throw lyramilk::exception(D("数据库错误：%s",mysql_error(_db_ptr)));
			}

			if(lyramilk::teapoy::env::is_debug()){
				log(lyramilk::log::debug,__FUNCTION__) << D("执行查询%s",sql.c_str()) << std::endl;
			}

			lyramilk::data::var ud("mysql.res",res);
			ud.userdata("mysql.db",_db_ptr);

			lyramilk::data::var::array ar;
			ar.push_back(ud);
			lyramilk::script::engine* e = (lyramilk::script::engine*)env.find(lyramilk::script::engine::s_env_engine())->second.userdata(lyramilk::script::engine::s_user_engineptr());
			return e->createobject("mysql.iterator",ar);
		}

		lyramilk::data::var execute(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string sql = args[0];

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
				log(lyramilk::log::error,__FUNCTION__) << D("数据库错误：%s",mysql_error(_db_ptr)) << std::endl;
				throw lyramilk::exception(D("数据库错误：%s",mysql_error(_db_ptr)));
			}
			int mysqlret = mysql_stmt_prepare(stmt.p,sql.c_str(),sql.length());
			if(mysqlret != 0){
				log(lyramilk::log::error,__FUNCTION__) << D("数据库错误：%s",mysql_error(_db_ptr)) << std::endl;
				throw lyramilk::exception(D("数据库错误：%s",mysql_error(_db_ptr)));
			}
			int param_count = mysql_stmt_param_count(stmt.p);
			std::vector<MYSQL_BIND> bind(param_count);
			std::vector<std::vector<char> > param_buff(param_count);

			lyramilk::data::var::array::const_iterator it = args.begin();
			if(it != args.end()) ++it;
			for(int i=0;it != args.end();++it,++i){
				lyramilk::data::string sv = it->str();
				bind[i].buffer_type = MYSQL_TYPE_STRING;
				bind[i].buffer_length = (unsigned long)sv.length();

				param_buff[i].assign(sv.begin(),sv.end());

				bind[i].buffer = param_buff[i].data();
			}
			mysqlret = mysql_stmt_bind_param(stmt.p,bind.data());
			if(mysqlret != 0){
				log(lyramilk::log::error,__FUNCTION__) << D("数据库错误：%s",mysql_error(_db_ptr)) << std::endl;
				throw lyramilk::exception(D("数据库错误：%s",mysql_error(_db_ptr)));
			}
			mysqlret = mysql_stmt_execute(stmt.p);
			if(mysqlret != 0){
				log(lyramilk::log::error,__FUNCTION__) << D("数据库错误：%s",mysql_error(_db_ptr)) << std::endl;
				throw lyramilk::exception(D("数据库错误：%s",mysql_error(_db_ptr)));
			}
			if(lyramilk::teapoy::env::is_debug()){
				log(lyramilk::log::debug,__FUNCTION__) << D("执行SQL命令%s",sql.c_str()) << std::endl;
			}
			return mysql_affected_rows(_db_ptr);
		}

		static int define(lyramilk::script::engine* p)
		{
			{
				lyramilk::script::engine::functional_map fn;
				fn["setopt"] = lyramilk::script::engine::functional<smysql,&smysql::setopt>;
				fn["open"] = lyramilk::script::engine::functional<smysql,&smysql::open>;
				fn["use"] = lyramilk::script::engine::functional<smysql,&smysql::use>;
				fn["ok"] = lyramilk::script::engine::functional<smysql,&smysql::ok>;
				fn["execute"] = lyramilk::script::engine::functional<smysql,&smysql::execute>;
				fn["query"] = lyramilk::script::engine::functional<smysql,&smysql::query>;
				p->define("mysql",fn,smysql::ctr,smysql::dtr);
			}
			return 2;
		}
	};

	lyramilk::data::var GetMysqlByConfig(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS(args,0,lyramilk::data::var::t_str);
		const lyramilk::data::var& cfg = env::get_config("config");
		const lyramilk::data::var& v = cfg.path(args[0]);

		mysql_clients::ptr p = mysql_clients_multiton::instance()->getobj(v["id"],v);
		lyramilk::script::engine* e = (lyramilk::script::engine*)env.find(lyramilk::script::engine::s_env_engine())->second.userdata(lyramilk::script::engine::s_user_engineptr());
		lyramilk::data::var::array ar;
		lyramilk::data::var ariv("__mysql_client",&p);
		ar.push_back(ariv);
		return e->createobject("mysql",ar);
	}

	static int define(lyramilk::script::engine* p)
	{
		int i = 0;
		i+= smysql::define(p);
		i+= smysql_iterator::define(p);
		p->define("GetMysqlByConfig",GetMysqlByConfig),++i;
		return i;
	}


	static __attribute__ ((constructor)) void __init()
	{
		lyramilk::teapoy::script2native::instance()->regist("mysql",define);
	}
}}}
