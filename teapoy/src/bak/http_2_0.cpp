#include "http_2_0.h"
#include "http_transaction.h"
#include <libmilk/log.h>
#include <libmilk/multilanguage.h>
#include <openssl/ssl.h>


namespace lyramilk{ namespace teapoy {

	ssize_t http_2_0::send_callback(nghttp2_session *session, const uint8_t *data,size_t length, int flags, void *user_data)
	{
		http_2_0* p = (http_2_0*)user_data;
		return SSL_write((SSL*)p->adapter->get_ssl_obj(),data,length);
	}

	ssize_t http_2_0::recv_callback(nghttp2_session *session, uint8_t *buf,size_t length, int flags, void *user_data)
	{
		http_2_0* p = (http_2_0*)user_data;
		int r = SSL_read((SSL*)p->adapter->get_ssl_obj(),buf,length);
		if(r == -1){
			int err = SSL_get_error((SSL*)p->adapter->get_ssl_obj(), r);
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
		if(frame->hd.type == NGHTTP2_HEADERS){
			p->hasdata = true;
		}
COUT << (unsigned int)frame->hd.type << "," << NGHTTP2_HEADERS << std::endl;
		return 0;
	}

	int http_2_0::on_header_callback(nghttp2_session *session,const nghttp2_frame *frame,const uint8_t *name, size_t namelen,const uint8_t *value, size_t valuelen,uint8_t flags, void *user_data)
	{
		if (frame->hd.type == NGHTTP2_HEADERS){
			http_2_0* p = (http_2_0*)user_data;
			p->request->header[lyramilk::data::string((const char*)name, namelen)] = lyramilk::data::string((const char*)value, valuelen);
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
		p->request->header.clear();

		p->hasdata = false;
		return 0;
	}

	int http_2_0::on_data_chunk_recv_callback(nghttp2_session *session,uint8_t flags, int32_t stream_id,const uint8_t *data, size_t len,void *user_data)
	{
		http_2_0* p = (http_2_0*)user_data;
		p->request->write((const char*)data,len);
		return 0;
	}

	ssize_t http_2_0::data_prd_read_callback(nghttp2_session *session, int32_t stream_id, uint8_t *buf, size_t length,uint32_t *data_flags, nghttp2_data_source *source, void *user_data)
	{
		http_2_0* p = (http_2_0*)user_data;

		p->oss.read((char*)buf,length);

		if(!p->oss.rdbuf()->in_avail()){
			*data_flags |= NGHTTP2_DATA_FLAG_EOF;
		}
		return p->oss.gcount();;
/*
		p->response->read((char*)buf,length);

		if(!p->response_body.rdbuf()->in_avail()){
			*data_flags |= NGHTTP2_DATA_FLAG_EOF;
		}
		return p->response_body.gcount();*/
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



	static httpchannel* ctr()
	{
		return new http_2_0();
	}

	static void dtr(httpchannel* ptr)
	{
		delete (http_2_0*)ptr;
	}

	httpchannel_creater http_2_0::proto_info = {"h2",lyramilk::teapoy::ctr,lyramilk::teapoy::dtr};

	http_2_0::http_2_0()
	{
		hasdata = false;
		h2_session = nullptr;
	}

	http_2_0::~http_2_0()
	{
		if(h2_session){
			nghttp2_session_del(h2_session);
		}
	}

	void http_2_0::dtr()
	{
		lyramilk::teapoy::dtr((httpchannel*)this);
	}

	bool http_2_0::oninit(std::ostream& os)
	{
		nghttp2_session_server_new(&h2_session,get_nghttp2_callbacks(),this);
		nghttp2_submit_settings(h2_session, NGHTTP2_FLAG_NONE, NULL, 0);
		return true;
	}

	bool http_2_0::onrequest(const char* cache,int size,std::ostream& os)
	{
		nghttp2_session_mem_recv(h2_session,(const unsigned char*)cache,size);
COUT << lyramilk::data::var(request->header)  << std::endl;

		static int i=0;
		{
			oss.str("");
			oss.clear();
			oss << "<html><head>http2测试</head><body>HTTP2 OK " << ++i << "</body></html>\n";

/*
			response_body.str("");
			response_body.clear();
			response_body << bodystr.str();*/

			lyramilk::data::var::map vm;
			vm[":status"] = "200";
			vm["Content-Length"] = oss.str().size();
			vm["Content-Type"] = "text/html; charset=utf-8";
			std::tr1::unordered_map<lyramilk::data::string,lyramilk::data::string> m;
			{
				lyramilk::data::var::map::iterator it = vm.begin();
				for(;it!=vm.end();++it){
					m[it->first] = it->second.str();
				}
			}

			std::vector<nghttp2_nv> headers;
			std::tr1::unordered_map<lyramilk::data::string,lyramilk::data::string>::iterator it = m.begin();
			for(;it!=m.end();++it){
				nghttp2_nv nv;
				nv.name = (unsigned char*)it->first.c_str();
				nv.namelen = it->first.size();
				nv.value = (unsigned char*)it->second.c_str();
				nv.valuelen = it->second.size();
				nv.flags = NGHTTP2_NV_FLAG_NONE;
				headers.push_back(nv);
			}

			nghttp2_data_provider data_prd;
			data_prd.read_callback = data_prd_read_callback;
			if(oss.str().empty()){
				nghttp2_submit_response(h2_session, stream_id, headers.data(),headers.size(), nullptr);
			}else{
				nghttp2_submit_response(h2_session, stream_id, headers.data(),headers.size(), &data_prd);
			}


			int r = nghttp2_session_send(h2_session);
			if(r<0){
				lyramilk::klog(lyramilk::log::warning,"mpush.aioapnspusher.notify_out") << lyramilk::kdict("HTTP2错误:%s",nghttp2_strerror(r)) << std::endl;
				return false;
			}
		}
		return true;
	}


}}
