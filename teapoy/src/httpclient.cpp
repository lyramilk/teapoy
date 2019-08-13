#include "httpclient.h"
#include "stringutil.h"

#include <libmilk/log.h>
#include <libmilk/dict.h>
#include <unistd.h>
#include <stdlib.h>
#include <libmilk/codes.h>
#include <string.h>



namespace lyramilk{ namespace teapoy
{
	httpclient::httpclient()
	{
		timeout_msec = -1;
	}

	httpclient::~httpclient()
	{
	}

	bool httpclient::open(const lyramilk::data::string& rawurl)
	{
		int port = -1;
		lyramilk::data::string scheme;
		{
			std::size_t scheme_sep = rawurl.find(":");
			if(scheme_sep != rawurl.npos && rawurl.size() > scheme_sep+2 && rawurl.compare(scheme_sep,3,"://") == 0){
				scheme = rawurl.substr(0,scheme_sep);
				std::size_t host_sep = rawurl.find("/",scheme_sep+3);
				host = rawurl.substr(scheme_sep+3,host_sep - scheme_sep - 3);
				std::size_t port_sep = host.find_last_of(":");
				if(port_sep != host.npos){
					char *tmp;
					port = strtoll(host.c_str() + port_sep + 1,&tmp,10);
					host = host.substr(0,port_sep);
				}
				url = rawurl.substr(host_sep);
			}
		}

		if(scheme == "https"){
			if(port == -1) port = 443;
		}else if(scheme == "http"){
			if(port == -1) port = 80;
		}else{
			return false;	
		}

		if(scheme == "https"){
			init_ssl();
			ssl(true);
		}

		return lyramilk::netio::client::open(host.c_str(),(lyramilk::data::uint16)port);
	}

	bool httpclient::post(const http_header_type& headers,const lyramilk::data::string& body)
	{
		TODO();
	}

	bool httpclient::get(const http_header_type& headers)
	{
		lyramilk::data::stringstream ss;
		ss <<	"GET " << url << " HTTP/1.1\r\n"
				"Content-Length: 0\r\n"
				"Host: " << host << "\r\n"
				"User-Agent: " "Teapoy/" TEAPOY_VERSION "\r\n";
		http_header_type::const_iterator it = headers.begin();
		for(;it!=headers.end();++it){
			ss << it->first << " :" << it->second << "\r\n";
		}
		ss << "\r\n";

		write(ss.str().c_str(),ss.str().size());
		return true;
	}

	lyramilk::data::string httpclient::get_host()
	{
		return host;
	}

	bool httpclient::wait_response(int timeout_msec)
	{
		if(check_read(timeout_msec)){
			return true;
		}
		return false;
	}

	int httpclient::get_response(http_header_type* headers,lyramilk::data::string* body)
	{
		http_header_type& response_headers = *headers;

		lyramilk::netio::socket_istream iss(this);
		lyramilk::data::string response_header_str;
		int parse_status = 0;
		while(iss.good() && parse_status<4 ){
			char c = iss.get();

			switch(parse_status){
			  case 0:
				if(c == '\r'){
					parse_status = 1;
				}else{
					parse_status = 0;
				}
				break;
			  case 1:
				if(c == '\n'){
					parse_status = 2;
				}else{
					parse_status = 0;
				}
				break;
			  case 2:
				if(c == '\r'){
					parse_status = 3;
				}else{
					parse_status = 0;
				}
				break;
			  case 3:
				if(c == '\n'){
					parse_status = 4;
				}else{
					parse_status = 0;
				}
				break;
			};
			response_header_str.push_back(c);
		}
		lyramilk::data::strings vheader = lyramilk::teapoy::split(response_header_str,"\r\n");

		{
			// 解析http头
			lyramilk::data::strings::iterator it = vheader.begin();
			lyramilk::data::string* laststr = nullptr;

			if(it!=vheader.end()){
				lyramilk::data::strings response_caption = lyramilk::teapoy::split(*it," ");
				if(response_caption.size() == 3){
					response_headers[":version"] = response_caption[0];
					response_headers[":status"] = response_caption[1];
				}
			}

			for(++it;it!=vheader.end();++it){
				std::size_t pos = it->find_first_of(":");
				if(pos != it->npos){
					lyramilk::data::string key = lyramilk::teapoy::lowercase(lyramilk::teapoy::trim(it->substr(0,pos)," \t\r\n"));

					lyramilk::data::string value = lyramilk::teapoy::trim(it->substr(pos + 1)," \t\r\n");
					if(!key.empty() && !value.empty()){
						laststr = &(response_headers[key] = value);
					}
				}else{
					if(laststr){
						(*laststr) += *it;
					}
				}
			}
		}

		{
			// 解析body
			http_header_type::iterator it = response_headers.find("content-length");
			if(it!=response_headers.end()){
				char* p;
				lyramilk::data::int64 cl = strtoull(it->second.c_str(),&p,10);

				while(iss.good() && cl > 0){
					char buff[4096];
					iss.read(buff,std::min((lyramilk::data::int64)sizeof(buff),cl));

					lyramilk::data::int64 gc = iss.gcount();
					if(gc > 0){
						if(cl > gc){
							cl -= gc;
							body->append(buff,gc);
						}else{
							body->append(buff,cl);
							return atoi(response_headers[":status"].c_str());
						}
					}
				}

				return atoi(response_headers[":status"].c_str());
			}

			it = response_headers.find("transfer-encoding");
			if(it!=response_headers.end() && it->second == "chunked"){
				lyramilk::data::string cls;

				while(iss.good()){
					cls.clear();
					char c;
					do{
						c = iss.get();
						cls.push_back(c);
					}while(c != '\n');
					char* p;
					lyramilk::data::int64 cl = strtoull(cls.c_str(),&p,16);
					if(cl == 0) break;
					char buff[4096];
					if(cl > 4096){
						while(cl > 0){
							iss.read(buff,std::min(4096ll,cl));
							lyramilk::data::int64 gc = iss.gcount();
							cl -= gc;
							body->append(buff,gc);
						}
					}else{
						iss.read(buff,cl);
						lyramilk::data::int64 gc = iss.gcount();
						cl -= gc;
						body->append(buff,gc);
					}

					iss.get();
					iss.get();
				}
				return atoi(response_headers[":status"].c_str());
			}

			it = response_headers.find("connection");
			if(it!=response_headers.end() && it->second == "close"){
				while(iss){
					char buff[4096];
					iss.read(buff,sizeof(buff));
					body->append(buff,iss.gcount());
				}
				return atoi(response_headers[":status"].c_str());
			}

		}
		return 404;
	}

