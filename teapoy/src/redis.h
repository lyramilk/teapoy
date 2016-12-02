#ifndef _lyramilk_teapoy_redis_h_
#define _lyramilk_teapoy_redis_h_

#include "config.h"
#include <libmilk/var.h>
#include <libmilk/netio.h>
#include <stdarg.h>

namespace lyramilk{ namespace teapoy{ namespace redis{
	/// 代表一个redis客户端。
	typedef bool (*redis_client_watch_handler)(bool success,const lyramilk::data::var& v,void* args);

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
	  public:
		redis_client();
		virtual ~redis_client();

		bool auth(lyramilk::data::string password);
		/// 执行redis命令，第二个参数为输出参数
		bool exec(const lyramilk::data::var::array& cmd,lyramilk::data::var& ret);
		/// 异步执行redis命令
		bool async_exec(const lyramilk::data::var::array& cmd,redis_client_watch_handler h,void* args,bool thread_join = true);
		/// 判断这个redis链接是不是指向一个ssdb数据库。
		bool is_ssdb();
	};

}}}

#endif
