#include "config.h"
#include "http.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include "stringutil.h"
#include <stdlib.h>
#include <stdio.h>

namespace lyramilk{ namespace teapoy {namespace http{

	lyramilk::data::string request::source()
	{
		if(!_source.empty()) return _source;
		sockaddr_in addr;
		socklen_t size = sizeof addr;
		if(getsockname(sockfd,(sockaddr*)&addr,&size) !=0 ) return _source;
		_source_port = ntohs(addr.sin_port);
		_source = inet_ntoa(addr.sin_addr);
		return _source;
	}

	lyramilk::data::string request::dest()
	{
		if(!_dest.empty()) return _dest;
		sockaddr_in addr;
		socklen_t size = sizeof addr;
		if(getpeername(sockfd,(sockaddr*)&addr,&size) !=0 ) return _dest;
		_dest_port = ntohs(addr.sin_port);
		_dest = inet_ntoa(addr.sin_addr);
		return _dest;
	}

	lyramilk::data::uint16 request::source_port()
	{
		if(_source_port) return _source_port;
		sockaddr_in addr;
		socklen_t size = sizeof addr;
		if(getsockname(sockfd,(sockaddr*)&addr,&size) !=0 ) return _source_port;
		_source_port = ntohs(addr.sin_port);
		_source = inet_ntoa(addr.sin_addr);
		return _source_port;
	}

	lyramilk::data::uint16 request::dest_port()
	{
		if(_dest_port) return _dest_port;
		sockaddr_in addr;
		socklen_t size = sizeof addr;
		if(getpeername(sockfd,(sockaddr*)&addr,&size) !=0 ) return _dest_port;
		_dest_port = ntohs(addr.sin_port);
		_dest = inet_ntoa(addr.sin_addr);
		return _dest_port;
	}

	request::request()
	{
		sockfd = 0;
		_source_port = 0;
		_dest_port = 0;
		sessionmgr = nullptr;
		header = nullptr;
		reset();
	}

	request::~request()
	{
		if(header) delete header;
	}

	void request::init(lyramilk::io::native_filedescriptor_type fd)
	{
		sockfd = fd;
	}

	bool request::parse(const char* buf,unsigned int size,unsigned int* remain)
	{
		if(s == s0){
			httpheaderstr.append(buf,size);
			std::size_t pos_headereof = httpheaderstr.find("\r\n\r\n");
			if(pos_headereof == httpheaderstr.npos) return false;
			pos_headereof += 4;

			header = new http_frame(this);
			if(!header->parse(httpheaderstr.c_str(),pos_headereof)){
				s = sbad;
				return true;
			}

			s = smime;

			if(header->get("Transfer-Encoding") == "chunked"){
				header->body = new http_chunkbody();
			}else{
				lyramilk::data::string cl_str = header->get("Content-Length");
				if(cl_str.empty()){
					s = sok;
					return true;
				}
				long long cl = strtoll(cl_str.c_str(),nullptr,10);
				if(cl > 0){
					header->body = new http_lengthedbody(cl);
				}else{
					s = sok;
					return true;
				}
			}
			unsigned int des = httpheaderstr.size() - pos_headereof;
			if(des > 0 && header->body->write(httpheaderstr.c_str() + pos_headereof,des,remain)){
				s = sok;
				return true;
			}
		}else if(header->body->write(buf,size,remain)){
			s = sok;
			return true;
		}
		return false;
	}

	bool request::ok()
	{
		return s != sbad;
	}

	void request::reset()
	{
		s = s0;
		httpheaderstr.clear();
		if(header) delete header;
		header = nullptr;
	}