	lyramilk::data::string httpclient::rcall(const char* url0,int timeout_msec)
	{
		httpclient c;
		c.open(url0);

		http_header_type request_headers;
		if(c.get(request_headers) && c.wait_response(timeout_msec)){
			http_header_type response_headers;
			lyramilk::data::string response_body;
			lyramilk::data::string response_body_utf8;

			int code = c.get_response(&response_headers,&response_body);
			if(code == 200){
				lyramilk::data::string& v = response_headers["content-type"]; 
				lyramilk::data::strings vs = lyramilk::teapoy::split(v,";");
				if(vs.size() == 2){
					if(vs[0].find("text/") != vs[0].npos){

						std::size_t pos = vs[1].find("charset=");
						if(pos != vs[1].npos){
							lyramilk::data::string charset = lyramilk::teapoy::trim(vs[1].substr(9)," \t\r\n");
							try{
								response_body_utf8 = lyramilk::data::iconv(response_body,charset,"utf8");
							}catch(std::exception&e){
								response_body_utf8 = response_body;
							}
							if(response_body_utf8.empty()){
								response_body_utf8 = response_body;
							}
						}
					}
				}else{
					response_body_utf8 = response_body;
				}

				return response_body_utf8;
			}
		}

		return "";
	}

	lyramilk::data::chunk httpclient::rcallb(const char* url0,int timeout_msec)
	{
		httpclient c;
		c.open(url0);

		http_header_type request_headers;
		if(c.get(request_headers) && c.wait_response(timeout_msec)){
			http_header_type response_headers;
			lyramilk::data::string response_body;

			int code = c.get_response(&response_headers,&response_body);
			if(code == 200){
				return lyramilk::data::chunk((const unsigned char*)response_body.c_str(),response_body.size());
			}
		}

		return lyramilk::data::chunk();
	}


	lyramilk::data::string httpclient::makeurl(const char* url,const lyramilk::data::stringdict& params)
	{
		static lyramilk::data::coding* urlcomponent = lyramilk::data::codes::instance()->getcoder("urlcomponent");
		lyramilk::data::stringstream ssurl;
		ssurl << url;
		if(strchr(url,'?') == nullptr){
			ssurl << "?";
		}else{
			ssurl << "&";
		}
		bool firstparam = true;
		lyramilk::data::stringdict::const_iterator it = params.begin();
		for(;it!=params.end();++it){
			if(firstparam){
				firstparam = false;
			}else{
				ssurl << "&";
			}
			ssurl << urlcomponent->encode(it->first) << "=" << urlcomponent->encode(it->second);
		}
		return ssurl.str();
	}

	lyramilk::data::string httpclient::makeurl(const char* url,const lyramilk::data::map& params)
	{
		static lyramilk::data::coding* urlcomponent = lyramilk::data::codes::instance()->getcoder("urlcomponent");
		lyramilk::data::stringstream ssurl;
		ssurl << url;
		if(strchr(url,'?') == nullptr){
			ssurl << "?";
		}else{
			ssurl << "&";
		}
		bool firstparam = true;
		lyramilk::data::map::const_iterator it = params.begin();
		for(;it!=params.end();++it){
			if(firstparam){
				firstparam = false;
			}else{
				ssurl << "&";
			}
			ssurl << urlcomponent->encode(it->first) << "=" << urlcomponent->encode(it->second.str());
		}
		return ssurl.str();
	}

}}

/*
		struct addrinfo hints;
		hints.ai_family = AF_INET;
		hints.ai_flags = AI_PASSIVE;
		hints.ai_flags = AI_CANONNAME;
		hints.ai_protocol = IPPROTO_IP;
		hints.ai_socktype = SOCK_STREAM;


		struct addrinfo *res,*pres;
		getaddrinfo(host.c_str(),"80",&hints,&res);
		struct sockaddr_in *addr;

		for (pres = res; pres != NULL; pres = pres->ai_next) {
			addr = (struct sockaddr_in *)pres->ai_addr;
			if(addr){
				break;
			}
		}
*/
