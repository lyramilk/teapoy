#include "http_1_0.h"
#include "stringutil.h"
#include "httplistener.h"
#include "url_dispatcher.h"
#include "fcache.h"
#include <libmilk/log.h>
#include <libmilk/dict.h>
#include <libmilk/stringutil.h>


namespace lyramilk{ namespace teapoy {

	static httpadapter* ctr(std::ostream& oss)
	{
		return new http_1_0(oss);
	}

	static void dtr(httpadapter* ptr)
	{
		delete (http_1_0*)ptr;
	}

	httpadapter_creater http_1_0::proto_info = {"http/1.0",lyramilk::teapoy::ctr,lyramilk::teapoy::dtr};

	http_1_0::http_1_0(std::ostream& oss):http_1_1(oss)
	{
	}

	http_1_0::~http_1_0()
	{
	}

	bool http_1_0::reset()
	{
		return request->reset() && response->reset() && httpadapter::reset();
	}

	void http_1_0::dtr()
	{
		lyramilk::teapoy::dtr((httpadapter*)this);
	}

	bool http_1_0::oninit(std::ostream& os)
	{
		return reset();
	}

	bool http_1_0::onrequest(const char* cache,int size,std::ostream& os)
	{
		int bytesused = 0;
		mime_status r = request->write(cache,size,&bytesused);
		if(r != ms_success){
			if(r == ms_continue){
				return true;
			}
			return false;
		}
		response->set("Connection",request->get("Connection"));

		cookie_from_request();
		if(service->call(request->get("Host"),request,response,this) != cs_ok){
			response->code = 404;
			lyramilk::data::string method = request->get(":method");
			lyramilk::netio::netaddress addr = channel->dest();
			lyramilk::klog(lyramilk::log::debug,"teapoy.web.http_1_0.onrequest") << D("%u %s:%u %s %s %s",response->code,addr.ip_str().c_str(),addr.port(),method.c_str(),request->url().c_str(),request->get("User-Agent").c_str()) << std::endl;
		}

		request_finish();

		lyramilk::data::string sconnect = lyramilk::data::lower_case(response->get("Connection"));
	
		if(sconnect == "keep-alive"){
			reset();
			if(bytesused == size){
				return true;
			}
			return onrequest(cache + bytesused,size - bytesused,os);
		}
		return false;
	}

	http_1_0::send_status http_1_0::send_header()
	{
		if(pri_ss != ss_0){
			lyramilk::klog(lyramilk::log::warning,"teapoy.web.http_1_0.send_header") << D("重复发送http头") << std::endl;
			return ss_error;
		}

		lyramilk::data::ostringstream os;
		errorpage* page = errorpage_manager::instance()->get(response->code);
		if(page){
			os <<	"HTTP/1.0 " << get_error_code_desc(page->code) << "\r\n";
			response->set("Content-Length",lyramilk::data::str(page->body.size()));
		}else{
			os <<	"HTTP/1.0 " << get_error_code_desc(response->code) << "\r\n";
			if(response->content_length == -1){
				response->ischunked = true;
				response->set("Transfer-Encoding","chunked");
			}else{
				response->set("Content-Length",lyramilk::data::str(response->content_length));
			}
		}

		merge_cookies();
		{
			http_header_type::const_iterator it = response->header.begin();
			for(;it!=response->header.end();++it){
				if(!it->second.empty()){
					os << it->first << ": " << it->second << "\r\n";
				}
			}
		}
		{
			std::multimap<lyramilk::data::string,lyramilk::data::string>::const_iterator it = response->header_ex.begin();
			for(;it!=response->header_ex.end();++it){
				if(!it->second.empty()){
					os << it->first << ": " << it->second << "\r\n";
				}
			}
		}

		os << "\r\n";
		send_raw_data(os.str().c_str(),os.str().size());

		if(page){
			send_raw_data(page->body.c_str(),page->body.size());
			return ss_nobody;
		}

		if(request->get(":method") == "HEAD"){
			return ss_nobody;
		}

		return ss_need_body;
	}

	bool http_1_0::allow_gzip()
	{
		return true;
	}

	bool http_1_0::allow_chunk()
	{
		return false;
	}

	bool http_1_0::allow_cached()
	{
		return true;
	}

}}
