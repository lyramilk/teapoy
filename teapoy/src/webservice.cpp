#include "webservice.h"
#include "httplistener.h"
#include "stringutil.h"
#include <libmilk/codes.h>
#include <libmilk/log.h>
#include <memory.h>

#ifdef OPENSSL_FOUND
	#include <openssl/ssl.h>
#endif
#ifdef NGHTTP2_FOUND
	#include <nghttp2/nghttp2.h>
#endif

namespace lyramilk{ namespace teapoy {



	static std::map<int,const char*> __init_code_map()
	{
		static std::map<int,const char*> ret;
		if(ret.empty()){
			//	Informational 1xx
			ret[100] = "100 Continue";
			ret[101] = "101 Switching Protocols";
			//	Successful 2xx
			ret[200] = "200 OK";
			ret[201] = "201 Created";
			ret[202] = "202 Accepted";
			ret[203] = "203 Non-Authoritative Information";
			ret[204] = "204 No Content";
			ret[205] = "205 Reset Content";
			ret[206] = "206 Partial Content";
			ret[207] = "207 Multi-Status";
			//	Redirection 3xx
			ret[300] = "300 Multiple Choices";
			ret[301] = "301 Moved Permanently";
			ret[302] = "302 Found";
			ret[303] = "303 See Other";
			ret[304] = "304 Not Modified";
			ret[305] = "305 Use Proxy";
			ret[306] = "306 Switch Proxy";	// 不再使用
			ret[307] = "307 Temporary Redirect";
			//	Client Error 4xx
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
			ret[416] = "416 Requested Range Not Satisfiable";
			ret[417] = "417 Expectation Failed";
			ret[421] = "421 too many connections";
			ret[422] = "422 Unprocessable Entity";
			ret[423] = "423 Locked";
			ret[424] = "424 Failed Dependency";
			ret[425] = "425 Unordered Collection";
			ret[426] = "426 Upgrade Required";
			ret[449] = "449 Retry With";
			ret[451] = "451 Unavailable For Legal Reasons";
			//	Server Error 5xx
			ret[500] = "500 Internal Server Error";
			ret[501] = "501 Method Not Implemented";
			ret[502] = "502 Bad Gateway";
			ret[503] = "503 Service Temporarily Unavailable";
			ret[504] = "504 Gateway Time-out";
			ret[505] = "505 HTTP Version Not Supported";
			ret[506] = "506 Variant Also Varies";

			char buff[256];
			int r = 0;

			for(std::map<int,const char*>::iterator it = ret.begin();it!=ret.end();++it){
				if(it->first >= 200 && it->first < 300) continue;
				errorpage* eg = new errorpage;
				r = snprintf(buff,sizeof(buff),"<html><head><meta charset=\"utf-8\" /><title>%s</title></head><body><center><h3>%s</h3></center></body></html>",it->second,it->second);
				eg->body.assign(buff,r);
				eg->header["Content-Type"] = "text/html;charset=utf8";
				eg->code = it->first;
				errorpage_manager::instance()->define(it->first,eg);
			}
		}
		return ret;
	}

	const char* get_error_code_desc(int code)
	{
		static std::map<int,const char*> code_map = __init_code_map();
		std::map<int,const char*>::iterator it = code_map.find(code);
		if(it == code_map.end()) return "500 Internal Server Error";
		return it->second;
	}

	void httpadapter::init()
	{
	}

	/// httprequest
	httprequest::httprequest()
	{
		adapter = nullptr;
	}

	httprequest::~httprequest()
	{
	}

	bool httprequest::reset()
	{
		is_params_parsed = false;
		is_cookies_parsed = false;
		fast_url.clear();
		_cookies.clear();
		_params.clear();
		mode.clear();
		header_extend.clear();
		return mime::reset();
	}

	lyramilk::data::string httprequest::scheme()
	{
		return adapter->service->scheme;
	}

	lyramilk::data::map& httprequest::cookies()
	{
		if(is_cookies_parsed) return _cookies;
		is_cookies_parsed = true;
		return _cookies;
	}


	lyramilk::data::map& httprequest::params()
	{
		if(is_params_parsed) return _params;
		is_params_parsed = 1;

		lyramilk::data::string urlparams;
		lyramilk::data::string url = get(":path");

		std::size_t sz = url.find("?");
		if(sz != url.npos){
			urlparams = url.substr(sz + 1);
		}

		if(get_body_ptr() && get_body_size() > 0){
			lyramilk::data::string s_mimetype = get("Content-Type");
			// 拼接正文中的参数
			{
				if(s_mimetype.find("www-form-urlencoded") != s_mimetype.npos){
					lyramilk::data::string charset;
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

					lyramilk::data::string paramurl((const char*)get_body_ptr(),get_body_size());
					if(charset.find_first_not_of("UuTtFf-8") != charset.npos){
						paramurl = lyramilk::data::codes::instance()->decode(charset,paramurl);
					}
					if(!urlparams.empty()){
						urlparams.push_back('&');
					}
					urlparams += paramurl;
				}
			}
			// 解析文件上传中的参数
			{
				if(s_mimetype.find("multipart/form-data") != s_mimetype.npos){

					std::size_t c = adapter->request->get_childs_count();
					for(std::size_t i=0;i<c;++i){
						mime* m = adapter->request->at(i);
						if(m->get("Content-Type") != "") continue;

					}
					//ss;
				}
			}
		}


		// 解析参数
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
			lyramilk::data::array& ar = parameters_of_key;
			ar.push_back(v);
		}


