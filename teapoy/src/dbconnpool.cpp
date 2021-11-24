#include <mysql/mysql.h>
#define MAROC_MYSQL MYSQL
#include "dbconnpool.h"
#include <libmilk/log.h>
#include <libmilk/dict.h>
#include <libmilk/factory.h>
#include <libmilk/exception.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>


#ifndef my_bool
	#define my_bool bool
#endif

#pragma push_macro("D")
#undef D
#pragma pop_macro("D")

namespace lyramilk{ namespace teapoy {

	static lyramilk::log::logss log(lyramilk::klog,"teapoy.native.dbconnect");

	mysql_client::mysql_client()
	{
		_db_ptr = nullptr;
	}

	mysql_client::~mysql_client()
	{
		mysql_close(_db_ptr);
	}

	mysql_clients::mysql_clients(const lyramilk::data::var& cfg)
	{
		lyramilk::data::var v = cfg;
		opts = v["opt"];
		cnf = v.path("/cnf/file").str();
		args = v.path("/cnf");
		group = v.path("/cnf/group");
	}

	mysql_clients::~mysql_clients()
	{
	}

	lyramilk::teapoy::mysql_client* mysql_clients::underflow()
	{
		lyramilk::teapoy::mysql_client* p = new lyramilk::teapoy::mysql_client();
		if(!p) return nullptr;

		p->_db_ptr = mysql_init(nullptr);
		my_bool my_true= true;
		mysql_options(p->_db_ptr, MYSQL_OPT_RECONNECT, &my_true);

		for(lyramilk::data::array::iterator it = opts.begin();it!=opts.end();++it){
			lyramilk::data::string opt = it->str();
			int ret = mysql_options(p->_db_ptr,MYSQL_INIT_COMMAND, opt.c_str());
			if(ret != 0){
				log(lyramilk::log::warning,__FUNCTION__) << D("设置Mysql配置(%s)失败：%s",opt.c_str(),mysql_error(p->_db_ptr)) << std::endl;
				delete p;
				return nullptr;
			}
		}

		int ret = mysql_options(p->_db_ptr,MYSQL_READ_DEFAULT_FILE, cnf.c_str());
		if(ret == 0){
			log(__FUNCTION__) << D("设置Mysql配置文件[%s]成功",cnf.c_str()) << std::endl;
		}else{
			log(lyramilk::log::warning,__FUNCTION__) << D("设置Mysql配置文件[%s]失败：%s",cnf.c_str(),mysql_error(p->_db_ptr)) << std::endl;
			delete p;
			return nullptr;
		}

		if(group.type_like(lyramilk::data::var::t_str)){
			lyramilk::data::string strgroup = group.str();
			ret = mysql_options(p->_db_ptr,MYSQL_READ_DEFAULT_GROUP, strgroup.c_str());
			if(ret == 0){
				log(__FUNCTION__) << D("设置Mysql配置组[%s]成功",strgroup.c_str()) << std::endl;
			}else{
				log(lyramilk::log::warning,__FUNCTION__) << D("设置Mysql配置组[%s]失败：%s",strgroup.c_str(),mysql_error(p->_db_ptr)) << std::endl;
				delete p;
				return nullptr;
			}
		}



		std::string host = args["host"].str();
		std::string user = args["user"].str();
		std::string passwd = args["password"].str();
		std::string db = args["db"].str();
		unsigned int port = args["port"].conv(3306);

		if(nullptr == mysql_real_connect(p->_db_ptr,
			host.empty()?nullptr:host.c_str(),
			user.empty()?nullptr:user.c_str(),
			passwd.empty()?nullptr:passwd.c_str(),
			db.empty()?nullptr:db.c_str(),
			port,
			nullptr,0)){
			log(lyramilk::log::warning,__FUNCTION__) << D("建立连接失败：host=%s:%d,user=%s,password=%s,db=%s,error=%s",host.c_str(),port,user.c_str(),passwd.c_str(),db.c_str(),mysql_error(p->_db_ptr)) << std::endl;
			delete p;
			return nullptr;
		}
		log(lyramilk::log::debug,__FUNCTION__) << D("建立连接成功：host=%s:%d,user=%s,password=%s,db=%s",host.c_str(),port,user.c_str(),passwd.c_str(),db.c_str()) << std::endl;
		return p;
	}

