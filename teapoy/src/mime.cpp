#include "mime.h"
#include "stringutil.h"
#include "fcache.h"
#include <string.h>
#include <stdlib.h>
#include <libmilk/debug.h>
#include <libmilk/codes.h>

namespace lyramilk{ namespace teapoy {

	mime::mime()
	{
		request_cache = nullptr;
		reset();
	}

	mime::~mime()
	{
		if(request_cache){
			delete request_cache;
		}
		mimes::const_iterator it = childs.begin();
		for(;it!=childs.end();++it){
			delete *it;
		}
		childs.clear();
	}

	bool mime::reset()
	{
		content_length = 0;
		if(request_cache){
			delete request_cache;
		}
		request_cache = new fcache;
		pps = mp_http_s0;
		body_ptr = nullptr;
		body_offset = 0;
		body_size = 0;

		mimes::const_iterator it = childs.begin();
		for(;it!=childs.end();++it){
			delete *it;
		}
		childs.clear();
		header.clear();
		return true;
	}


	static const char* memfind(const char* p,std::size_t l,const char* d,std::size_t dl)
	{
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

	mime_status mime::parse(const char* base,std::size_t base_size,std::size_t* bytes_used)
	{
		if(pps == mp_http_s0 || pps == mp_head){
			const char* p = base;

			const char* sep = memfind(p,base_size,"\r\n\r\n",4);
			if(sep == nullptr) return ms_continue;

			lyramilk::data::string httpheader;
			if(pps == mp_http_s0){
				const char* cacheheader = p;
				std::size_t header_size = sep - p;
				body_offset = header_size + 4;

				const char* cap_eof = memfind(cacheheader,header_size,"\r\n",2);
				if(!cap_eof) return ms_error;


				lyramilk::data::string httpcaption(cacheheader,cap_eof - cacheheader);
				httpheader.assign(cap_eof+2,cacheheader + header_size - cap_eof - 2);
				lyramilk::data::strings fields = lyramilk::teapoy::split(httpcaption," ");
				if(fields.size() != 3){
					return ms_error;
				}

				header[":method"] = fields[0];
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
				header[":path"] = path;	//url
				header[":version"] = fields[2];
				pps = mp_head;
			}else{
				const char* cacheheader = p;
				std::size_t header_size = sep - p;
				body_offset = header_size + 4;
				httpheader.assign(cacheheader,sep - cacheheader);
			}

			lyramilk::data::strings vheader = lyramilk::teapoy::split(httpheader,"\r\n");
			lyramilk::data::strings::iterator it = vheader.begin();
			lyramilk::data::string* laststr = nullptr;
			for(;it!=vheader.end();++it){
				std::size_t pos = it->find_first_of(":");
				if(pos != it->npos){
					lyramilk::data::string key =lyramilk::teapoy::trim(it->substr(0,pos)," \t\r\n");

					lyramilk::data::string value = lyramilk::teapoy::trim(it->substr(pos + 1)," \t\r\n");
					if(!key.empty() && !value.empty()){
						laststr = &(header[key] = value);
					}
				}else{
					if(laststr){
						(*laststr) += *it;
					}
				}
			}
			pps = mp_head_end;
		}
		if(pps == mp_head_end){
			{
				http_header_type::const_iterator it = header.find("transfer-encoding");
				if(it != header.end() && it->second == "chunked"){
					pps = mp_chunked;
				}
			}
			{
				http_header_type::const_iterator it = header.find("content-length");
				if(it != header.end()){
					char* p;
					content_length = strtoll(it->second.c_str(),&p,10);
				}else{
					content_length = 0;
				}
				pps = mp_lengthed;
			}
		}

		if(pps == mp_lengthed){
			if(base_size - body_offset >= content_length){
				if(bytes_used){
					*bytes_used = body_offset + content_length;
				}
				body_ptr = base + body_offset;
				body_size = base_size- body_offset;
				return ms_success;
			}
			return ms_continue;
		}else if(pps == mp_chunked){
		}else if(pps == mp_error){
			return ms_error;
		}
		return ms_error;
	}

	mime_status mime::write(const char* cache,int cache_size,int* cache_used_size)
	{
		request_cache->write(cache,cache_size);
		std::size_t bytes_total = request_cache->length();
		std::size_t bytes_used = 0;
		mime_status ms = parse(request_cache->data(),bytes_total,&bytes_used);
		if(ms == ms_success){
			if(cache_used_size){
				*cache_used_size = cache_size - (bytes_total - bytes_used);
			}

			if(body_ptr && body_size > 0){
				//解析文件
				{
					lyramilk::data::string s_mimetype = get("Content-Type");
					lyramilk::data::string boundary;
					if(s_mimetype.find("multipart/form-data") != s_mimetype.npos){
						std::size_t pos_boundary = s_mimetype.find("boundary=");
						if(pos_boundary!= s_mimetype.npos){
							std::size_t pos_boundary_bof = pos_boundary + 9;
							std::size_t pos_boundary_eof = s_mimetype.find(";",pos_boundary_bof);
							if(pos_boundary_eof != s_mimetype.npos){
								boundary = s_mimetype.substr(pos_boundary_bof,pos_boundary_eof - pos_boundary_bof);
							}else{
								boundary = s_mimetype.substr(pos_boundary_bof);
							}
						}
					}

					lyramilk::data::string boundary1 = "--";
					boundary1.append(boundary);
					boundary1.push_back('\r');
					boundary1.push_back('\n');

					lyramilk::data::string boundary2 = "--";
					boundary2.append(boundary);
					boundary2.push_back('-');
					boundary2.push_back('-');
					boundary2.push_back('\r');
					boundary2.push_back('\n');

					const char *p = body_ptr;
					const char *e = body_ptr + body_size;
	
					p = memfind(p,std::size_t(e - p),boundary1.c_str(),boundary1.size());
					while(p < e){
						if(p == nullptr) break;
						const char *header = p + boundary1.size();

						if(header + 2 >= e){
							break;
						}
						p += boundary1.size();
						const char *n = memfind(p,std::size_t(e - p),boundary1.c_str(),boundary1.size());
						if(n == nullptr){
							n = memfind(p,std::size_t(e - p),boundary2.c_str(),boundary2.size());
							if(n == nullptr){
								n = e;
							}
						}
						mime* ts = new mime();
						ts->pps = mp_head;
						std::size_t bs = (n-p);
						if(bs >= 2){
							ts->parse(p,bs - 2,nullptr);
							lyramilk::data::string cd = ts->get("Content-Disposition");
							lyramilk::data::strings cdinfos = split(cd,";"); 
							lyramilk::data::strings::iterator it = cdinfos.begin();
							for(;it!=cdinfos.end();++it){
								lyramilk::data::strings s = split(*it,"=");
								if(s.size() == 2){
									lyramilk::data::string key = "..form_data.";
									key += trim(s[0],",\"' ");
									ts->set(key,trim(s[1],",\"' "));
								}
							}
						}
						childs.push_back(ts);
						p = n;
					}
				}
			}
		}
		return ms;
	}

	const mime::mimes& mime::get_childs_obj() const
	{
		return childs;
	}

	std::size_t mime::get_childs_count() const
	{
		return childs.size();
	}

	mime* mime::at(std::size_t idx)
	{
		if(idx < get_childs_count()){
			return childs[idx];
		}
		return nullptr;
	}

	unsigned char* mime::get_body_ptr()
	{
		return (unsigned char*)body_ptr;
	}

	std::size_t mime::get_body_size()
	{
		return body_size;
	}

	const lyramilk::data::case_insensitive_map& mime::get_header_obj() const
	{
		return header;
	}

	bool mime::empty() const
	{
		return header.empty();
	}
	lyramilk::data::string mime::get(const lyramilk::data::string& field) const
	{
		http_header_type::const_iterator it = header.find(field);
		if(it != header.end()){
			return it->second;
		}
		return "";
	}

	void mime::set(const lyramilk::data::string& field,const lyramilk::data::string& value)
	{
		header[field] = value;
	}

}}
