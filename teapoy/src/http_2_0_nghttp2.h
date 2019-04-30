#ifndef _lyramilk_teapoy_http_2_0_h_
#define _lyramilk_teapoy_http_2_0_h_

#include "config.h"
#include "webservice.h"
#include <nghttp2/nghttp2.h>

#ifdef OPENSSL_FOUND
	#include <openssl/ssl.h>
#endif

namespace lyramilk{ namespace teapoy {

	class http_2_0:public httpadapter
	{
	  protected:
		bool closeconnect;
		int32_t stream_id;
		nghttp2_session *h2_session;
		static nghttp2_session_callbacks* get_nghttp2_callbacks();
		static ssize_t send_callback(nghttp2_session *session, const uint8_t *data,size_t length, int flags, void *user_data);
		static ssize_t recv_callback(nghttp2_session *session, uint8_t *buf,size_t length, int flags, void *user_data);
		static int on_frame_send_callback(nghttp2_session *session,const nghttp2_frame *frame,void *user_data);
		static int on_frame_recv_callback(nghttp2_session *session,const nghttp2_frame *frame,void *user_data);
		static int on_header_callback(nghttp2_session *session,const nghttp2_frame *frame,const uint8_t *name, size_t namelen,const uint8_t *value, size_t valuelen,uint8_t flags, void *user_data);
		static int on_begin_headers_callback(nghttp2_session *session,const nghttp2_frame *frame,void *user_data);
		static int on_stream_close_callback(nghttp2_session *session, int32_t stream_id,uint32_t error_code,void *user_data);
		static int on_data_chunk_recv_callback(nghttp2_session *session,uint8_t flags, int32_t stream_id,const uint8_t *data, size_t len,void *user_data);
		static ssize_t data_prd_read_callback(nghttp2_session *session, int32_t stream_id, uint8_t *buf, size_t length,uint32_t *data_flags, nghttp2_data_source *source, void *user_data);
	  public:
		http_2_0(std::ostream& oss);
		virtual ~http_2_0();

		virtual void dtr();

		virtual bool oninit(std::ostream& os);
		virtual bool onrequest(const char* cache,int size,std::ostream& os);

		virtual bool reset();
		virtual bool call();

	  public:
		virtual send_status send_header();
		virtual bool send_data(const char* p,lyramilk::data::uint32 l);
		virtual void send_finish();
	  public:
		virtual bool allow_gzip();
		virtual bool allow_chunk();
		virtual bool allow_cached();
	  public:
		lyramilk::data::stringdict response_header;
		lyramilk::data::strings response_cookies;
		std::stringstream response_body;
	  public:
		static httpadapter_creater proto_info;
	 
	};

}}

#endif
