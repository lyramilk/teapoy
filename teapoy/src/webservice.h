#ifndef _lyramilk_teapoy_webservice_h_
#define _lyramilk_teapoy_webservice_h_

#include "config.h"
#include "mime.h"
#include <libmilk/netaio.h>

namespace lyramilk{ namespace teapoy {
	using lyramilk::data::stringdict;

	class httplistener;
	class aiohttpchannel;
	class httprequest;
	class httpresponse;
	class httpsession;
	class httpadapter;

	struct httpcookie
	{
		lyramilk::data::string key;
		lyramilk::data::string value;
		lyramilk::data::string domain;
		lyramilk::data::string path;
		time_t max_age;
		time_t expires;
		bool secure;
		bool httponly;
		httpcookie();
		~httpcookie();
	};


	class httprequest:public mime
	{
		friend class http_1_0;
		friend class http_1_1;
		friend class http_2_0;
		friend class aiohttpchannel;

		httpadapter* adapter;
		lyramilk::data::string fast_url;

		lyramilk::data::map _cookies;
		lyramilk::data::map _params;
	  public:
		lyramilk::data::string mode;
		lyramilk::data::stringdict data;
	  public:
		httprequest();
	  	virtual ~httprequest();
	  public:
		bool reset();
		lyramilk::data::string scheme();

		lyramilk::data::map& cookies();
		lyramilk::data::map& params();

		lyramilk::data::string url();
		void url(const lyramilk::data::string& paramurl);

		lyramilk::data::string uri();
	};

	class httpresponse
	{
		friend class http_1_0;
		friend class http_1_1;
		friend class http_2_0;
		friend class aiohttpchannel;
	  public:
		httpadapter* adapter;
		stringdict header;
	  public:
		httpresponse();
	  	virtual ~httpresponse();
	  public:
		bool reset();
		lyramilk::data::string get(const lyramilk::data::string& field);
		void set(const lyramilk::data::string& field,const lyramilk::data::string& value);
	};

	class httpsession
	{
		int _l;
	  public:
		httpsession();
	  	virtual ~httpsession();

		virtual bool init(const lyramilk::data::map& info) = 0;

		virtual bool set(const lyramilk::data::string& key,const lyramilk::data::string& value) = 0;
		virtual lyramilk::data::string get(const lyramilk::data::string& key) = 0;

		virtual lyramilk::data::string get_session_id() = 0;
	  public:
		virtual void add_ref();
		virtual void relese();
		virtual bool in_using();
	};

	class httpadapter
	{
	  public:
		static const char* get_error_code_desc(int code);
	  public:
		std::map<lyramilk::data::string,httpcookie> cookies;
		httplistener* service;
		aiohttpchannel* channel;
		httprequest* request;
		httpresponse* response;

		bool is_responsed;
	  protected:
		std::ostream& os;
	  public:
		httpadapter(std::ostream& oss);
		virtual ~httpadapter();


		void cookie_from_request();
		void set_cookie(const lyramilk::data::string& key,const lyramilk::data::string& value);
		lyramilk::data::string get_cookie(const lyramilk::data::string& key);

		void set_cookie_obj(const httpcookie& value);
		httpcookie& get_cookie_obj(const lyramilk::data::string& key);


		virtual void dtr() = 0;

		virtual bool oninit(std::ostream& os) = 0;
		virtual bool onrequest(const char* cache,int size,std::ostream& os) = 0;
		virtual bool reset() = 0;
	  public:
		virtual bool send_bodydata(httpresponse* response,const char* p,lyramilk::data::uint32 l);	//发送数据

		virtual bool send_header_with_length(httpresponse* response,lyramilk::data::uint32 code,lyramilk::data::uint64 content_length) = 0;	//正常模式

		virtual bool send_header_with_chunk(httpresponse* response,lyramilk::data::uint32 code) = 0;	//chunk模式
		virtual void send_chunk(httpresponse* response,const char* p,lyramilk::data::uint32 l);
		virtual void send_chunk_finish(httpresponse* response);

	  protected:
		virtual void send_raw_data(httpresponse* response,const char* p,lyramilk::data::uint32 l);	//发送原始数据
		httprequest pri_request;
		httpresponse pri_response;

	};

	typedef httpadapter* (*httpadapterctr)(std::ostream& oss);
	typedef void (*httpadapterdtr)(httpadapter* ptr);

	struct httpadapter_creater
	{
		lyramilk::data::string name;
		httpadapterctr ctr;
		httpadapterdtr dtr;
	};

	class aiohttpchannel:public lyramilk::netio::aiosession_sync
	{
	  protected:
		friend class httplistener;
		httpadapter* adapter;
		httplistener* service;
	  public:
		stringdict default_response_header;
	  public:
		aiohttpchannel();
		virtual ~aiohttpchannel();

		virtual bool oninit(std::ostream& os);
		virtual bool onrequest(const char* cache,int size,std::ostream& os);
		virtual void init_handler(httpadapter* handler);

		virtual bool lock();
		virtual bool unlock();
	};
}}

#endif