	mysql_clients_multiton* mysql_clients_multiton::instance()
	{
		static mysql_clients_multiton _mm;
		return &_mm;
	}

	mysql_clients::ptr mysql_clients_multiton::getobj(lyramilk::data::string id)
	{
		lyramilk::data::string token = id;
		mysql_clients* c = get(token);
		if(c == nullptr){
			const lyramilk::data::var& cfg = get_config(token);
			if(cfg.type() == lyramilk::data::var::t_invalid) return nullptr;

			lyramilk::threading::mutex_sync _(lock);
			define(token,new mysql_clients(cfg));
			c = get(token);
		}
		if(c == nullptr) return nullptr;
		return c->get();
	}

	void mysql_clients_multiton::add_config(lyramilk::data::string id,const lyramilk::data::var& cfg)
	{
		cfgmap[id] = cfg;
	}

	const lyramilk::data::var& mysql_clients_multiton::get_config(lyramilk::data::string id)
	{
		lyramilk::data::map::const_iterator it = cfgmap.find(id);
		if(it != cfgmap.end()){
			return it->second;
		}
		return lyramilk::data::var::nil;
	}

	///////////////////////////////////////////////////////////

	redis_clients::redis_clients(const lyramilk::data::var& cfg)
	{
		{
			const lyramilk::data::var& v = cfg["host"];
			if(v.type_like(lyramilk::data::var::t_str)){
				host = v.str();
			}else{
				host = "127.0.0.1";
			}
		}
		{
			const lyramilk::data::var& v = cfg["port"];
			if(v.type_like(lyramilk::data::var::t_int)){
				port = v;
			}else{
				port = 6379;
			}
		}
		{
			const lyramilk::data::var& v = cfg["password"];
			if(v.type_like(lyramilk::data::var::t_str)){
				pwd = v.str();
			}
		}

		const lyramilk::data::var &vrdonly = cfg["readonly"];
		if(vrdonly.type_like(lyramilk::data::var::t_bool)){
			readonly = vrdonly;
		}else{
			readonly = false;
		}
		const lyramilk::data::var &vlistener = cfg["enablelog"];
		if(vlistener.type_like(lyramilk::data::var::t_bool)){
			withlistener = vlistener;
		}else{
			withlistener = false;
		}
	}

	redis_clients::~redis_clients()
	{
	}

	lyramilk::teapoy::redis::redis_client* redis_clients::underflow()
	{
		lyramilk::teapoy::redis::redis_client* p = new lyramilk::teapoy::redis::redis_client();
		if(!p) return nullptr;
		p->open(host.c_str(),port);
		try{
			p->auth(pwd);
		}catch(lyramilk::exception& e){
		}
		if(withlistener){
			p->set_listener(lyramilk::teapoy::redis::redis_client::default_listener);
		}
		return p;
	}



	redis_clients_multiton* redis_clients_multiton::instance()
	{
		static redis_clients_multiton _mm;
		return &_mm;
	}

	redis_clients::ptr redis_clients_multiton::getobj(lyramilk::data::string id)
	{
		lyramilk::data::string token = id;
		redis_clients* c = get(token);
		if(c == nullptr){
			const lyramilk::data::var& cfg = get_config(token);
			if(cfg.type() == lyramilk::data::var::t_invalid) return nullptr;
			lyramilk::threading::mutex_sync _(lock);
			define(token,new redis_clients(cfg));
			c = get(token);
		}
		if(c == nullptr) return nullptr;
		return c->get();
	}

	void redis_clients_multiton::add_config(lyramilk::data::string id,const lyramilk::data::var& cfg)
	{
		cfgmap[id] = cfg;
	}

