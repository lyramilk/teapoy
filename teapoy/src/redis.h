#ifndef _lyramilk_teapoy_redis_h_
#define _lyramilk_teapoy_redis_h_

#include "config.h"
#include <libmilk/var.h>
#include <stdarg.h>

namespace lyramilk{ namespace teapoy{ namespace redis{
	/// 普通异常，发生异常时程序可以捕获异常继续运行。
	class exception:public std::exception
	{
		lyramilk::data::string str;
	  public:
		/**
			@brief 无任何信息的构造函数
		*/
		exception() throw();
		/**
			@brief 只携带错误信息的异常
			@details 通过一个字符串来构造异常。
			@param str 格式化参数
		*/
		exception(lyramilk::data::string msg) throw();
		/**
			@breif 析构函数
		*/
		virtual ~exception() throw();
		/**
			@breif 返回异常信息。
			@details 覆盖了std::exception中的方法。
			@return 返回异常信息。
		*/
		virtual const char* what() const throw();
	};

	/// 索引溢出异常，在数组操作中，如果给定索引超过了数组元素数目时发生该异常。
	class exception_outofbound:public exception
	{
	  public:
		exception_outofbound() throw()
		{}
		exception_outofbound(lyramilk::data::string msg) throw():exception(msg)
		{}
	};

	/// 当数据的类型与预期该数据的类型不匹配且不兼容时发生该异常。
	class exception_typenotmatch:public exception
	{
	  public:
		exception_typenotmatch() throw()
		{}
		exception_typenotmatch(lyramilk::data::string msg) throw():exception(msg)
		{}
	};
	/// 初始化失败的对象会发生该异常。
	class exception_invalid:public exception
	{
	  public:
		exception_invalid() throw()
		{}
		exception_invalid(lyramilk::data::string msg) throw():exception(msg)
		{}
	};

	/// 执行命令失败时发生该异常
	class exception_executecommandfail:public exception
	{
	  public:
		exception_executecommandfail() throw()
		{}
		exception_executecommandfail(lyramilk::data::string msg) throw():exception(msg)
		{}
	};


	class redis_param;
	class redis_param_iterator:public std::iterator<std::output_iterator_tag,redis_param,int>
	{
		friend class redis_param;
		redis_param* parent;
		unsigned long long i;
		redis_param_iterator(const redis_param* parent);
	  public:
		redis_param_iterator();
		redis_param_iterator(const redis_param_iterator &o);
		~redis_param_iterator();

		redis_param_iterator& operator =(const redis_param_iterator &o);
		bool operator ==(const redis_param_iterator &o);
		bool operator !=(const redis_param_iterator &o);

		redis_param_iterator& operator ++();
		redis_param_iterator operator ++(int);

		redis_param operator*();
		redis_param operator->();
		static redis_param_iterator eof;
	};


	/**
		@brief redis参数
		@details 当redis_reply析构时，由该redis_reply获得得redis_param都会失效，所以想要长久地使用从redis_reply得到的数据，请拷贝该数据的副本。
	*/
	class redis_param
	{
	  protected:
		void* pnative;
		redis_param(void* param);
		operator bool();
	  public:
		/// redis参数类型
		enum param_type{
			t_unknow = 256,t_string,t_array,t_integer,t_nil,t_status,t_error
		};
		virtual ~redis_param();

		/// 返回对应的redis数据的类型。
		param_type type() const;

		/// 以stl字符串形式获得该参数
		lyramilk::data::string str() const throw(exception_typenotmatch,exception_invalid);
		const char* c_str() const throw(exception_typenotmatch,exception_invalid);

		/// 以 long long形式获得该参数
		long long toint() const throw(exception_typenotmatch,exception_invalid);
		
		/// lyramilk::data::string类型转换操作符
		operator lyramilk::data::string () const throw(exception_typenotmatch,exception_invalid);

		/// long long 类型转换操作符
		operator long long () const throw(exception_typenotmatch,exception_invalid);

		/// 获取数组中指定索引上的元素
		redis_param at(unsigned long long index) const throw(exception_typenotmatch,exception_invalid,exception_outofbound);

		/// 获取数组元素数量
		unsigned long long size() const throw(exception_typenotmatch,exception_invalid);

		/// 迭代器开始
		typedef redis_param_iterator iterator;
		iterator begin() const;
		iterator end() const;
		redis_param* operator->();
	};

	/// 代表执行一次命令的结果
	class redis_reply:public redis_param
	{
		friend class redis_client;
	  public:
		redis_reply(void* param);
		redis_reply(const redis_reply& o);
		redis_reply& operator =(const redis_reply& o);
		virtual ~redis_reply();

		/// 判断是否有错误。
		bool good() const;
	};


	class redis_client_listener
	{
	  public:
		virtual void notify(const char* str,int len,redis_reply& reply) = 0;
	};

	/// 代表一个redis客户端。
	class redis_client
	{
		bool master;
		void* ctx;
		lyramilk::data::string host;
		unsigned short port;
		lyramilk::data::string pwd;
		redis_client_listener* listener;
		bool isssdb;
	  public:
		redis_client();
		redis_client(const redis_client&);
		redis_client& operator =(const redis_client&);
		virtual ~redis_client();

		/// 连接一个redis服务器
		bool open(const char* host,unsigned short port,lyramilk::data::string pwd = "");

		/// 关闭一个redis链接。
		void close();

		/// 执行redis命令
		redis_reply execute(const char* fmt,...);

		/// 执行redis命令
		redis_reply execute(const char* fmt,va_list va);

		/// 设置命令监听器,并返回旧的监听器
		redis_client_listener* set_listener(redis_client_listener* lst);

		/// 测试客户端是否可用
		bool alive();

		int getfd();

		lyramilk::data::var::array ssdb_exec(const lyramilk::data::var::array& ar);

		bool is_ssdb();
	};



}}}



#endif
