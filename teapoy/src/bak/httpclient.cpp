#include "httpclient.h"
#include <libmilk/log.h>
#include <libmilk/multilanguage.h>
#include <unistd.h>
#include <stdlib.h>
#include <libmilk/codes.h>




#include <sys/epoll.h>
#include <sys/poll.h>

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <cassert>

#include <iostream>
#include <streambuf>

#ifdef OPENSSL_FOUND
	#include <openssl/bio.h>
	#include <openssl/crypto.h>
	#include <openssl/evp.h>
	#include <openssl/x509.h>
	#include <openssl/x509v3.h>
	#include <openssl/ssl.h>
	#include <openssl/err.h>
	#include <string.h>
#endif



namespace lyramilk{ namespace teapoy
{
	/*
	class httpclient
	{
	  public:
		httpclient();
		~httpclient();

		bool open(const lyramilk::data::string& url);
		int fd();

		bool post(const lyramilk::data::stringdict& headers,const lyramilk::data::string& body);
		bool get(const lyramilk::data::stringdict& headers);

		bool wait_response();
		int get_response(lyramilk::data::stringdict& headers,lyramilk::data::string& body);

		static lyramilk::data::string httpclient(const lyramilk::data::string& url,const lyramilk::data::stringdict& params,int timeout_msec = 2000);
	};	*/

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

COUT << "raw=" << rawurl << ",scheme=" << scheme << ",host=" << host << ",port=" << port << ",url=" << url << "," << std::endl;
		return lyramilk::netio::client::open(host.c_str(),(lyramilk::data::uint16)port);
	}

	bool httpclient::post(const lyramilk::data::stringdict& headers,const lyramilk::data::string& body)
	{
		TODO();
	}

	bool httpclient::get(const lyramilk::data::stringdict& headers)
	{
		lyramilk::data::stringstream ss;
		ss <<	"GET " << url << " HTTP/1.0\r\n"
				"Content-Length: 0\r\n"
				"Host: " << host << "\r\n";
		lyramilk::data::stringdict::const_iterator it = headers.begin();
		for(;it!=headers.end();++it){
			ss << it->first << " :" << it->second << "\r\n";
		}
		ss << "\r\n";

COUT << ss.str() << std::endl;
		write(ss.str().c_str(),ss.str().size());
		return true;
	}

	bool httpclient::wait_response()
	{
		TODO();
	}

	int httpclient::get_response(lyramilk::data::stringdict* headers,lyramilk::data::string* body)
	{
		lyramilk::data::stringdict& h = *headers;
		lyramilk::data::string& b = *body;
		char buff[4096];
		int r = read(buff,sizeof(buff));
		std::cout.write(buff,r) << std::endl;

		b = buff;
		return 200;
	}

	lyramilk::data::string httpclient::rcall(const char* url0,const lyramilk::data::stringdict& params,int timeout_msec)
	{
		lyramilk::data::string url = makeurl(url0,params);
		TODO();
	}


	lyramilk::data::string httpclient::makeurl(const char* url,const lyramilk::data::stringdict& params)
	{
		static lyramilk::data::coding* urlcomponent = lyramilk::data::codes::instance()->getcoder("urlcomponent");
		lyramilk::data::stringstream ssurl;
		ssurl << url;
		if(strchr(url,'?') == nullptr){
			ssurl << "?";
		}
		lyramilk::data::stringdict::const_iterator it = params.begin();
		for(;it!=params.end();++it){
			ssurl << "&" << urlcomponent->encode(it->first) << "=" << urlcomponent->encode(it->second);
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
