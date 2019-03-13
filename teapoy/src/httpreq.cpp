#include "http.h"
#include "web.h"
#include "stringutil.h"
#include <string.h>
#include <libmilk/codes.h>
#include <algorithm>

namespace lyramilk{ namespace teapoy {namespace http{
	const static lyramilk::data::string http_boundary = "boundary=";

	///	http_body
	http_body::http_body()
	{
		s_ptr = nullptr;
		s_len = 0;
	}
	http_body::~http_body()
	{
	}

	void http_body::reserve(lyramilk::data::uint64 sz)
	{
		s_cache.reserve(sz);
	}

	void http_body::writedata(const char* p,unsigned int sz)
	{
		s_cache.append(p,sz);
	}

	const char* http_body::ptr() const
	{
		if(!s_cache.empty()) return s_cache.c_str();
		return s_ptr;
	}

	lyramilk::data::uint64 http_body::size() const
	{
		if(!s_cache.empty()) return s_cache.size();
		return s_len;
	}
	///	http_chunkbody
	http_chunkbody::http_chunkbody()
	{
		reserve(65536);
		block_cache.reserve(16384);
		block_size = 0;
		block_eof = 0;
	}

	http_chunkbody::~http_chunkbody()
	{}

	void http_chunkbody::release()
	{
		delete this;
	}

	bool http_chunkbody::write(const char* p,unsigned int sz,unsigned int* remain)
	{
		block_cache.append(p,sz);

		while(block_cache.size() > block_size + block_eof){
			if(block_size > 0){
				writedata(block_cache.c_str() + block_eof,block_size);
				block_cache = block_cache.substr(block_eof + block_size);
				block_size = 0;
			}else if(block_eof > 0){
				//最后一个包的结尾\r\n被分割到一个单独数据包中的情况。
				if(remain)*remain = block_eof;
				block_cache.clear();
				return true;
			}else{
				std::size_t pos = block_cache.find("\r\n");
				if(pos == block_cache.npos) return false;
				block_size = strtoll(block_cache.c_str(),nullptr,16);
				if(block_size == 0){
					//如果所有包都取完了，最后却少了\r\n结尾，可能这个\r\n被单独分到了一个数据包中。这时，需要继续接收数据把最后两个字节除掉，不让它计入下一个请求。
					if(block_cache.size() > 2){
						if(remain) *remain = block_cache.size() - 2;
						return true;
					}
				}
				block_cache = block_cache.substr(pos + 2);
				block_eof = 2;
			}
		}
		return false;
	}

	bool http_chunkbody::ok() const
	{
		return true;
	}

	///	http_lengthedbody
	http_lengthedbody::http_lengthedbody(lyramilk::data::uint64 len)
	{
		totalbytes = len;
		reserve(totalbytes);
	}

	http_lengthedbody::~http_lengthedbody()
	{}

	void http_lengthedbody::release()
	{
		delete this;
	}

	bool http_lengthedbody::write(const char* p,unsigned int sz,unsigned int* remain)
	{
		if(totalbytes >= sz){
			totalbytes -= sz;
			writedata(p,sz);
			return totalbytes == 0;
		}else if(totalbytes > 0){
			if(remain){
				*remain = sz - (int)totalbytes;
			}
			totalbytes = 0;
			writedata(p,totalbytes);
		}
		return true;
	}

	bool http_lengthedbody::ok() const
	{
		return true;
	}


	///	http_refbody
	http_refbody::http_refbody(const char* pbody,lyramilk::data::uint64 bodylength)
	{
		s_ptr = pbody;
		s_len = bodylength;
	}

	http_refbody::~http_refbody()
	{}

	void http_refbody::release()
	{
		delete this;
	}

	bool http_refbody::write(const char* p,unsigned int sz,unsigned int* remain)
	{
		return true;
	}

	bool http_refbody::ok() const
	{
		return true;
	}

	///	http_resource
	http_resource::http_resource()
	{
		body = nullptr;
		pchilds = nullptr;
	}

	http_resource::~http_resource()
	{
		if(body)body->release();
		if(pchilds) delete pchilds;
	}


	void http_resource::init_body()
	{
	}

	long http_resource::childcount()
	{
		init_body();
		if(!pchilds) return 0;
		return pchilds->size();
	}

	http_resource* http_resource::child(long index)
	{
		init_body();
		if(!pchilds) return nullptr;
		return &pchilds->at(index);
	}

	/// http_frame

	http_frame::http_frame(request* args)
	{
		req = args;
		cookies_inited = false;
		params_inited = false;
		body_inited = false;
	}

	inline const char* strnstr(const char* str1,const char* str2,long long nstr1)
	{
		int n = strlen(str2);
		long long sz = nstr1 - n;
		for(long long i=0;i<sz;++i){
			if(str1[i] == str2[0]){
				if(0 == memcmp(str1+i,str2,n)) return str1 + i;
			}
		}
		return nullptr;
	}

