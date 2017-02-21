#ifndef _lyramilk_teapoy_redis_h_
#define _lyramilk_teapoy_redis_h_

#include "config.h"
#include <libmilk/var.h>
#include <libmilk/netio.h>
#include <stdarg.h>

namespace lyramilk{ namespace teapoy{ namespace redis{
	/// 代表一个redis客户端。
	typedef bool (*redis_client_watch_handler)(bool success,const lyramilk::data::var& v,void* args);
	/// 代表一个redis监听器
	typedef void (*redis_client_listener)(const lyramilk::data::string& addr,const lyramilk::data::var::array& cmd,bool success,const lyramilk::data::var& ret);

	class redis_client:public lyramilk::netio::client
	{
		lyramilk::data::string host;
		unsigned short port;
		lyramilk::data::string pwd;
		enum redisdb_type{
			t_unknow,
			t_redis,
			t_ssdb,
		}type;
		redis_client_listener listener;
		lyramilk::data::string addr;
		void reconnect();
	  public:
		redis_client();
		virtual ~redis_client();

		virtual bool open(lyramilk::data::string host,lyramilk::data::uint16 port);
		virtual bool close();
		/// 设置监听器
		void set_listener(redis_client_listener lst);
		/// 登录
		bool auth(lyramilk::data::string password);
		/// 执行redis命令，第二个参数为输出参数
		bool exec(const lyramilk::data::var::array& cmd,lyramilk::data::var& ret);
		/// 异步执行redis命令
		bool async_exec(const lyramilk::data::var::array& cmd,redis_client_watch_handler h,void* args,bool thread_join = true);
		/// 判断这个redis链接是不是指向一个ssdb数据库。
		bool is_ssdb();

		bool parse(lyramilk::data::stringstream& is,lyramilk::data::var& v);

		void static default_listener(const lyramilk::data::string& addr,const lyramilk::data::var::array& cmd,bool success,const lyramilk::data::var& ret);
	};

}}}

#endif