	static std::map<int,lyramilk::data::string> __init_code_map()
	{
		std::map<int,lyramilk::data::string> ret;
		ret[100] = "100 Continue";
		ret[101] = "101 Switching Protocols";
		ret[200] = "200 OK";
		ret[201] = "201 Created";
		ret[202] = "202 Accepted";
		ret[203] = "203 Non-Authoritative Information";
		ret[204] = "204 No Content";
		ret[205] = "205 Reset Content";
		ret[206] = "206 Partial Content";
		ret[300] = "300 Multiple Choices";
		ret[301] = "301 Moved Permanently";
		ret[302] = "302 Moved Temporarily";
		ret[303] = "303 See Other";
		ret[304] = "304 Not Modified";
		ret[305] = "305 Use Proxy";
		ret[400] = "400 Bad Request";
		ret[401] = "401 Authorization Required";
		ret[402] = "402 Payment Required";
		ret[403] = "403 Forbidden";
		ret[404] = "404 Not Found";
		ret[405] = "405 Method Not Allowed";
		ret[406] = "406 Not Acceptable";
		ret[407] = "407 Proxy Authentication Required";
		ret[408] = "408 Request Time-out";
		ret[409] = "409 Conflict";
		ret[410] = "410 Gone";
		ret[411] = "411 Length Required";
		ret[412] = "412 Precondition Failed";
		ret[413] = "413 Request Entity Too Large";
		ret[414] = "414 Request-URI Too Large";
		ret[415] = "415 Unsupported Media Type";
		ret[500] = "500 Internal Server Error";
		ret[501] = "501 Method Not Implemented";
		ret[502] = "502 Bad Gateway";
		ret[503] = "503 Service Temporarily Unavailable";
		ret[504] = "504 Gateway Time-out";
		ret[505] = "505 HTTP Version Not Supported";
		ret[506] = "506 Variant Also Varies";
		return ret;
	}

	lyramilk::data::string get_error_code_desc(int code)
	{
		static std::map<int,lyramilk::data::string> code_map = __init_code_map();
		std::map<int,lyramilk::data::string>::iterator it = code_map.find(code);
		if(it == code_map.end()) return "500 Internal Server Error";
		return it->second;
	}

	response::response()
	{
		os = nullptr;
		major = 1;
		minor = 1;
		header["teapoy"] = "1.0.0";
	}

	response::~response()
	{
	}


	lyramilk::data::var& response::get(const lyramilk::data::string& key)
	{
		return header[key];
	}

	void response::set(const lyramilk::data::string& key,const lyramilk::data::var& value)
	{
		header[key] = value;
	}


	void response::set_http_version(lyramilk::data::uint32 major,lyramilk::data::uint32 minor)
	{
		this->major = major;
		this->minor = minor;
	}

	void response::init(std::ostream* os,lyramilk::data::var::map* cookies)
	{
		this->os = os;
		this->cookies = cookies;
	}

	void response::send_header_and_body(lyramilk::data::uint32 code,const char* p,lyramilk::data::uint64 l)
	{
		if(os){
			*os << "HTTP/" << major << "." << minor << " " << get_error_code_desc(code) << "\r\n";
			*os << "Access-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: *\r\n";
			header["Server"] = "teapoy/" TEAPOY_VERSION;
			header["Content-Length"] = l;

			{
				lyramilk::data::var::map::iterator it = header.begin();
				for(;it!=header.end();++it){
					*os << it->first << ": " << it->second << "\r\n";
				}
			}
			{
				lyramilk::data::var::map::iterator it = cookies->begin();
				for(;it!=cookies->end();++it){
					if(it->second.type() == lyramilk::data::var::t_map){
						lyramilk::data::var::map& m = it->second;
						lyramilk::data::var& v = m["value"];
						if(v.type_like(lyramilk::data::var::t_str)){
							*os << "Set-Cookie: " << it->first << "=" << v.str() << ";" << "\r\n";
						}
					}else if(it->second.type() == lyramilk::data::var::t_str){
						*os << "Set-Cookie: " << it->first << "=" << it->second.str() << ";" << "\r\n";
					}
				}
			}
			*os << "\r\n";
			os->write(p,l);
			os->flush();
		}
	}


