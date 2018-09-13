#include "http_2_0.h"
#include "http_transaction.h"
#include <libmilk/log.h>
#include <libmilk/multilanguage.h>
#include <openssl/ssl.h>


namespace lyramilk{ namespace teapoy {


	static httpchannel* ctr()
	{
		return new http_2_0();
	}

	static void dtr(httpchannel* ptr)
	{
		delete (http_2_0*)ptr;
	}

	httpchannel_creater http_2_0::proto_info = {"h2",lyramilk::teapoy::ctr,lyramilk::teapoy::dtr};

	http_2_0::http_2_0()
	{
	}

	http_2_0::~http_2_0()
	{
	}

	void http_2_0::dtr()
	{
		lyramilk::teapoy::dtr((httpchannel*)this);
	}

	bool http_2_0::oninit(std::ostream& os)
	{
		return true;
	}

	bool http_2_0::onrequest(const char* cache,int size,std::ostream& os)
	{

		return false;
	}


}}
