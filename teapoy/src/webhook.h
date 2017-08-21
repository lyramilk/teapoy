#ifndef _lyramilk_teapoy_webhook_h_
#define _lyramilk_teapoy_webhook_h_

#include "config.h"
#include <libmilk/var.h>
#include <libmilk/factory.h>

namespace lyramilk{ namespace teapoy{ 
	namespace http {
		class request;
	}
	namespace web {
		class session_info;

		class webhook
		{
		  public:
			virtual void* create_env();
			virtual void destroy_env(void* p);

			virtual bool url_decryption(void* p,lyramilk::teapoy::http::request* req,lyramilk::data::string* dst);
			virtual bool result_encrypt(void* p,lyramilk::teapoy::web::session_info* si,const char* response,lyramilk::data::uint64 l,std::ostream& os);
		};

		class webhook_helper
		{
			void* p;
			webhook& h;
		  public:
			webhook_helper(webhook& argsh);
			~webhook_helper();

			bool url_decryption(lyramilk::teapoy::http::request* req,lyramilk::data::string* dst);
			bool result_encrypt(lyramilk::teapoy::web::session_info* si,const char* response,lyramilk::data::uint64 l,std::ostream& os);
		};


		class allwebhooks:public lyramilk::util::multiton_factory<webhook>
		{
		  public:
			static allwebhooks* instance();
		};
	}
}}
#endif
#include "web.h"
