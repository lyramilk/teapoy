#include "config.h"
#include "http.h"
#include "stringutil.h"
#include <strings.h>
#include <libmilk/exception.h>
#include <libmilk/multilanguage.h>
#include <libmilk/codes.h>
#include <stdlib.h>

#define PARSE_STATUS_URL_PARAM	0x1
#define PARSE_STATUS_BODY_PARAM	0x2
#define PARSE_STATUS_COOKIE	0x4

namespace lyramilk{ namespace teapoy {namespace http{

	request::request()
	{
		reset();
	}

	request::~request()
	{
	}

	bool request::parse(const char* buf,int size,int* remain)
	{
		if(s == s0){
			httpheader.append(buf,size);
			std::size_t pos_captioneof = httpheader.find("\r\n");
			if(pos_captioneof == httpheader.npos) return false;
			lyramilk::data::string caption = httpheader.substr(0,pos_captioneof);
			lyramilk::data::string mimeheader = httpheader.substr(pos_captioneof+2);
			lyramilk::teapoy::strings fields = lyramilk::teapoy::split(caption," ");

			if(fields.size() != 3){
				s = sbad;
				return true;
			}
			method = fields[0];
			url  = fields[1];
			std::size_t pos = url.find("?");
			if(pos != url.npos && pos < url.size()){
				url_pure = lyramilk::data::codes::instance()->decode("url",url.substr(0,pos));
			}else{
				url_pure = lyramilk::data::codes::instance()->decode("url",url);
			}

			lyramilk::data::string verstr = fields[2];
			if(verstr.compare(0,5,"HTTP/") != 0){
				s = sbad;
				return true;
			}
			verstr = verstr.substr(5);
			std::size_t pos_verdot = verstr.find('.');
			if(pos_verdot == verstr.npos){
				ver.major = atoi(verstr.c_str());
				ver.minor = 0;
			}else{
				ver.major = atoi(verstr.substr(0,pos_verdot).c_str());
				ver.minor = atoi(verstr.substr(pos_verdot+1).c_str());
			}
			s = smime;
			return mime::parse(mimeheader.c_str(),mimeheader.size(),remain);
		}
		return mime::parse(buf,size,remain);
	}

	bool request::bad()
	{
		if(mime::bad()){
			return true;
		}
		return s == sbad;
	}

	void request::reset()
	{
		s = s0;
		parse_status = 0;
		httpheader.clear();
		ver.major = 0;
		ver.minor = 0;
		method.clear();
		url.clear();
		parameter.clear();
		cookies.clear();


		mime::reset();
	}

	bool request::parse_url()
	{
		if(parse_status&PARSE_STATUS_URL_PARAM) return true;
		std::size_t pos = url.find("?");
		lyramilk::data::string urlparams;
		if(pos != url.npos && pos < url.size()){
			lyramilk::data::string url1 = url.substr(0,pos);
			urlparams = url.substr(pos + 1);
			url_pure = lyramilk::data::codes::instance()->decode("url",url1) + "?" + urlparams;
		}else{
			url_pure = lyramilk::data::codes::instance()->decode("url",url);
		}

		lyramilk::teapoy::strings param_fields = lyramilk::teapoy::split(urlparams,"&");
		lyramilk::teapoy::strings::iterator it = param_fields.begin();

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

			lyramilk::data::var& parameters_of_key = parameter[k];
			parameters_of_key.type(lyramilk::data::var::t_array);
			lyramilk::data::var::array& ar = parameters_of_key;
			ar.push_back(v);
		}
		parse_status |= PARSE_STATUS_URL_PARAM;
		return true;
	}

	bool request::parse_body_param()
	{
		if(parse_status&PARSE_STATUS_BODY_PARAM) return true;
		lyramilk::data::var vmimetype = get("Content-Type");
		lyramilk::data::string urlparams;
		//解析请求正文中的参数
		if(vmimetype.type_like(lyramilk::data::var::t_str)){
			lyramilk::data::string str = vmimetype;
			lyramilk::data::string charset;
			if(body_length > 0 && str.find("www-form-urlencoded") != str.npos){
				std::size_t pos_charset = str.find("charset=");
				if(pos_charset!= str.npos){
					std::size_t pos_charset_bof = pos_charset + 8;
					std::size_t pos_charset_eof = str.find(";",pos_charset_bof);
					if(pos_charset_eof != str.npos){
						charset = str.substr(pos_charset_bof,pos_charset_eof - pos_charset_bof);
					}else{
						charset = str.substr(pos_charset_bof);
					}
				}

				lyramilk::data::string paramurl((const char*)offset_body,body_length);
				if(charset.find_first_not_of("UuTtFf-8") != charset.npos){
					paramurl = lyramilk::data::codes::instance()->decode(charset,paramurl);
				}
				if(!urlparams.empty()){
					urlparams.push_back('&');
				}
				urlparams += paramurl;
			}
		}

		lyramilk::teapoy::strings param_fields = lyramilk::teapoy::split(urlparams,"&");
		lyramilk::teapoy::strings::iterator it = param_fields.begin();

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

			lyramilk::data::var& parameters_of_key = parameter[k];
			parameters_of_key.type(lyramilk::data::var::t_array);
			lyramilk::data::var::array& ar = parameters_of_key;
			ar.push_back(v);
		}

		parse_status |= PARSE_STATUS_BODY_PARAM;
		return true;
	}

	bool request::parse_cookies()
	{
		if(parse_status&PARSE_STATUS_COOKIE) return true;
		lyramilk::data::string cookiestr = get("cookie");

		lyramilk::teapoy::strings vheader = lyramilk::teapoy::split(cookiestr,";");
		lyramilk::teapoy::strings::iterator it = vheader.begin();
		for(;it!=vheader.end();++it){
			std::size_t pos = it->find_first_of("=");
			if(pos != it->npos){
				lyramilk::data::string key = lyramilk::teapoy::trim(it->substr(0,pos)," \t\r\n");
				lyramilk::data::string value = lyramilk::teapoy::trim(it->substr(pos + 1));
				cookies[key] = value;
			}
		}
		parse_status |= PARSE_STATUS_COOKIE;
		return true;
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

	void make_response_header(std::ostream& os,const char* retcodemsg,bool has_header_ending,int httpver_major,int httpver_minor)
	{
		os <<	"HTTP/" << httpver_major << "." << httpver_minor << " " << retcodemsg << "\r\n"
				"Server: " TEAPOY_VERSION "\r\n";
		if(has_header_ending) os << "\r\n";
	}

}}}
