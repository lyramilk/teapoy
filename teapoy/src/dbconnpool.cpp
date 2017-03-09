#include <libmilk/log.h>
#include <libmilk/multilanguage.h>
#include <libmilk/factory.h>
#include <libmilk/exception.h>
#include <stdio.h>

#include <mysql/mysql.h>
#define MAROC_MYSQL MYSQL
#include "dbconnpool.h"

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
		opts = cfg["opt"];
		cnf = cfg.path("/cnf/file").str();
		group = cfg.path("/cnf/group");
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

		for(lyramilk::data::var::array::iterator it = opts.begin();it!=opts.end();++it){
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
				return nullptr;
			}
		}

		if(nullptr == mysql_real_connect(p->_db_ptr,nullptr,nullptr,nullptr,nullptr,0,nullptr,0)){
			return nullptr;
		}
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
		lyramilk::data::var::map::const_iterator it = cfgmap.find(id);
		if(it != cfgmap.end()){
			return it->second;
		}
		return lyramilk::data::var::nil;
	}

	///////////////////////////////////////////////////////////

	redis_clients::redis_clients(const lyramilk::data::var& cfg)
	{

		host = cfg["host"].str();
		port = cfg["port"];
		pwd = cfg["password"].str();

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
		lyramilk::data::var::map::const_iterator it = cfgmap.find(id);
		if(it != cfgmap.end()){
			return it->second;
		}
		return lyramilk::data::var::nil;
	}


	///////////////////////////////////////////////////////////
	filelogers::filelogers(const lyramilk::data::var& cfg)
	{
		filepath = cfg["file"].str();
		fp = fopen(filepath.c_str(),"ab");
	}

	filelogers::~filelogers()
	{
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
		if(c == nullptr){
			const lyramilk::data::var& cfg = get_config(token);
			if(cfg.type() == lyramilk::data::var::t_invalid) return nullptr;
			lyramilk::threading::mutex_sync _(lock);
			define(token,new filelogers(cfg));
			c = get(token);
		}
		return c;
	}

	void filelogers_multiton::add_config(lyramilk::data::string id,const lyramilk::data::var& cfg)
	{
		cfgmap[id] = cfg;
	}

	const lyramilk::data::var& filelogers_multiton::get_config(lyramilk::data::string id)
	{
		lyramilk::data::var::map::const_iterator it = cfgmap.find(id);
		if(it != cfgmap.end()){
			return it->second;
		}
		return lyramilk::data::var::nil;
	}
}}