		return _params;
	}

	lyramilk::data::string httprequest::url()
	{
		if(fast_url.empty()){
			lyramilk::data::string path = header[":path"];
			static lyramilk::data::coding* code_url = lyramilk::data::codes::instance()->getcoder("url");
			std::size_t pos_path_qmask = path.find('?');
			if(pos_path_qmask == path.npos){
				path = code_url->decode(path);
			}else{
				if(code_url){
					lyramilk::data::string path1 = path.substr(0,pos_path_qmask);
					lyramilk::data::string path2 = path.substr(pos_path_qmask + 1);
					path = code_url->decode(path1) + "?" + path2;
				}
			}

			fast_url = path;
		}
		return fast_url;
	}

	void httprequest::url(const lyramilk::data::string& paramurl)
	{
		fast_url = paramurl;
	}

	lyramilk::data::string httprequest::uri()
	{
		if(header[":path"].find("://") == std::string::npos){
			return scheme() + "://" + header["host"] + header[":path"];
		}else{
			return header[":path"];
		}
	}

	/// httpresponse
	httpresponse::httpresponse()
	{
	}

	httpresponse::~httpresponse()
	{
	}

	bool httpresponse::reset()
	{
		header = adapter->channel->default_response_header;
		header_ex.clear();
		content_length = -1;
		code = 0;
		ischunked = false;
		adapter->pri_ss = httpadapter::ss_0;
		return true;
	}

	lyramilk::data::string httpresponse::get(const lyramilk::data::string& field)
	{
		http_header_type::const_iterator it = header.find(field);
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


	///	httpcookie
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


	///	errorpage_manager
	errorpage_manager* errorpage_manager::instance()
	{
		static errorpage_manager _mm;
		return &_mm;
	}


	/// httpadapter
	httpsessionptr::httpsessionptr()
	{
		p = nullptr;
	}

	httpsessionptr::httpsessionptr(httpsession* p)
	{
		this->p = p;
		if(p){
			p->add_ref();
		}
	}

	httpsessionptr::httpsessionptr(const httpsessionptr& o)
	{
		this->p = const_cast<httpsessionptr&>(o).p;
		if(p){
			p->add_ref();
		}
	}

	httpsessionptr& httpsessionptr::operator = (httpsession* p)
	{
		this->p = p;
		if(p){
			p->add_ref();
		}
		return *this;
	}

	httpsessionptr& httpsessionptr::operator = (const httpsessionptr& o)
	{
		this->p = const_cast<httpsessionptr&>(o).p;
		if(p){
			p->add_ref();
		}
		return *this;
	}

	httpsessionptr::~httpsessionptr()
	{
		if(p){
			p->relese();
		}
	}

	httpsession* httpsessionptr::operator->()
	{
		return p;
	}

	httpsessionptr::operator httpsession*()
	{
		return p;
	}

	/// httpadapter
	httpadapter::httpadapter(std::ostream& oss):os(oss)
	{
		channel = nullptr;
		service = nullptr;

		request = &pri_request;
		response = &pri_response;
		pri_ss = ss_0;
	}

	httpadapter::~httpadapter()
	{
		//LYRAMILK_STACK_TRACE(256);
	}

	bool httpadapter::reset()
	{
		return true;
	}
	void httpadapter::cookie_from_request()
	{
		lyramilk::data::string cookiestr = request->get("Cookie");
		lyramilk::data::strings cookielines = lyramilk::teapoy::split(cookiestr,";");
		lyramilk::data::strings::iterator it = cookielines.begin();
		for(;it!=cookielines.end();++it){
			lyramilk::data::strings cookiekv = lyramilk::teapoy::split(*it,"=");
			if(cookiekv.size() > 1){
				set_cookie(lyramilk::teapoy::trim(cookiekv[0]," \t\r\n"),lyramilk::teapoy::trim(cookiekv[1]," \t\r\n"));
			}
		}
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

	bool httpadapter::allow_gzip()
	{
		return false;
	}

	bool httpadapter::allow_chunk()
	{
		return false;
	}

	bool httpadapter::allow_cached()
	{
		return false;
	}

	bool httpadapter::merge_cookies()
	{
		std::map<lyramilk::data::string,httpcookie>::const_iterator it = response->adapter->cookies.begin();
		for(;it!=response->adapter->cookies.end();++it){
			lyramilk::data::ostringstream os;
			os << it->first << "=" << it->second.value;
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

			if(!it->second.samesite.empty()){
				os << ";SameSite=" << it->second.samesite;
			}else{
				os << ";SameSite=Lax";
			}

			if(it->second.secure){
				os << ";Secure";
			}
			if(it->second.httponly){
				os << ";HttpOnly";
			}
			response->header_ex.insert(std::make_pair("Set-Cookie",os.str()));
		}
		return true;
	}

	void httpadapter::send_raw_data(const char* p,lyramilk::data::uint32 l)
	{
		os.write(p,l);
	}

	void httpadapter::request_finish()
	{
	}

	httpsessionptr httpadapter::get_session()
	{
		httpsessionptr session;
		lyramilk::data::string sid = get_cookie("TeapoyId");
		//为了含义明确
		if(!sid.empty()){
			session = service->session_mgr->get_session(sid);
		}

		if(session == nullptr){
			session = service->session_mgr->create_session();
			if(session){
				lyramilk::data::string newsid = session->get_session_id();
				if(!newsid.empty()){
					httpcookie c;
					c.key = "TeapoyId";
					c.value = newsid;
					c.path = "/";
					c.httponly = true;
					c.samesite = "Strict";
					set_cookie_obj(c);
				}
			}
		}
		return session;
	}

	/// aiohttpchannel

	aiohttpchannel::aiohttpchannel()
	{
		adapter = nullptr;
		service = nullptr;

		default_response_header["Server"] = "teapoy/" TEAPOY_VERSION;
		default_response_header["Teapoy"] = TEAPOY_VERSION;
		default_response_header["Access-Control-Allow-Origin"] = "*";
		default_response_header["Access-Control-Allow-Methods"] = "*";
		//default_response_header["Access-Control-Allow-Headers"] = "*";
	}

	aiohttpchannel::~aiohttpchannel()
	{
		if(adapter){
			adapter->dtr();
		}
	}

	bool aiohttpchannel::oninit(std::ostream& os)
	{
		__init_code_map();
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
				adapter = service->create_http_session_byprotocol(lyramilk::data::string(data,len),os);
				if(adapter){
					init_handler(adapter);
					return adapter->oninit(os);
				}
			}

			ssl_peer_certificate_info = ssl_get_peer_certificate_info();
		}
#endif

		
		setkeepalive(20,3);

		return true;
	}

