#include "webhook.h"

namespace lyramilk{ namespace teapoy { namespace web {
	void* webhook::create_env()
	{
		return nullptr;
	}

	void webhook::destroy_env(void* p)
	{
	}

	bool webhook::url_decryption(void* p,lyramilk::teapoy::http::request* req,lyramilk::data::string* dst)
	{
		return false;
	}

	bool webhook::result_encrypt(void* p,lyramilk::teapoy::web::session_info* si,const char* response,lyramilk::data::uint64 l,std::ostream& os)
	{
		return false;
	}

	webhook_helper::webhook_helper(webhook& argsh):h(argsh)
	{
		p = h.create_env();
	}

	webhook_helper::~webhook_helper()
	{
		h.destroy_env(p);
	}

	bool webhook_helper::url_decryption(lyramilk::teapoy::http::request* req,lyramilk::data::string* dst)
	{
		return h.url_decryption(p,req,dst);
	}

	bool webhook_helper::result_encrypt(lyramilk::teapoy::web::session_info* si,const char* response,lyramilk::data::uint64 l,std::ostream& os)
	{
		return h.result_encrypt(p,si,response,l,os);
	}

	allwebhooks* allwebhooks::instance()
	{
		static allwebhooks _mm;
		return &_mm;
	}
}}}
