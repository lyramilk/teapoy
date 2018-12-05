#include "httplistener.h"
#include "webservice.h"
#ifdef OPENSSL_FOUND
	#include <openssl/ssl.h>
#endif
#include <libmilk/log.h>
#include <libmilk/dict.h>

namespace lyramilk{ namespace teapoy {


#ifdef OPENSSL_FOUND
	int httplistener::next_proto_cb(SSL *s, const unsigned char **data, unsigned int *len,void *arg)
	{
		httplistener* plistener = (httplistener*)arg;
		*data = (const unsigned char*)plistener->webprotostr.c_str();
		*len = plistener->webprotostr.size();
		return SSL_TLSEXT_ERR_OK;
	}

	int httplistener::alpn_select_proto_cb(SSL *ssl, const unsigned char **out,unsigned char *outlen, const unsigned char *in,unsigned int inlen, void *arg)
	{
		httplistener* plistener = (httplistener*)arg;
		if(plistener == nullptr) return SSL_TLSEXT_ERR_NOACK;

#ifdef _DEBUG
		for (unsigned int i = 0; i < inlen; i += in[i] + 1) {
			const char* p = (const char*)&in[i + 1];
			std::size_t sz = in[i];

			std::cout << " * ";
			std::cout.write(p, sz);
			std::cout << std::endl;
		}
#endif

		for (unsigned int i = 0; i < inlen; i += in[i] + 1) {
			const unsigned char* p = (const unsigned char*)&in[i];
			std::size_t sz = in[i] + 1;

			unsigned char buff[256];
			memcpy(buff,p,sz);
			buff[sz] = 0;

			std::size_t pos = plistener->webprotostr.find( buff);
			if(pos != plistener->webprotostr.npos){
				*out = plistener->webprotostr.c_str() + pos + 1;
				*outlen = plistener->webprotostr[pos];
				std::cout.write((const char*)*out,*outlen) << std::endl;
				return SSL_TLSEXT_ERR_OK;
			}
		}
		return SSL_TLSEXT_ERR_NOACK;
	}
#endif

	httplistener::httplistener()
	{
		scheme = "http";
	}

	httplistener::~httplistener()
	{
	}

	lyramilk::netio::aiosession* httplistener::create()
	{
		aiohttpchannel* selector = lyramilk::netio::aiosession::__tbuilder<lyramilk::teapoy::aiohttpchannel>();
		if(selector){
			selector->service = this;
		}
		return selector;
	}

	bool httplistener::init_ssl(lyramilk::data::string certfilename, lyramilk::data::string keyfilename)
	{
#ifdef OPENSSL_FOUND
		SSL_CTX* ssl_ctx = (SSL_CTX*)get_ssl_ctx();
#if OPENSSL_VERSION_NUMBER >= 0x10002000L
		SSL_CTX_set_alpn_select_cb(ssl_ctx, alpn_select_proto_cb, this);
#endif
		SSL_CTX_set_next_protos_advertised_cb(ssl_ctx, next_proto_cb, this);
#endif
		return lyramilk::netio::aiolistener::init_ssl(certfilename,keyfilename);
	}

	void httplistener::define_http_protocols(lyramilk::data::string proto,httpadapterctr ctr,httpadapterdtr dtr)
	{
		webprotos[proto].ctr = ctr;
		webprotos[proto].dtr = dtr;

		lyramilk::klog(lyramilk::log::debug,"teapoy.define_http_protocols")<< D("定义协议%s",proto.c_str()) << std::endl;;

		webprotostr.clear();
		std::map<lyramilk::data::string,httpadapter_creater>::const_iterator it = webprotos.begin();
		for(;it!=webprotos.end();++it){
			webprotostr.push_back((unsigned char)it->first.size());
			webprotostr.append((unsigned char*)it->first.c_str(),it->first.size());
		}
	}

	httpadapter* httplistener::create_http_session_byprotocol(lyramilk::data::string proto,std::ostream& oss)
	{
		std::map<lyramilk::data::string,httpadapter_creater>::const_iterator it = webprotos.find(proto);
		if(it==webprotos.end()) return nullptr;
		return it->second.ctr(oss);
	}

	void httplistener::destory_http_session_byprotocol(lyramilk::data::string proto,httpadapter* ptr)
	{
		std::map<lyramilk::data::string,httpadapter_creater>::const_iterator it = webprotos.find(proto);
		if(it==webprotos.end()) return ;
		it->second.dtr(ptr);
	}

	lyramilk::io::aiopoll* httplistener::get_aio_pool()
	{
		return pool;
	}

}}
