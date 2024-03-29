#include "http_1_1.h"
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
		return new http_1_1(oss);
	}

	static void dtr(httpadapter* ptr)
	{
		delete (http_1_1*)ptr;
	}

	httpadapter_creater http_1_1::proto_info = {"http/1.1",lyramilk::teapoy::ctr,lyramilk::teapoy::dtr};

	http_1_1::http_1_1(std::ostream& oss):httpadapter(oss)
	{
	}

	http_1_1::~http_1_1()
	{
	}

	bool http_1_1::reset()
	{
		return request->reset() && response->reset() && httpadapter::reset();
	}

	void http_1_1::dtr()
	{
		lyramilk::teapoy::dtr((httpadapter*)this);
	}

	bool http_1_1::oninit(std::ostream& os)
	{
		return reset();
	}

	bool http_1_1::onrequest(const char* cache,int size,std::ostream& os)
	{
		int bytesused = 0;
		mime_status r = request->write(cache,size,&bytesused);
		if(r != ms_success){
			if(r == ms_continue){
				return true;
			}
			return false;
		}


		lyramilk::data::string sreqconn = request->get("Connection");
		if(sreqconn.empty()){
			response->set("Connection","keep-alive");
		}else{
			response->set("Connection",sreqconn);
		}

		cookie_from_request();
		if(service->call(request->get("Host"),request,response,this) != cs_ok){
			response->code = 404;

			lyramilk::data::string method = request->get(":method");
			lyramilk::netio::netaddress addr = channel->dest();
			lyramilk::klog(lyramilk::log::debug,"teapoy.web.http_1_1.onrequest") << D("%u %s:%u %s %s %s",response->code,addr.ip_str().c_str(),addr.port(),method.c_str(),request->url().c_str(),request->get("User-Agent").c_str()) << std::endl;
		}

		request_finish();

		lyramilk::data::string sconnect = lyramilk::data::lower_case(response->get("Connection"));
	
		if(sconnect != "close"){
			reset();
			if(bytesused == size){
				return true;
			}
			return onrequest(cache + bytesused,size - bytesused,os);
		}
		return false;
	}



	http_1_1::send_status http_1_1::send_header()
	{
		if(pri_ss != ss_0){
			lyramilk::klog(lyramilk::log::warning,"teapoy.web.http_1_1.send_header") << D("重复发送http头") << std::endl;
			return ss_error;
		}

		lyramilk::data::ostringstream os;
		errorpage* page = errorpage_manager::instance()->get(response->code);
		if(page){
			os <<	"HTTP/1.1 " << get_error_code_desc(page->code) << "\r\n";
			response->set("Content-Length",lyramilk::data::str(page->body.size()));
		}else{
			os <<	"HTTP/1.1 " << get_error_code_desc(response->code) << "\r\n";
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

	bool http_1_1::send_data(const char* p,lyramilk::data::uint32 l)
	{
		if(pri_ss == ss_0) pri_ss = send_header();
		if(pri_ss != ss_need_body) return false;

		if(response->ischunked){
			char buff_chunkheader[32];
			unsigned int szh = sprintf(buff_chunkheader,"%x\r\n",l);
			send_raw_data(buff_chunkheader,szh);
			send_raw_data(p,l);
			send_raw_data("\r\n",2);
		}else{
			send_raw_data(p,l);
		}
		return true;
	}

	void http_1_1::request_finish()
	{
		if(pri_ss == ss_0) pri_ss = send_header();
		if(response->ischunked){
			send_raw_data("0\r\n\r\n",5);
		}
	}

	bool http_1_1::allow_gzip()
	{
		return true;
	}

	bool http_1_1::allow_chunk()
	{
		return true;
	}

	bool http_1_1::allow_cached()
	{
		return true;
	}

}}
