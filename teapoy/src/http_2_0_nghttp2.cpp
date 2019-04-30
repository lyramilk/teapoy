#include "http_2_0_nghttp2.h"
#include <libmilk/log.h>
#include <libmilk/dict.h>
#include <openssl/ssl.h>
#include "stringutil.h"
#include "httplistener.h"
#include "url_dispatcher.h"
#include "fcache.h"

#include <errno.h>

namespace lyramilk{ namespace teapoy {

	ssize_t http_2_0::send_callback(nghttp2_session *session, const uint8_t *data,size_t length, int flags, void *user_data)
	{
		http_2_0* p = (http_2_0*)user_data;

		size_t cur = 0;
		while(cur < length){
			int r = SSL_write((SSL*)p->channel->get_ssl_obj(),data + cur,length - cur);
			if(r == -1){
				if(errno == EAGAIN ) break;
				if(errno == EINTR) break;
			}
			if(r > 0){
				cur += r; 
			}
		}
		return cur;
	}

	ssize_t http_2_0::recv_callback(nghttp2_session *session, uint8_t *buf,size_t length, int flags, void *user_data)
	{
		http_2_0* p = (http_2_0*)user_data;
		int r = SSL_read((SSL*)p->channel->get_ssl_obj(),buf,length);
		if(r == -1){
			int err = SSL_get_error((SSL*)p->channel->get_ssl_obj(), r);
			if (err == SSL_ERROR_WANT_WRITE || err == SSL_ERROR_WANT_READ) {
				return NGHTTP2_ERR_WOULDBLOCK;
			}else{
				return NGHTTP2_ERR_CALLBACK_FAILURE;
			}
		}else if(r == 0){
			return NGHTTP2_ERR_EOF;
		}
		return r;
	}

	int http_2_0::on_frame_send_callback(nghttp2_session *session,const nghttp2_frame *frame,void *user_data)
	{
		return 0;
	}

	int http_2_0::on_frame_recv_callback(nghttp2_session *session,const nghttp2_frame *frame,void *user_data)
	{
		http_2_0* p = (http_2_0*)user_data;
		p->stream_id = frame->hd.stream_id;

		if(frame->hd.type == NGHTTP2_WINDOW_UPDATE){
			nghttp2_window_update* uf = (nghttp2_window_update*)frame;
			nghttp2_session_set_local_window_size(p->h2_session,NGHTTP2_FLAG_NONE,frame->hd.stream_id,uf->window_size_increment);
			nghttp2_session_send(p->h2_session);

		}

		if(frame->hd.flags&NGHTTP2_FLAG_END_HEADERS){
		}
		if(frame->hd.flags&NGHTTP2_FLAG_END_STREAM){
			p->response->set("Connection",p->request->get("Connection"));

			p->cookie_from_request();
			if(!p->service->call(p->request->get("Host"),p->request,p->response,p)){
				p->response->code = 404;
				p->response->content_length = 0;
				p->send_header();
			}
		
			std::vector<nghttp2_nv> headers;
			lyramilk::data::stringdict::iterator it;
			for(it = p->response_header.begin();it!=p->response_header.end();++it){
				if(it->first.compare(0,1,":") == 0){
					nghttp2_nv nv;
					nv.name = (unsigned char*)it->first.c_str();
					nv.namelen = it->first.size();
					nv.value = (unsigned char*)it->second.c_str();
					nv.valuelen = it->second.size();
					nv.flags = NGHTTP2_NV_FLAG_NONE;
					headers.push_back(nv);
				}
			}
			for(it = p->response_header.begin();it!=p->response_header.end();++it){
				if(it->first.compare(0,1,":") != 0){
					nghttp2_nv nv;
					nv.name = (unsigned char*)it->first.c_str();
					nv.namelen = it->first.size();
					nv.value = (unsigned char*)it->second.c_str();
					nv.valuelen = it->second.size();
					nv.flags = NGHTTP2_NV_FLAG_NONE;
					headers.push_back(nv);
				}
			}

			nghttp2_data_provider data_prd;
			data_prd.read_callback = data_prd_read_callback;

			if(p->response_body.str().empty()){
				nghttp2_submit_response(p->h2_session, frame->hd.stream_id, headers.data(),headers.size(), nullptr);
			}else{
				nghttp2_submit_response(p->h2_session, frame->hd.stream_id, headers.data(),headers.size(), &data_prd);
			}

			int r = nghttp2_session_send(p->h2_session);
			if(r<0){
				lyramilk::klog(lyramilk::log::warning,"teapoy.nghttp2.notify_out") << lyramilk::kdict("HTTP2错误:%s",nghttp2_strerror(r)) << std::endl;
				return NGHTTP2_ERR_EOF;
			}
		}
		return 0;
	}

	int http_2_0::on_header_callback(nghttp2_session *session,const nghttp2_frame *frame,const uint8_t *name, size_t namelen,const uint8_t *value, size_t valuelen,uint8_t flags, void *user_data)
	{
		if (frame->hd.type == NGHTTP2_HEADERS){
			http_2_0* p = (http_2_0*)user_data;
			lyramilk::data::string skey((const char*)name, namelen);
			lyramilk::data::string svalue((const char*)value, valuelen);
			p->request->header[lyramilk::teapoy::lowercase(skey)] = svalue;
			if(skey == ":authority"){
				p->request->header["host"] = svalue;
			}
		}
		return 0;
	}

	int http_2_0::on_begin_headers_callback(nghttp2_session *session,const nghttp2_frame *frame,void *user_data)
	{
		return 0;
	}

	int http_2_0::on_stream_close_callback(nghttp2_session *session, int32_t stream_id,uint32_t error_code,void *user_data)
	{
		http_2_0* p = (http_2_0*)user_data;
		lyramilk::data::string sconnect = p->response->get("Connection");
		if(sconnect == "close"){
			p->closeconnect = true;
		}else{
			p->reset();
		}
		return 0;
	}

