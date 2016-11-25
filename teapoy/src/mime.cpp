#include "mime.h"
#include "stringutil.h"
#include <strings.h>
#include <libmilk/exception.h>
#include <libmilk/multilanguage.h>
#include <libmilk/log.h>
#include <stdio.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <algorithm>

#define LENGTH_OF_MEMCACHE (10*1024*1024)

namespace lyramilk{ namespace teapoy {
	const static lyramilk::data::string mime_boundary = "boundary=";

	mime::mime()
	{
		tmpbodyfile = nullptr;
		offset_body = nullptr;
		reset();
	}

	mime::~mime()
	{
		if(tmpbodyfile){
			fclose(tmpbodyfile);
			tmpbodyfile = nullptr;
		}
	}

	bool mime::parsebody(const char* buf,int size,int* remain)
	{
		if(usefilecache){
			if(!tmpbodyfile){
				tmpbodyfile = tmpfile();
			}
			if(fwrite(buf,size,1,tmpbodyfile) == 1){
				if(sbody == s){
					fpos_t size;
					fgetpos(tmpbodyfile,&size);
					rlength_body = size.__pos;
					if(rlength_body < body_length){
						return false;
					}

					int fd = fileno(tmpbodyfile);

					offset_body = mmap(nullptr,rlength_body,PROT_READ | PROT_WRITE,MAP_PRIVATE,fd,0);
					if(!offset_body){
						s = sbad;
						lyramilk::klog(lyramilk::log::error,"lyramilk.teapoy.mime.parsebody") << D("映射缓冲区错误：%s",strerror(errno)) << std::endl;
						return true;
					}

					if(parsebodydata((const char*)offset_body,rlength_body)){
						s = s0;
					}else{
						s = sbad;
					}

					if(remain) *remain = rlength_body - body_length;
					return true;
				}


			}else{
				s = sbad;
				lyramilk::klog(lyramilk::log::error,"lyramilk.teapoy.mime.parsebody") << D("写入临时文件失败：%s",strerror(errno)) << std::endl;
				return true;
			}
		}else{
			if(sbody == s){
				bodycache.append(buf,size);
				rlength_body = bodycache.size();
				offset_body = bodycache.c_str();

				if(rlength_body < body_length){
					return false;
				}

				if(parsebodydata((const char*)offset_body,body_length)){
					s = s0;
				}else{
					s = sbad;
				}
				if(remain) *remain = rlength_body - body_length;
				/*
				if(tmpbodyfile){
					fclose(tmpbodyfile);
					tmpbodyfile = nullptr;
				}*/
				return true;
			}else{

			}
		}
		COUT.write(buf,size).flush();
		return false;
	}

	struct tmpstring
	{
		const char* ptr;
		lyramilk::data::int64 len;
	};

	bool mime::parsebodydata(const char* buf,int size)
	{
		if(!boundary.empty() && ismultipart){
			lyramilk::data::string boundary1;
			boundary1.reserve(boundary.size()+2);
			boundary1.push_back('-');
			boundary1.push_back('-');
			boundary1.append(boundary);

			const char* boundary_str = boundary1.c_str();
			lyramilk::data::int64 boundary_len = boundary1.size();

			const char* p = buf;
			const char* e = buf + size - boundary1.size() - 2;
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
					childs.push_back(mime());
					mime& m = childs.back();
					m.parsepart(s.ptr,s.len);
				}
			}
			
