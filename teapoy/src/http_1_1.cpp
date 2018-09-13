#include "http_1_1.h"
#include "stringutil.h"
#include "httplistener.h"
#include "url_dispatcher.h"
#include <string.h>
#include <stdlib.h>
#include <libmilk/debug.h>

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
		request_cache = nullptr;
		pps = pp_head;
		body_writep_offset = body_offset = 0;
		content_length = data_size = 0;
	}

	http_1_1::~http_1_1()
	{
	}

	void http_1_1::dtr()
	{
		lyramilk::teapoy::dtr((httpadapter*)this);
	}

	bool http_1_1::oninit(std::ostream& os)
	{
		return true;
	}

	bool http_1_1::onrequest(const char* cache,int size,std::ostream& os)
	{
		int surplus = 0;
		parse_result r = parse_request(cache,size,&surplus);
		if(r != pr_ok){
			if(r == pr_continue){
				return true;
			}
			return false;
		}
		response->set("Connection",request->get("Connection"));

		if(!service->call(request,response,this)){
			send_header_with_length(response,404,0);
		}

		lyramilk::data::string sconnect = response->get("Connection");
	
		if(sconnect != "close"){
			delete request_cache;
			request_cache = nullptr;
			if(surplus < 1){
				return true;
			}
			return onrequest(cache + size - surplus,surplus,os);
		}
		return false;
	}

	bool http_1_1::send_header_with_chunk(httpresponse* response,lyramilk::data::uint32 code)
	{
		if(is_responsed) return false;
		os <<	"HTTP/1.1 " << get_error_code_desc(code) << "\r\n"
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
		}
		os << "\r\n";
		is_responsed = true;
		return true;
	}

	bool http_1_1::send_header_with_length(httpresponse* response,lyramilk::data::uint32 code,lyramilk::data::uint64 content_length)
	{
		if(is_responsed) return false;
		os <<	"HTTP/1.1 " << get_error_code_desc(code) << "\r\n"
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
		}
		os << "\r\n";
		is_responsed = true;
		return true;
	}

	static const char* memfind(const char* p,std::size_t l,const char* d)
	{
		std::size_t dl = strlen(d);
		if(dl == 0) return d;
		while(true){
			const char* e = (const char*)memchr(p,d[0],l);
			if(e == nullptr) return nullptr;
			l -= (e-p);
			p = e + 1;
			if(l < dl) return nullptr;
			if(memcmp(e,d,dl) == 0){
				return e;
			}
		}
		return nullptr;
	}



	parse_result http_1_1::parse_request(const char* cache,int size,int* surplus)
	{
		if(request_cache == nullptr) request_cache = new fcache;
		request_cache->write(cache,size);

		if(pps == pp_head){
			const char* p = request_cache->data();

			const char* sep = memfind(p,request_cache->length(),"\r\n\r\n");
			if(sep == nullptr) return pr_continue;

			const char* header = p;
			std::size_t header_size = sep - p;
			body_writep_offset = body_offset = header_size + 4;

			const char* cap_eof = memfind(header,header_size,"\r\n");
			if(!cap_eof) return pr_error;


			lyramilk::data::string httpcaption(header,cap_eof - header);
			lyramilk::data::string httpheader(cap_eof+2,header + header_size - cap_eof - 2);
			lyramilk::data::strings fields = lyramilk::teapoy::split(httpcaption," ");
			if(fields.size() != 3){
				return pr_error;
			}

			request->header[":method"] = fields[0];
			request->header[":path"] = fields[1];	//url
			request->header[":version"] = fields[2];

			lyramilk::data::strings vheader = lyramilk::teapoy::split(httpheader,"\r\n");
			lyramilk::data::strings::iterator it = vheader.begin();
			lyramilk::data::string* laststr = nullptr;
			for(;it!=vheader.end();++it){
				std::size_t pos = it->find_first_of(":");
				if(pos != it->npos){
					lyramilk::data::string key = lyramilk::teapoy::lowercase(lyramilk::teapoy::trim(it->substr(0,pos)," \t\r\n"));

					lyramilk::data::string value = lyramilk::teapoy::trim(it->substr(pos + 1)," \t\r\n");
					if(!key.empty() && !value.empty()){
						laststr = &(request->header[key] = value);
					}
				}else{
					if(laststr){
						(*laststr) += *it;
					}
				}
			}
			{
				stringdict::const_iterator it = request->header.find("transfer-encoding");
				if(it != request->header.end() && it->second == "chunked"){
					pps = pp_chunked;
				}
			}
			if(pps == pp_head){
				stringdict::const_iterator it = request->header.find("content-length");
				if(it != request->header.end()){
					char* p;
					content_length = strtoll(it->second.c_str(),&p,10);
				}else{
					content_length = 0;
				}
				pps = pp_lengthed;
			}
		}

		if(pps == pp_lengthed){
			if(request_cache->length() - body_offset >= content_length){
				if(surplus){
					*surplus = request_cache->length() - body_offset - content_length;
				}
				request->body = request_cache->data();
				request->body_length = request_cache->length()- body_offset;

				is_responsed = false;
				return pr_ok;
			}
			return pr_continue;
		}else if(pps == pp_chunked){
		}else if(pps == pp_error){
			return pr_error;
		}
		return pr_error;
	}

}}