	const lyramilk::data::var& redis_clients_multiton::get_config(lyramilk::data::string id)
	{
		lyramilk::data::map::const_iterator it = cfgmap.find(id);
		if(it != cfgmap.end()){
			return it->second;
		}
		return lyramilk::data::var::nil;
	}
	///////////////////////////////////////////////////////////
#if (defined Z_HAVE_LIBMONGO) || (defined Z_HAVE_MONGODB)

	mongo_client::mongo_client():c(true)
	{
	}

	mongo_client::~mongo_client()
	{
	}

	mongo_clients::mongo_clients(const lyramilk::data::var& cfg)
	{
		{
			const lyramilk::data::var& v = cfg["host"];
			if(v.type_like(lyramilk::data::var::t_str)){
				host = v.str();
			}else{
				host = "127.0.0.1";
			}
		}
		{
			const lyramilk::data::var& v = cfg["port"];
			if(v.type_like(lyramilk::data::var::t_int)){
				port = v;
			}else{
				port = 27017;
			}
		}
		{
			const lyramilk::data::var& v = cfg["password"];
			if(v.type_like(lyramilk::data::var::t_str)){
				pwd = v.str();
			}
		}
		{
			const lyramilk::data::var& v = cfg["user"];
			if(v.type() == lyramilk::data::var::t_str){
				user = v.str();
			}
		}
	}

	mongo_clients::~mongo_clients()
	{
	}

	lyramilk::teapoy::mongo_client* mongo_clients::underflow()
	{
		lyramilk::teapoy::mongo_client* p = new lyramilk::teapoy::mongo_client();
		if(!p) return nullptr;

		::mongo::HostAndPort hap(host.c_str(),port);
		std::string errmsg;
		if(!p->c.connect(hap,errmsg)){
			log(lyramilk::log::error,"underflow") << D("Mongo(%s:%d)初始化失败：%s",host.c_str(),port,errmsg.c_str()) << std::endl;
			delete p;
			return nullptr;
		}
		p->user = user;
		p->pwd = pwd;
		return p;
	}

	mongo_clients_multiton* mongo_clients_multiton::instance()
	{
		static mongo_clients_multiton _mm;
		return &_mm;
	}

	mongo_clients::ptr mongo_clients_multiton::getobj(lyramilk::data::string id)
	{
		lyramilk::data::string token = id;
		mongo_clients* c = get(token);
		if(c == nullptr){
			const lyramilk::data::var& cfg = get_config(token);
			if(cfg.type() == lyramilk::data::var::t_invalid) return nullptr;

			lyramilk::threading::mutex_sync _(lock);
			define(token,new mongo_clients(cfg));
			c = get(token);
		}
		if(c == nullptr) return nullptr;
		return c->get();
	}

	void mongo_clients_multiton::add_config(lyramilk::data::string id,const lyramilk::data::var& cfg)
	{
		cfgmap[id] = cfg;
	}

	const lyramilk::data::var& mongo_clients_multiton::get_config(lyramilk::data::string id)
	{
		lyramilk::data::map::const_iterator it = cfgmap.find(id);
		if(it != cfgmap.end()){
			return it->second;
		}
		return lyramilk::data::var::nil;
	}
#endif
	///////////////////////////////////////////////////////////
	filelogers::filelogers(const lyramilk::data::var& cfg)
	{
		filepath = cfg["file"].str();
		fp = fopen(filepath.c_str(),"a");
		if(!fp){
			log(lyramilk::log::error,"underflow") << D("FileLoger初始化%s失败：%s",filepath.c_str(),strerror(errno)) << std::endl;
		}
		time_t stime = time(0);
		localtime_r(&stime,&daytime);
	}

	filelogers::~filelogers()
	{
	}

	bool filelogers::good()
	{
		return fp != nullptr;
	}