			return true;
		}else{
			return true;
		}
	}

	void mime::parsepart(const char* buf,int size)
	{
		const char* headereof = strstr(buf,"\r\n\r\n");
		ptr_part = buf;
		len_part = size;
		ptr_body = headereof + 4;
		lyramilk::data::string mimeheader(buf,headereof-buf);
		parsemimeheader(mimeheader);
		s = spart;
	}
	
	void mime::parsemimeheader(const lyramilk::data::string& mimeheaderstr)
	{
		lyramilk::teapoy::strings vheader = lyramilk::teapoy::split(mimeheaderstr,"\r\n");
		lyramilk::teapoy::strings::iterator it = vheader.begin();
		lyramilk::data::var* laststr = nullptr;
		for(;it!=vheader.end();++it){
			std::size_t pos = it->find_first_of(":");
			if(pos != it->npos){
				lyramilk::data::string key = lyramilk::teapoy::lowercase(lyramilk::teapoy::trim(it->substr(0,pos)," \t\r\n"));

				lyramilk::data::string value = lyramilk::teapoy::trim(it->substr(pos + 1));
				laststr = &(header[key] = value);
			}else{
				if(laststr){
					(*laststr) += *it;
				}
			}
		}

	}

	bool mime::parse(const char* buf,int size,int* remain)
	{
		if(s == s0){
			mimeheader.append(buf,size);
			if(mimeheader == "\r\n") return true;
			std::size_t pos_header = mimeheader.find("\r\n\r\n");
			if(pos_header == mimeheader.npos) return false;
			lyramilk::data::string body = mimeheader.substr(pos_header + 4);
			mimeheader = mimeheader.substr(0,pos_header);
			parsemimeheader(mimeheader);

			lyramilk::data::var& cl = operator[]("Content-Length");
			if(cl != lyramilk::data::var::nil){
				body_length = cl;
				if(body_length > LENGTH_OF_MEMCACHE){
					usefilecache = true;
				}
				s = sbody;
			}else if(get("Transfer-Encoding") == "chunked"){
				TODO();
				s = schunked;
				usefilecache = true;
			}else{
				s = s0;
				body_length = 0;
			}

			if(s == sbody || s == schunked){
				lyramilk::data::var& ct = operator[]("Content-Type");
				if(ct != lyramilk::data::var::nil){
					lyramilk::data::string ct_str = ct.str();
					std::size_t pos_boundary = ct_str.find(mime_boundary);
					if(pos_boundary!=ct_str.npos){
						ismultipart = true;
						boundary = lyramilk::teapoy::lowercase(ct_str.substr(pos_boundary  + mime_boundary.size()));
					}
				}
				if(!body.empty()){
					return parsebody(body.c_str(),body.size(),remain);
				}
				return s == sbody && body_length == 0;
			}

			return true;
		}
		return parsebody(buf,size,remain);
	}

	bool mime::bad()
	{
		return s == sbad;
	}

	void mime::reset()
	{
		s = s0;
		mimeheader.clear();
		header.clear();
		ismultipart = false;
		usefilecache = false;
		bodycache.clear();
		boundary.clear();
		body_length = 0;
		childs.clear();

		if(tmpbodyfile){
			if(offset_body){
				munmap((void*)offset_body,rlength_body);
			}
			fclose(tmpbodyfile);
			tmpbodyfile = nullptr;
		}
		offset_body = nullptr;
		rlength_body = 0;


		ptr_part = nullptr;
		len_part = 0;
		ptr_body = nullptr;
	}

	lyramilk::data::var mime::get(lyramilk::data::string k) const
	{
		lyramilk::data::string key = lyramilk::teapoy::lowercase(k);
		lyramilk::data::var::map::const_iterator it = header.find(key);
		if(it!=header.end()){
			return it->second;
		}
		return lyramilk::data::var::nil;
	}

	lyramilk::data::var& mime::operator[](lyramilk::data::string k)
	{
		lyramilk::data::string key = lyramilk::teapoy::lowercase(k);
		lyramilk::data::var::map::iterator it = header.find(key);
		if(it!=header.end()){
			return it->second;
		}
		static lyramilk::data::var tmpvar;
		tmpvar = lyramilk::data::var::nil;
		return tmpvar;
	}

	void mime::set(lyramilk::data::string k,const lyramilk::data::var& v)
	{
		TODO();
	}

	const char* mime::getbodyptr()
	{
		return ptr_body;
	}

	lyramilk::data::int64 mime::getbodylength()
	{
		return ptr_part + len_part - ptr_body;
	}

	int mime::getmultipartcount() const
	{
		return childs.size();
	}

	mime& mime::selectpart(int index)
	{
		return childs[index];
	}

	const char* mime::getpartptr()
	{
		return ptr_part;
	}

	lyramilk::data::int64 mime::getpartlength()
	{
		return len_part;
	}
}}
