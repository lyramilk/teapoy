#ifndef _lyramilk_teapoy_dnsproxy_h_
#define _lyramilk_teapoy_dnsproxy_h_

#include "config.h"
#include <libmilk/netaio.h>
#include <libmilk/thread.h>
#include <arpa/inet.h>
#include "httpclient.h"
#include "webservice.h"

namespace lyramilk{ namespace teapoy {

	struct dns_header
	{
		unsigned short transation_id:16;
		union{
			struct{
				//小端 第一个字节
				unsigned short rd:1;
				unsigned short tc:1;
				unsigned short aa:1;
				unsigned short opcode:4;
				unsigned short qr:1;
				//第二个字节
				unsigned short rcode:4;
				unsigned short reserve3:3;
				unsigned short ra:1;
			};
			unsigned short flag:16;
		};
		unsigned short question;
		unsigned short answer;
		unsigned short auth;
		unsigned short addition;
	};

	struct dns_addr_item
	{
		lyramilk::data::string name;
		lyramilk::data::string addr;
		unsigned short type;
		unsigned short classic;

		unsigned int ttl;
		unsigned short datalength;
		dns_addr_item()
		{
			ttl = 300;
			datalength = 4;
		}

		static const char* type_typename(unsigned short dnstype);
	};

	class dns_packet
	{
	  public:
		dns_header header;
		dns_addr_item queries;
		std::vector<dns_addr_item> answers;
	  	unsigned int copy(char* buff,unsigned int size) const;
	  	unsigned int from(const char* buff,unsigned int size);
	};

	struct dns_ipv4_result
	{
		union{
			unsigned int i;
			struct 
			{
				unsigned int a:8;
				unsigned int b:8;
				unsigned int c:8;
				unsigned int d:8;
			};
		}rdata;
		unsigned int ttl;
	};

	typedef std::map<lyramilk::data::string,dns_packet> dnscache_type;


	class dnsasync:public lyramilk::io::aioselector
	{
	  protected:
		lyramilk::io::native_filedescriptor_type fd;
		sockaddr_in addr;
	  protected:
		virtual bool notify_out();
		virtual bool notify_hup();
		virtual bool notify_err();
		virtual bool notify_pri();
		virtual lyramilk::io::native_filedescriptor_type getfd();
	  public:
		virtual bool open(const char* host,unsigned short port);
		virtual bool open(const struct sockaddr* addr,socklen_t addr_len);
		virtual bool async_send(unsigned char *buf, int buf_size);

		dnsasync();
		virtual ~dnsasync();
	};

	class dnsasynccache:public dnsasync
	{
	  protected:
		lyramilk::threading::mutex_rw& lock;
		dnscache_type& dnscache;
	  protected:
		virtual void ondestory();
		virtual bool notify_in();
	  public:
		dnsasynccache(lyramilk::threading::mutex_rw& l,dnscache_type& c);
		virtual ~dnsasynccache();
	};

	class dnsproxy : public lyramilk::netio::udplistener
	{
		struct sockaddr_in upstream;
		dnsasynccache* dch;

		lyramilk::threading::mutex_rw lock;
		dnscache_type dnscache;
		int serverfd;
		bool attachdch;

		lyramilk::data::string host;
		unsigned short port;
	  public:
		dnsproxy();
	  	virtual ~dnsproxy();

		virtual bool onrequest(const char* cache, int size, lyramilk::data::ostream& os,const struct sockaddr* addr,socklen_t addr_len);
		virtual int sync_call(const lyramilk::data::string& host,const char *buf, int buf_size,char *rdata,int rdata_size);
		virtual bool bind_upstream_dns(const lyramilk::data::string& host,unsigned short port = 53);
		virtual bool get_ipv4_by_host(const lyramilk::data::string& host,std::vector<dns_ipv4_result>* ipv4arr);
	};
}}

#endif
