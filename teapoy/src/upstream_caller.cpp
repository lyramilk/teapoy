#include "upstream_caller.h"
#include "httplistener.h"
#include <libmilk/log.h>
#include <unistd.h>

namespace lyramilk{ namespace teapoy {

	upstream_caller::upstream_caller()
	{
		client_sock = -1;
		adapter = nullptr;
	}

	upstream_caller::~upstream_caller()
	{
		if(client_sock >= 0){
			::close(client_sock);
		}
	}

	bool upstream_caller::rcall(const lyramilk::data::string& rawurl,const lyramilk::data::stringdict& params,httpadapter* adapter)
	{
		if(!hc.open(hc.makeurl(rawurl.c_str(),params))) return false;

		this->adapter = adapter;

		http_header_type headers;
		hc.get(headers);
		return true;
	}

	void upstream_caller::set_client_sock(lyramilk::io::native_filedescriptor_type fd)
	{
		client_sock = fd;
	}

	bool upstream_caller::notify_in()
	{
		http_header_type headers;
		lyramilk::data::string body;
		int code = hc.get_response(&headers,&body);
		if(code == 200){
			adapter->request->mode = "ondownstream";
			const_cast<http_header_type&>(adapter->request->get_header_obj()) = headers;
			adapter->request->header_extend[":body"] = body;
			if(!adapter->service->call(hc.get_host(),adapter->request,adapter->response,adapter)){
				adapter->response->code = 500;
			}
		}
		return false;
	}

	bool upstream_caller::notify_out()
	{
		TODO();
	}

	bool upstream_caller::notify_hup()
	{
		return false;
	}

	bool upstream_caller::notify_err()
	{
		return false;
	}

	bool upstream_caller::notify_pri()
	{
		TODO();
	}

	lyramilk::io::native_filedescriptor_type upstream_caller::getfd()
	{
		return hc.fd();
	}


	void upstream_caller::ondestory()
	{
		upstream_caller::dtr(this);
	}


	upstream_caller* upstream_caller::ctr()
	{
		return new upstream_caller;
	}

	void upstream_caller::dtr(upstream_caller *p)
	{
		delete p;
	}
}}



