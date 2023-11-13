#ifndef _lyramilk_teapoy_upstream_caller_h_
#define _lyramilk_teapoy_upstream_caller_h_

#include "config.h"
#include <libmilk/netaio.h>
#include "httpclient.h"
#include "webservice.h"

namespace lyramilk{ namespace teapoy {

	class upstream_caller : public lyramilk::io::aioselector
	{
		httpadapter* adapter;
		lyramilk::io::native_filedescriptor_type client_sock;
		lyramilk::data::ostringstream ss;

		httpclient hc;
	  public:
		upstream_caller();
		virtual ~upstream_caller();

		virtual bool rcall(const lyramilk::data::string& rawurl,const lyramilk::data::stringdict& params,httpadapter* adapter);


		virtual void set_client_sock(lyramilk::io::native_filedescriptor_type fd);

		virtual bool notify_in();
		virtual bool notify_out();
		virtual bool notify_hup();
		virtual bool notify_err();
		virtual bool notify_pri();

		virtual lyramilk::io::native_filedescriptor_type getfd();
		virtual void ondestory();

	  public:
		static upstream_caller* ctr();
		static void dtr(upstream_caller *p);
	};
}}

#endif
