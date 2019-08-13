#ifndef _lyramilk_teapoy_httplistener_h_
#define _lyramilk_teapoy_httplistener_h_

#include "config.h"
#include "webservice.h"
#include "url_dispatcher.h"
#include "sessionmgr.h"
#include <libmilk/netaio.h>
#ifdef OPENSSL_FOUND
	#include <openssl/ssl.h>
#endif

namespace lyramilk{ namespace teapoy {


	class httplistener:public lyramilk::netio::aiolistener
	{
		std::map<lyramilk::data::string,url_dispatcher> dispatcher;
	  protected:
#ifdef OPENSSL_FOUND
		static int next_proto_cb(SSL *s, const unsigned char **data, unsigned int *len,void *arg);
		static int alpn_select_proto_cb(SSL *ssl, const unsigned char **out,unsigned char *outlen, const unsigned char *in,unsigned int inlen, void *arg);
#endif
		std::map<lyramilk::data::string,httpadapter_creater> webprotos;
		lyramilk::data::chunk webprotostr;
	  public:
		lyramilk::data::string scheme;

		httpsession_manager* session_mgr;

		httplistener();
	  	virtual ~httplistener();
		virtual lyramilk::netio::aiosession* create();

		virtual bool init_ssl(lyramilk::data::string certfilename, lyramilk::data::string keyfilename);

		virtual void define_http_protocols(lyramilk::data::string proto,httpadapterctr ctr,httpadapterdtr dtr);
		virtual httpadapter* create_http_session_byprotocol(lyramilk::data::string proto,std::ostream& oss);
		virtual void destory_http_session_byprotocol(lyramilk::data::string proto,httpadapter* ptr);

		virtual lyramilk::io::aiopoll* get_aio_pool();


		virtual bool add_dispatcher_selector(lyramilk::data::string hostname,lyramilk::ptr<url_selector> selector);
		virtual bool remove_dispatcher(lyramilk::data::string hostname);

		virtual url_check_status call(lyramilk::data::string hostname,httprequest* request,httpresponse* response,httpadapter* adapter);
	};
}}

#endif


