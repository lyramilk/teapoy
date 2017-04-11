#ifndef _lyramilk_teapoy_http_h_
#define _lyramilk_teapoy_http_h_

#include <libmilk/var.h>
#include <libmilk/netaio.h>
#include <libmilk/factory.h>
#include "mime.h"
#include "session.h"

namespace lyramilk{ namespace teapoy {namespace http{
	class request;
/*
	class mimetype_parser
	{
	};

	class mimetype_parser_factory:public lyramilk::util::multiton_factory<mimetype_parser>
	{
	  public:
		static mimetype_parser_factory* instance();
	};

	class transfer_encoding_reader
	{
	};
	class transfer_encoding_reader_factory:public lyramilk::util::multiton_factory<transfer_encoding_reader>
	{
	  public:
		static transfer_encoding_reader_factory* instance();
	};
*/

	class http_body
	{
		lyramilk::data::string s_cache;
	  protected:
		const char* s_ptr;
		lyramilk::data::uint64 s_len;
		void reserve(lyramilk::data::uint64 sz);
		void writedata(const char* p,unsigned int sz);
	  public:
		http_body();
		virtual ~http_body();
		virtual bool write(const char* p,unsigned int sz,unsigned int* remain) = 0;
		virtual bool ok() const = 0;
		virtual const char* ptr() const;
		virtual lyramilk::data::uint64 size() const;

		virtual void release() = 0;
	};

	class http_chunkbody:public http_body
	{
		lyramilk::data::uint64 block_size;
		lyramilk::data::uint64 block_eof;
		lyramilk::data::string block_cache;
	  public:
		http_chunkbody();
		virtual ~http_chunkbody();
		virtual void release();
		virtual bool write(const char* p,unsigned int sz,unsigned int* remain);
		virtual bool ok() const;
	};

	class http_lengthedbody:public http_body
	{
		lyramilk::data::uint64 totalbytes;
		lyramilk::data::uint64 hasbytes;
	  public:
		http_lengthedbody(lyramilk::data::uint64 bodylength);
		virtual ~http_lengthedbody();
		virtual void release();
		virtual bool write(const char* p,unsigned int sz,unsigned int* remain);
		virtual bool ok() const;
	};

	class http_refbody:public http_body
	{
		virtual bool write(const char* p,unsigned int sz,unsigned int* remain);
		virtual bool ok() const;
	  public:
		http_refbody(const char* pbody,lyramilk::data::uint64 bodylength);
		virtual ~http_refbody();
		virtual void release();
	};

	class http_resource:public mime
	{
	  protected:
		std::vector<http_resource> *pchilds;
		virtual void init_body();
	  public:
		http_body* body;
	  public:
		http_resource();
		virtual ~http_resource();

		long childcount();
		http_resource* child(long index);
	};

	class http_frame:public http_resource
	{
		request* req;

		lyramilk::data::var::map _cookies;
		lyramilk::data::var::map _params;

		bool body_inited;
		bool cookies_inited;
		bool params_inited;
		virtual void init_body();
	  public:
		http_frame(request* args);
		bool parse(const char* p,long long sz);
		bool ok();

		lyramilk::data::var::map& cookies();
		lyramilk::data::var::map& params();

		lyramilk::data::string uri;
		lyramilk::data::string url;
		lyramilk::data::string method;
		lyramilk::data::uint32 major:4;
		lyramilk::data::uint32 minor:4;

		lyramilk::data::string boundary;
		struct 
		{
			unsigned int multipart:1;
			unsigned int bad:1;
		}flags;
	};

	class request
	{
		lyramilk::data::string httpheaderstr;
		enum 
		{
			s0,
			smime,	//己成功解析mime头
			sok,	//己接收了全部的请求体
			sbad,	//http头错误
		}s;
		lyramilk::data::string _source;
		lyramilk::data::string _dest;
		lyramilk::data::uint16 _source_port;
		lyramilk::data::uint16 _dest_port;

		lyramilk::io::native_filedescriptor_type sockfd;
	  public:
		http_frame* header;
		lyramilk::teapoy::web::sessions* sessionmgr;

		lyramilk::data::string ssl_peer_certificate_info;

		lyramilk::data::string source();
		lyramilk::data::string dest();
		lyramilk::data::uint16 source_port();
		lyramilk::data::uint16 dest_port();

		request();
		virtual ~request();

		virtual void init(lyramilk::io::native_filedescriptor_type fd);
		virtual bool parse(const char* buf,unsigned int size,unsigned int* remain);
		virtual bool ok();
		virtual void reset();
	};


	lyramilk::data::string get_error_code_desc(int code);
	void make_response_header(std::ostream& os,const char* retcodemsg,bool has_header_ending,int httpver_major = 1,int httpver_minor = 0);
	
}}}

#endif
