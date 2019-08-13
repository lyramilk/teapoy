#ifndef _lyramilk_teapoy_mime_h_
#define _lyramilk_teapoy_mime_h_

#include "config.h"
#include <libmilk/var.h>

namespace lyramilk{ namespace teapoy {
	class fcache;
	typedef lyramilk::data::case_insensitive_map http_header_type;

	enum mime_status{
		ms_error = 0x0,
		ms_continue,
		ms_success,
	};

	enum mime_process{
		mp_unknow =	0x0,
		mp_head,
		mp_head_end,
		mp_error,
		mp_chunked,
		mp_lengthed,
		mp_http_s0,
	};

	class mime
	{
	  	std::size_t content_length;
	  	const char* body_ptr;
	  	std::size_t body_offset;
	  	std::size_t body_size;
	  public:
		typedef std::vector<mime*> mimes;
	  protected:
		fcache* request_cache;
		mime_process pps;
		mimes childs;
		http_header_type header;
		virtual mime_status parse(const char* base,std::size_t base_size,std::size_t* bytes_used);
	  public:
		mime();
	  	virtual ~mime();

		virtual bool reset();
		virtual mime_status write(const char* cache,int cahe_size,int* cahe_used_size);
	  public:
		virtual const mimes& get_childs_obj() const;
		virtual std::size_t get_childs_count() const;
		virtual mime* at(std::size_t idx);

		virtual unsigned char* get_body_ptr();
		virtual std::size_t get_body_size();
	  public:
		virtual const http_header_type& get_header_obj() const;
		virtual bool empty() const;
		virtual lyramilk::data::string get(const lyramilk::data::string& field) const;
		virtual void set(const lyramilk::data::string& field,const lyramilk::data::string& value);
	};




}}

#endif