	int http_2_0::on_data_chunk_recv_callback(nghttp2_session *session,uint8_t flags, int32_t stream_id,const uint8_t *data, size_t len,void *user_data)
	{
		http_2_0* p = (http_2_0*)user_data;
		int nouse = 0;
		p->request->pps = mp_head_end;
		p->request->write((const char*)data,len,&nouse);
		//nghttp2_session_consume_stream(p->h2_session,stream_id,???);
		//nghttp2_session_send(p->h2_session);

		return 0;
	}

	ssize_t http_2_0::data_prd_read_callback(nghttp2_session *session, int32_t stream_id, uint8_t *buf, size_t length,uint32_t *data_flags, nghttp2_data_source *source, void *user_data)
	{
		http_2_0* p = (http_2_0*)user_data;


		p->response_body.read((char*)buf,length);

		if(!p->response_body.rdbuf()->in_avail()){
			*data_flags |= NGHTTP2_DATA_FLAG_EOF;
		}
		return p->response_body.gcount();;
	}

	nghttp2_session_callbacks* http_2_0::get_nghttp2_callbacks()
	{
		static nghttp2_session_callbacks *h2_cbks = nullptr;
		if(h2_cbks) return h2_cbks;

		int r = nghttp2_session_callbacks_new(&h2_cbks);
		if(r != 0){
			lyramilk::klog(lyramilk::log::warning,"lyramilk.teapoy.http_2_0") << lyramilk::kdict("初始化nghttp2回调失败") << std::endl;
		}
		nghttp2_session_callbacks_set_send_callback(h2_cbks, send_callback);
		nghttp2_session_callbacks_set_recv_callback(h2_cbks, recv_callback);

		nghttp2_session_callbacks_set_on_frame_send_callback(h2_cbks, on_frame_send_callback);
		nghttp2_session_callbacks_set_on_frame_recv_callback(h2_cbks, on_frame_recv_callback);
		nghttp2_session_callbacks_set_on_header_callback(h2_cbks, on_header_callback);
		nghttp2_session_callbacks_set_on_begin_headers_callback(h2_cbks, on_begin_headers_callback);
		nghttp2_session_callbacks_set_on_stream_close_callback(h2_cbks, on_stream_close_callback);
		nghttp2_session_callbacks_set_on_data_chunk_recv_callback(h2_cbks, on_data_chunk_recv_callback);
		return h2_cbks;
	}


	static httpadapter* ctr(std::ostream& oss)
	{
		return new http_2_0(oss);
	}

	static void dtr(httpadapter* ptr)
	{
		delete (http_2_0*)ptr;
	}

	httpadapter_creater http_2_0::proto_info = {"h2",lyramilk::teapoy::ctr,lyramilk::teapoy::dtr};


	http_2_0::http_2_0(std::ostream& oss):httpadapter(oss)
	{
		h2_session = nullptr;
		stream_id = 0;
		closeconnect = false;
	}

	http_2_0::~http_2_0()
	{
		if(h2_session){
			nghttp2_session_del(h2_session);
		}
	}

	void http_2_0::dtr()
	{
		lyramilk::teapoy::dtr((httpadapter*)this);
	}

	bool http_2_0::oninit(std::ostream& os)
	{
		nghttp2_session_server_new(&h2_session,get_nghttp2_callbacks(),this);
		nghttp2_submit_settings(h2_session, NGHTTP2_FLAG_NONE, NULL, 0);
		return reset();
	}

	bool http_2_0::onrequest(const char* cache,int size,std::ostream& os)
	{
		nghttp2_session_mem_recv(h2_session,(const unsigned char*)cache,size);
		return !closeconnect;
	}

	bool http_2_0::reset()
	{
		response_header.clear();
		response_cookies.clear();
		response_body.str("");
		response_body.clear();
		return request->reset() && response->reset() && httpadapter::reset();
	}

	bool http_2_0::call()
	{
		cookie_from_request();
		if(!service->call(request->get("Host"),request,response,this)){
			response->code = 404;
			response->content_length = 0;
			send_header();
		}
		return true;
	}

	http_2_0::send_status http_2_0::send_header()
	{
		lyramilk::data::ostringstream os;
		errorpage* page = errorpage_manager::instance()->get(response->code);
		if(page){
			response_header[":status"] = lyramilk::data::str(page->code);
			response_header["Content-Length"] = lyramilk::data::str(page->body.size());
		}else{
			response_header[":status"] = lyramilk::data::str(response->code);
			if(response->content_length == -1){
				response_header["Transfer-Encoding"] = "chunked";
			}else{
				response_header["Content-Length"] = lyramilk::data::str(response->content_length);
			}
		}


		merge_cookies();
		{
			http_header_type::const_iterator it = response->header.begin();
			for(;it!=response->header.end();++it){
				if(!it->second.empty()){
					response_header[it->first] = it->second;
				}
			}
		}
		{
			std::multimap<lyramilk::data::string,lyramilk::data::string>::const_iterator it = response->header_ex.begin();
			for(;it!=response->header_ex.end();++it){
				if(!it->second.empty()){
					response_header[it->first] = it->second;
				}
			}
		}
COUT << response_header << std::endl;
		return ss_need_body;

	}


	bool http_2_0::send_data(const char* p,lyramilk::data::uint32 l)
	{
		response_body.write(p,l);
		return true;
	}

	void http_2_0::send_finish()
	{
		
	}

	bool http_2_0::allow_gzip()
	{
		return true;
	}

	bool http_2_0::allow_chunk()
	{
		return true;
	}

	bool http_2_0::allow_cached()
	{
		return true;
	}

}}
