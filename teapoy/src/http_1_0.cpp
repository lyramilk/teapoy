#include "http_1_0.h"
#include "stringutil.h"
#include "httplistener.h"
#include "url_dispatcher.h"
#include "fcache.h"


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
		return false;
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
		if(!service->call(request,response,this)){
			send_header_with_length(response,404,0);
		}

		lyramilk::data::string sconnect = response->get("Connection");
	
		if(sconnect == "keep-alive"){
			reset();
			if(bytesused == size){
				return true;
			}
			return onrequest(cache + bytesused,size - bytesused,os);
		}
		return false;
	}

	bool http_1_0::send_header_with_chunk(httpresponse* response,lyramilk::data::uint32 code)
	{
		if(is_responsed) return false;
		os <<	"HTTP/1.0 " << get_error_code_desc(code) << "\r\n"
				"Transfer-Encoding: chunked\r\n";
		{
			stringdict::const_iterator it = channel->default_response_header.begin();
			for(;it!=channel->default_response_header.end();++it){
				if(!it->second.empty()){
					os << it->first << ": " << it->second << "\r\n";
				}
			}
		
		}
		if(response != nullptr){
			stringdict::const_iterator it = response->header.begin();
			for(;it!=response->header.end();++it){
				if(!it->second.empty()){
					os << it->first << ": " << it->second << "\r\n";
				}
			}
			{
				std::map<lyramilk::data::string,httpcookie>::const_iterator it = response->adapter->cookies.begin();
				for(;it!=response->adapter->cookies.end();++it){
					os << "Set-Cookie: " << it->first << "=" << it->second.value;
					if(it->second.expires != 0){
						tm __t;
						tm *t = localtime_r(&it->second.expires,&__t);
						if(t){
							os << ";expires=" << asctime(&__t);
						}
					}
					if(it->second.max_age != 0){
						os << ";max_age=" << it->second.max_age;
					}
					if(!it->second.domain.empty()){
						os << ";domain=" << it->second.domain;
					}
					if(!it->second.path.empty()){
						os << ";path=" << it->second.path;
					}
					if(it->second.secure){
						os << ";Secure";
					}
					if(it->second.httponly){
						os << ";HttpOnly";
					}
					os << "\r\n";
				}
			}
		}
		os << "\r\n";
		is_responsed = true;
		return true;
	}

	bool http_1_0::send_header_with_length(httpresponse* response,lyramilk::data::uint32 code,lyramilk::data::uint64 content_length)
	{
		if(is_responsed) return false;
		os <<	"HTTP/1.0 " << get_error_code_desc(code) << "\r\n"
				"Content-Length: " << content_length << "\r\n";
		{
			stringdict::const_iterator it = channel->default_response_header.begin();
			for(;it!=channel->default_response_header.end();++it){
				if(!it->second.empty()){
					os << it->first << ": " << it->second << "\r\n";
				}
			}
		
		}
		if(response != nullptr){
			stringdict::const_iterator it = response->header.begin();
			for(;it!=response->header.end();++it){
				if(!it->second.empty()){
					os << it->first << ": " << it->second << "\r\n";
				}
			}
			{
				std::map<lyramilk::data::string,httpcookie>::const_iterator it = response->adapter->cookies.begin();
				for(;it!=response->adapter->cookies.end();++it){
					os << "Set-Cookie: " << it->first << "=" << it->second.value;
					if(it->second.expires != 0){
						tm __t;
						tm *t = localtime_r(&it->second.expires,&__t);
						if(t){
							os << ";expires=" << asctime(&__t);
						}
					}
					if(it->second.max_age != 0){
						os << ";max_age=" << it->second.max_age;
					}
					if(!it->second.domain.empty()){
						os << ";domain=" << it->second.domain;
					}
					if(!it->second.path.empty()){
						os << ";path=" << it->second.path;
					}
					if(it->second.secure){
						os << ";Secure";
					}
					if(it->second.httponly){
						os << ";HttpOnly";
					}
					os << "\r\n";
				}
			}
		}
		os << "\r\n";
		is_responsed = true;
		return true;
	}


}}