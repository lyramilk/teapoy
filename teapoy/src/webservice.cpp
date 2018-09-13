#include "webservice.h"
#include "httplistener.h"
#include "stringutil.h"
#include <libmilk/codes.h>

#ifdef OPENSSL_FOUND
	#include <openssl/ssl.h>
#endif
#ifdef NGHTTP2_FOUND
	#include <nghttp2/nghttp2.h>
#endif

namespace lyramilk{ namespace teapoy {



	static std::map<int,const char*> __init_code_map()
	{
		std::map<int,const char*> ret;
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

	const char* httpadapter::get_error_code_desc(int code)
	{
		static std::map<int,const char*> code_map = __init_code_map();
		std::map<int,const char*>::iterator it = code_map.find(code);
		if(it == code_map.end()) return "500 Internal Server Error";
		return it->second;
	}

	/// httprequest
	httprequest::httprequest()
	{
		adapter = nullptr;
	}

	httprequest::~httprequest()
	{
	}

	lyramilk::data::string httprequest::scheme()
	{
		return adapter->service->scheme;
	}

	lyramilk::data::var::map& httprequest::cookies()
	{
		if(!_cookies.empty()) return _cookies;
		_cookies["..init"] = 1;

		return _cookies;
	}


	lyramilk::data::var::map& httprequest::params()
	{
		if(!_params.empty()) return _params;
		_params["..init"] = 1;

		lyramilk::data::string urlparams;
		lyramilk::data::string url = get(":path");
COUT << url << std::endl;
		std::size_t sz = url.find("?");
		if(sz != url.npos){
			urlparams = url.substr(sz + 1);
		}

		//解析请求正文中的参数
		{
			lyramilk::data::string s_mimetype = get("Content-Type");
			lyramilk::data::string charset;
			if(body && body_length > 0 && s_mimetype.find("www-form-urlencoded") != s_mimetype.npos){
				std::size_t pos_charset = s_mimetype.find("charset=");
				if(pos_charset!= s_mimetype.npos){
					std::size_t pos_charset_bof = pos_charset + 8;
					std::size_t pos_charset_eof = s_mimetype.find(";",pos_charset_bof);
					if(pos_charset_eof != s_mimetype.npos){
						charset = s_mimetype.substr(pos_charset_bof,pos_charset_eof - pos_charset_bof);
					}else{
						charset = s_mimetype.substr(pos_charset_bof);
					}
				}

				lyramilk::data::string paramurl((const char*)body,body_length);
				if(charset.find_first_not_of("UuTtFf-8") != charset.npos){
					paramurl = lyramilk::data::codes::instance()->decode(charset,paramurl);
				}
				if(!urlparams.empty()){
					urlparams.push_back('&');
				}
				urlparams += paramurl;
			}
		}

		lyramilk::data::strings param_fields = lyramilk::teapoy::split(urlparams,"&");
		lyramilk::data::strings::iterator it = param_fields.begin();

		lyramilk::data::coding* urlcoder = lyramilk::data::codes::instance()->getcoder("urlcomponent");
		for(;it!=param_fields.end();++it){
			std::size_t pos_eq = it->find("=");
			if(pos_eq == it->npos || pos_eq == it->size()) continue;
			lyramilk::data::string k = it->substr(0,pos_eq);
			lyramilk::data::string v = it->substr(pos_eq + 1);
			if(urlcoder){
				k = urlcoder->decode(k);
				v = urlcoder->decode(v);
			}

			lyramilk::data::var& parameters_of_key = _params[k];
			parameters_of_key.type(lyramilk::data::var::t_array);
			lyramilk::data::var::array& ar = parameters_of_key;
			ar.push_back(v);
		}
		return _params;
	}

	lyramilk::data::string httprequest::url()
	{
		if(fast_url.empty()){
			fast_url = header[":path"];
		}
		return fast_url;
	}

	void httprequest::url(const lyramilk::data::string& paramurl)
	{
		fast_url = paramurl;
	}

	lyramilk::data::string httprequest::uri()
	{
		return scheme() + "://" + header["host"] + url();
	}

	/// httpresponse
	httpresponse::httpresponse()
	{
	}

	httpresponse::~httpresponse()
	{
	}

	lyramilk::data::string httpresponse::get(const lyramilk::data::string& field)
	{
		stringdict::const_iterator it = header.find(field);
		if(it != header.end()){
			return it->second;
		}
		return "";
	}

	void httpresponse::set(const lyramilk::data::string& field,const lyramilk::data::string& value)
	{
		header[field] = value;
	}

	/// httpsession
	httpsession::httpsession()
	{
		_l = 0;
	}

	httpsession::~httpsession()
	{
	}

	void httpsession::add_ref()
	{
		__sync_add_and_fetch(&_l,1);
	}

	void httpsession::relese()
	{
		__sync_sub_and_fetch(&_l,1);
	}

	bool httpsession::in_using()
	{
		return _l > 0;
	}

	/// httppart
	httppart::httppart()
	{
		body = nullptr;
		body_length = 0;
	}

	httppart::~httppart()
	{

	}

	lyramilk::data::string httppart::get(const lyramilk::data::string& field)
	{
		lyramilk::data::string lowercase_field = lowercase(field);

		stringdict::const_iterator it = header.find(lowercase_field);
		if(it != header.end()){
			return it->second;
		}
		return "";
	}

	void httppart::set(const lyramilk::data::string& field,const lyramilk::data::string& value)
	{
		lyramilk::data::string lowercase_field = lowercase(field);
		header[lowercase_field] = value;
	}

	//	httpcookie
	httpcookie::httpcookie()
	{
		max_age = 0;
		expires = 0;
		secure = false;
		httponly = false;
	}

	httpcookie::~httpcookie()
	{
	}


	/// httpadapter
	httpadapter::httpadapter(std::ostream& oss):os(oss)
	{
		channel = nullptr;
		service = nullptr;
		is_responsed = false;

		request = &pri_request;
		response = &pri_response;
	}

	httpadapter::~httpadapter()
	{
		//LYRAMILK_STACK_TRACE(256);
	}


	void httpadapter::set_cookie(const lyramilk::data::string& key,const lyramilk::data::string& value)
	{
		cookies[key].value = value;
	}

	lyramilk::data::string httpadapter::get_cookie(const lyramilk::data::string& key)
	{
		return cookies[key].value;
	}

	void httpadapter::set_cookie_obj(const httpcookie& value)
	{
		cookies[value.key] = value;
	}

	httpcookie& httpadapter::get_cookie_obj(const lyramilk::data::string& key)
	{
		return cookies[key];
	}

	bool httpadapter::send_bodydata(httpresponse* response,const char* p,lyramilk::data::uint32 l)
	{
		os.write(p,l);
		return true;
	}

	void httpadapter::send_raw_data(httpresponse* response,const char* p,lyramilk::data::uint32 l)
	{
		os.write(p,l);
	}

	void httpadapter::send_chunk(httpresponse* response,const char* p,lyramilk::data::uint32 l)
	{
		char buff_chunkheader[32];
		unsigned int szh = sprintf(buff_chunkheader,"%x\r\n",l);
		os.write(buff_chunkheader,szh);
		os.write(p,l);
		os.write("\r\n",2);
	}

	void httpadapter::send_chunk_finish(httpresponse* response)
	{
		os.write("0\r\n\r\n",5);
	}




	/// aiohttpchannel

	aiohttpchannel::aiohttpchannel()
	{
		handler = nullptr;
		service = nullptr;
		pending = false;

		default_response_header["Server"] = "teapoy/" TEAPOY_VERSION;
		default_response_header["Teapoy"] = TEAPOY_VERSION;
		default_response_header["Access-Control-Allow-Origin"] = "*";
		default_response_header["Access-Control-Allow-Methods"] = "*";
		//default_response_header["Access-Control-Allow-Headers"] = "*";
	}

	aiohttpchannel::~aiohttpchannel()
	{
		if(handler){
			handler->dtr();
		}
	}

	bool aiohttpchannel::oninit(std::ostream& os)
	{
#ifdef OPENSSL_FOUND
		if(ssl()){
			const char* data = nullptr;
			unsigned int len = 0;


			SSL_get0_next_proto_negotiated((SSL*)get_ssl_obj(), (const unsigned char**)&data, &len);

#if OPENSSL_VERSION_NUMBER >= 0x10002000L
			if(len ==0){
				SSL_get0_alpn_selected((SSL*)get_ssl_obj(),(const unsigned char**)&data,&len);
			}
#endif
			if(len > 0){
				handler = service->create_http_session_byprotocol(lyramilk::data::string(data,len),os);
				if(handler){
					init_handler(handler);
					return handler->oninit(os);
				}
			}
		}
#endif
		return true;
	}

	bool aiohttpchannel::onrequest(const char* cache,int size,std::ostream& os)
	{
printf("%.*s\n",size,cache);
		if(handler){
			if(handler->onrequest(cache,size,os)){
				if(!pending){
					handler->dtr();
				}
				return true;
			}
			return false;

		}
		int space = 0;
		const char* httpver = nullptr;
		int httpverbytes = 0;
		for(const char* p=cache;p<cache+size-1;++p){
			if(p[0] == ' ') ++space;
			if(space == 2 && httpver == nullptr){
				httpver = p + 1;
			}
			if(p[0] == '\r' && p[1] == '\n'){
				if(p < httpver){
					os << "HTTP/1.0 505 HTTP Version Not Supported\r\nContent-Length: 0\r\n\r\n";
					return false;
				}
				httpverbytes = p-httpver;
				break;
			}
		}

		if(httpverbytes != 8){
			os << "HTTP/1.0 505 HTTP Version Not Supported\r\nContent-Length: 0\r\n\r\n";
			return false;
		}
		if(memcmp("HTTP/1.1",httpver,8) == 0){
			handler = service->create_http_session_byprotocol("http/1.1",os);
			if(handler){
				init_handler(handler);
				if(handler->onrequest(cache,size,os)){
					if(!pending){
						handler->dtr();
					}
					handler = nullptr;
					return true;
				}
				return false;
			}
		}
		if(memcmp("HTTP/1.0",httpver,8) == 0){
			handler = service->create_http_session_byprotocol("http/1.0",os);
			if(handler){
				init_handler(handler);
				if(handler->onrequest(cache,size,os)){
					if(!pending){
						handler->dtr();
					}
					handler = nullptr;
					return true;
				}
				return false;
			}
		}
		if(memcmp("HTTP/2.0",httpver,8) == 0){
			handler = service->create_http_session_byprotocol("http/2.0",os);
			if(handler){
				init_handler(handler);
				if(handler->onrequest(cache,size,os)){
					if(!pending){
						handler->dtr();
					}
					handler = nullptr;
					return true;
				}
				return false;
			}
		}

		os << "HTTP/1.0 505 HTTP Version Not Supported\r\nContent-Length: 0\r\n\r\n";
		return false;
	}

	void aiohttpchannel::init_handler(httpadapter* handler)
	{
		handler->channel = this;
		handler->service = service;
		handler->request->adapter = handler;
		handler->request->adapter = handler;
	}



	bool aiohttpchannel::lock()
	{
		if(lyramilk::netio::aiosession2::lock()){
			pending = true;
			return true;
		}
		return false;
	}

	bool aiohttpchannel::unlock()
	{
		if(lyramilk::netio::aiosession2::unlock()){
			pending = false;
			return true;
		}
		return false;
	}

}}