	bool aiohttpchannel::onrequest(const char* cache,int size,std::ostream& os)
	{
#ifdef _DEBUG
		printf("\t收到\x1b[36m%d\x1b[0m字节\n%.*s\n",size,size,cache);
		lyramilk::klog(lyramilk::log::trace,"aiohttpchannel").write(cache,size) << std::endl;
#endif
		if(adapter){
			return adapter->onrequest(cache,size,os);
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
			adapter = service->create_http_session_byprotocol("http/1.1",os);
		}
		if(memcmp("HTTP/1.0",httpver,8) == 0){
			adapter = service->create_http_session_byprotocol("http/1.0",os);
		}
		if(memcmp("HTTP/2.0",httpver,8) == 0){
			adapter = service->create_http_session_byprotocol("http/2.0",os);
		}

		if(adapter == nullptr){
			os << "HTTP/1.0 505 HTTP Version Not Supported\r\nContent-Length: 0\r\n\r\n";
			return false;
		}
		init_handler(adapter);
		return adapter->oninit(os) && adapter->onrequest(cache,size,os);
	}

	void aiohttpchannel::init_handler(httpadapter* handler)
	{
		adapter->channel = this;
		adapter->service = service;
		adapter->request->adapter = handler;
		adapter->response->adapter = handler;

	}



	bool aiohttpchannel::lock()
	{
		if(lyramilk::netio::aiosession::lock()){
			return true;
		}
		return false;
	}

	bool aiohttpchannel::unlock()
	{
		if(lyramilk::netio::aiosession::unlock()){
			return true;
		}
		return false;
	}



	session_response_datawrapper::session_response_datawrapper(httpadapter* _adapter,std::ostream& _os):adapter(_adapter),os(_os)
	{}

	session_response_datawrapper::~session_response_datawrapper()
	{}

	lyramilk::data::string session_response_datawrapper::class_name()
	{
		return "lyramilk.teapoy.session_response";
	}

	lyramilk::data::string session_response_datawrapper::name() const
	{
		return class_name();
	}

	lyramilk::data::datawrapper* session_response_datawrapper::clone() const
	{
		return new session_response_datawrapper(adapter,os);
	}

	void session_response_datawrapper::destory()
	{
		delete this;
	}

	bool session_response_datawrapper::type_like(lyramilk::data::var::vt nt) const
	{
		return false;
	}

}}