	bool filelogers::write(const char* str,std::size_t sz)
	{
		time_t nowtime = time(0);
		tm curtime;
		tm* t = localtime_r(&nowtime,&curtime);
		if(daytime.tm_year != t->tm_year || daytime.tm_mon != t->tm_mon || daytime.tm_mday != t->tm_mday){
			lyramilk::threading::mutex_sync _(lock);
			if(daytime.tm_year != t->tm_year || daytime.tm_mon != t->tm_mon || daytime.tm_mday != t->tm_mday){
				char buff[64];
				snprintf(buff,sizeof(buff),".%04d%02d%02d",(1900 + daytime.tm_year),(daytime.tm_mon + 1),daytime.tm_mday);
				daytime = *t;
				lyramilk::data::string destfilename = filepath;
				destfilename.append(buff);
				rename(filepath.c_str(),destfilename.c_str());
				FILE* newfp = fopen(filepath.c_str(),"a");
				if(newfp){
					FILE* oldfp = fp;
					fp = newfp;
					if(oldfp)fclose(oldfp);
				}
			}
		}
		if(!fp) return false;
		if(fwrite(str,sz,1,fp) == 1){
			fflush(fp);
			return true;
		}
		return false;
	}

	filelogers_multiton* filelogers_multiton::instance()
	{
		static filelogers_multiton _mm;
		return &_mm;
	}

	filelogers* filelogers_multiton::getobj(lyramilk::data::string id)
	{
		lyramilk::data::string token = id;
		filelogers* c = get(token);
		if(c != nullptr) return c;
		const lyramilk::data::var& cfg = get_config(token);
		if(cfg.type() == lyramilk::data::var::t_invalid) return nullptr;

		lyramilk::threading::mutex_sync _(lock);
		c = get(token);
		if(c != nullptr) return c;

		filelogers* p = new filelogers(cfg);
		if(p){
			define(token,p);
			return p;
		}
		return c;
	}

	void filelogers_multiton::add_config(lyramilk::data::string id,const lyramilk::data::var& cfg)
	{
		cfgmap[id] = cfg;
	}

	const lyramilk::data::var& filelogers_multiton::get_config(lyramilk::data::string id)
	{
		lyramilk::data::map::const_iterator it = cfgmap.find(id);
		if(it != cfgmap.end()){
			return it->second;
		}
		return lyramilk::data::var::nil;
	}





#ifdef CAVEDB_FOUND
	cavedb_leveldb_minimal_multiton* cavedb_leveldb_minimal_multiton::instance()
	{
		static cavedb_leveldb_minimal_multiton _mm;
		return &_mm;
	}

	lyramilk::cave::leveldb_minimal_adapter* cavedb_leveldb_minimal_multiton::getobj(lyramilk::data::string id)
	{
		lyramilk::data::string token = id;
		return get(token);
	}
	void cavedb_leveldb_minimal_multiton::add_config(lyramilk::data::string id,const lyramilk::data::var& cfg)
	{
		if(slaves.find(id) == slaves.end()){
			lyramilk::data::string emptystr;
			lyramilk::data::string ssdb_host = cfg["host"].conv(emptystr);
			unsigned short ssdb_port = cfg["port"].conv(0);
			lyramilk::data::string ssdb_password = cfg["password"].conv(emptystr);
			lyramilk::data::string leveldb_path = cfg["cache"].conv(emptystr);

			lyramilk::cave::leveldb_minimal_adapter* mstore = new lyramilk::cave::leveldb_minimal_adapter;
			mstore->open_leveldb(leveldb_path,1000);
			
			lyramilk::data::string replid = "";
			lyramilk::data::uint64 offset = 0;
			mstore->get_sync_info("",&replid,&offset);

			lyramilk::cave::slave_ssdb* datasource = new lyramilk::cave::slave_ssdb;
			slaves[id] = datasource;
			datasource->slaveof(ssdb_host,ssdb_port,ssdb_password,"",replid,offset,mstore);
			define(id,mstore);
		}
	}

#endif

}}