	bool http_frame::parse(const char* p,long long sz)
	{
		lyramilk::data::string caption;
		const char* cap_eof = strnstr(p,"\r\n",sz);
		if(!cap_eof) return false;
		caption.assign(p,cap_eof-p);
		cap_eof += 2;	//	跳过crlf

		lyramilk::data::strings fields = lyramilk::teapoy::split(caption," ");

		if(fields.size() != 3){
			return false;
		}
		method = fields[0];
		lyramilk::data::string path = fields[1];
		{
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
		}
		rawuri = path;	//url

		lyramilk::data::string verstr = fields[2];
		if(verstr.compare(0,5,"HTTP/") != 0){
			return false;
		}
		verstr = verstr.substr(5);
		std::size_t pos_verdot = verstr.find('.');
		if(pos_verdot == verstr.npos){
			major = atoi(verstr.c_str());
			minor = 0;
		}else{
			major = atoi(verstr.substr(0,pos_verdot).c_str());
			minor = atoi(verstr.substr(pos_verdot+1).c_str());
		}

		return mime::parse(cap_eof,p+sz-cap_eof);
	}

	bool http_frame::ok()
	{
		return true;
	}

	void http_frame::invalid_params()
	{
		_params.clear();
		params_inited = false;
	}

	lyramilk::data::map& http_frame::cookies()
	{
		if(cookies_inited) return _cookies; 
		lyramilk::data::string cookiestr = get("cookie");

		lyramilk::data::strings vheader = lyramilk::teapoy::split(cookiestr,";");
		lyramilk::data::strings::iterator it = vheader.begin();
		for(;it!=vheader.end();++it){
			std::size_t pos = it->find_first_of("=");
			if(pos != it->npos){
				lyramilk::data::string key = lyramilk::teapoy::trim(it->substr(0,pos)," \t\r\n");
				lyramilk::data::string value = lyramilk::teapoy::trim(it->substr(pos + 1)," \t\r\n");
				_cookies[key] = value;
			}
		}
		cookies_inited = true;
		return _cookies;
	}

	lyramilk::data::map& http_frame::params()
	{
		if(params_inited) return _params; 
		lyramilk::data::string urlparams;

		std::size_t sz = uri.find("?");
		if(sz != uri.npos){
			urlparams = uri.substr(sz + 1);
		}

		//解析请求正文中的参数
		{
			lyramilk::data::string s_mimetype = get("Content-Type");
			lyramilk::data::string charset;
			if(body && body->size() > 0 && s_mimetype.find("www-form-urlencoded") != s_mimetype.npos){
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

				lyramilk::data::string paramurl((const char*)body->ptr(),body->size());
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
			lyramilk::data::array& ar = parameters_of_key;
			ar.push_back(v);
		}
		params_inited = true;
		return _params;
	}

	lyramilk::data::string http_frame::get_url()
	{
		std::size_t sz = uri.find("?");
		if(sz == uri.npos){
			return uri;
		}
		return uri.substr(0,sz);
	}


	struct tmpstring
	{
		const char* ptr;
		lyramilk::data::int64 len;
	};

	void http_frame::init_body()
	{
		if(body_inited) return; 
		body_inited = true;
		lyramilk::data::string ct_str = get("Content-Type");
		std::size_t pos_boundary = ct_str.find(http_boundary);
		if(pos_boundary!=ct_str.npos){
			//boundary = lyramilk::teapoy::lowercase(ct_str.substr(pos_boundary  + http_boundary.size()));
			boundary = ct_str.substr(pos_boundary  + http_boundary.size());

			lyramilk::data::string boundary1;
			boundary1.reserve(boundary.size()+2);
			boundary1.push_back('-');
			boundary1.push_back('-');
			boundary1.append(boundary);


			const char* boundary_str = boundary1.c_str();
			lyramilk::data::int64 boundary_len = boundary1.size();

			const char* p = body->ptr();
			const char* e = body->ptr() + body->size() - boundary1.size() - 2;
			std::vector<const char*> vp;
			const char* de = nullptr;
			for(;p < e;++p){
				int b = memcmp(p,boundary_str,boundary_len);
				if(b == 0){
					if(p[boundary_len+1] == '-' && p[boundary_len] == '-'){
						de = p;
						break;
					}
					vp.push_back(p);
				}
			}

			if(!vp.empty()){
				pchilds = new std::vector<http_resource>();
				pchilds->reserve(vp.size());
			}

			if(de){
				std::vector<tmpstring> vs;
				{
					std::vector<const char*>::reverse_iterator it = vp.rbegin();
					for(;it!=vp.rend();++it){
						tmpstring s;
						s.ptr = (*it) + boundary_len;
						s.len = de - s.ptr - 2;
						de = (*it);
						vs.push_back(s);
					}
					std::reverse(vs.begin(),vs.end());
				}

				std::vector<tmpstring>::iterator it = vs.begin();
				for(;it!=vs.end();++it){
					tmpstring& s = *it;
					const char* chilemime_eof = strnstr(s.ptr,"\r\n\r\n",s.len);
					if(!chilemime_eof) continue;
					chilemime_eof+= 4;

					pchilds->push_back(http_resource());
					http_resource& res = pchilds->back();

					res.parse(s.ptr,chilemime_eof - s.ptr);
					res.body = new http_refbody(chilemime_eof,s.ptr + s.len - chilemime_eof);
				}
			}

		}
	}

}}}