	void response::send_header_and_length(lyramilk::data::uint32 code,lyramilk::data::uint64 l)
	{
		if(os){
			*os << "HTTP/" << major << "." << minor << " " << get_error_code_desc(code) << "\r\n";
			*os << "Access-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: *\r\n";

			header["Server"] = "teapoy/" TEAPOY_VERSION;
			header["Content-Length"] = l;

			{
				lyramilk::data::var::map::iterator it = header.begin();
				for(;it!=header.end();++it){
					*os << it->first << ": " << it->second << "\r\n";
				}
			}
			{
				lyramilk::data::var::map::iterator it = cookies->begin();
				for(;it!=cookies->end();++it){
					if(it->second.type() == lyramilk::data::var::t_map){
						lyramilk::data::var::map& m = it->second;
						lyramilk::data::var& v = m["value"];
						if(v.type_like(lyramilk::data::var::t_str)){
							*os << "Set-Cookie: " << it->first << "=" << v.str() << ";" << "\r\n";
						}
					}else if(it->second.type() == lyramilk::data::var::t_str){
						*os << "Set-Cookie: " << it->first << "=" << it->second.str() << ";" << "\r\n";
					}
				}
			}
			*os << "\r\n";
		}
	}

	void response::send_header_and_length(const char* code,lyramilk::data::uint64 code_length,lyramilk::data::uint64 l)
	{
		if(os){
			*os << "HTTP/" << major << "." << minor << " ";
			os->write(code,code_length) << "\r\n";
			*os << "Access-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: *\r\n";

			header["Server"] = "teapoy/" TEAPOY_VERSION;
			header["Content-Length"] = l;

			{
				lyramilk::data::var::map::iterator it = header.begin();
				for(;it!=header.end();++it){
					*os << it->first << ": " << it->second << "\r\n";
				}
			}
			{
				lyramilk::data::var::map::iterator it = cookies->begin();
				for(;it!=cookies->end();++it){
					if(it->second.type() == lyramilk::data::var::t_map){
						lyramilk::data::var::map& m = it->second;
						lyramilk::data::var& v = m["value"];
						if(v.type_like(lyramilk::data::var::t_str)){
							*os << "Set-Cookie: " << it->first << "=" << v.str() << ";" << "\r\n";
						}
					}else if(it->second.type() == lyramilk::data::var::t_str){
						*os << "Set-Cookie: " << it->first << "=" << it->second.str() << ";" << "\r\n";
					}
				}
			}
			*os << "\r\n";
		}
	}

	void response::send_body(const char* p,lyramilk::data::uint64 l)
	{
		os->write(p,l);
	}

/*
	void response::send_body_finish()
	{
		os->flush();
	}*/

	void response::send_header_for_chunk(lyramilk::data::uint32 code)
	{
		if(os){
			*os << "HTTP/" << major << "." << minor << " " << get_error_code_desc(code) << "\r\n";
			*os << "Access-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: *\r\n";
			header.erase("Content-Length");
			header["Server"] = "teapoy/" TEAPOY_VERSION;
			header["Transfer-Encoding"] = "chunked";

			{
				lyramilk::data::var::map::iterator it = header.begin();
				for(;it!=header.end();++it){
					*os << it->first << ": " << it->second << "\r\n";
				}
			}
			{
				lyramilk::data::var::map::iterator it = cookies->begin();
				for(;it!=cookies->end();++it){
					if(it->second.type() == lyramilk::data::var::t_map){
						lyramilk::data::var::map& m = it->second;
						lyramilk::data::var& v = m["value"];
						if(v.type_like(lyramilk::data::var::t_str)){
							*os << "Set-Cookie: " << it->first << "=" << v.str() << ";" << "\r\n";
						}
					}else if(it->second.type() == lyramilk::data::var::t_str){
						*os << "Set-Cookie: " << it->first << "=" << it->second.str() << ";" << "\r\n";
					}
				}
			}
			*os << "\r\n";
		}
	}

	void response::send_chunk(const char* p,lyramilk::data::uint32 l)
	{
		char buff_chunkheader[30];
		unsigned int szh = sprintf(buff_chunkheader,"%x\r\n",l);
		os->write(buff_chunkheader,szh);
		os->write(p,l);
		os->write("\r\n",2);
	}

	void response::send_chunk_finish()
	{
		os->write("0\r\n\r\n",5);
		os->flush();
	}

	void make_response_header(std::ostream& os,const char* retcodemsg,bool has_header_ending,int httpver_major,int httpver_minor)
	{
		os <<	"HTTP/" << httpver_major << "." << httpver_minor << " " << retcodemsg << "\r\n"
				"Server: teapoy/" TEAPOY_VERSION << "\r\n";
		os <<	"Access-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: *\r\n";
		if(has_header_ending) os << "\r\n";
	}
}}}
