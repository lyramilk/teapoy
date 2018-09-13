#include "http_2_0.h"
#include "http_2_0_hpack.h"

#include "httplistener.h"
#include "url_dispatcher.h"

#include <libmilk/log.h>
#include <libmilk/multilanguage.h>
#include <openssl/ssl.h>
#include <endian.h>


namespace lyramilk{ namespace teapoy {

	void inline dumpbytes(const void* pv,int size)
	{
		const unsigned char* p = (const unsigned char*)pv;
		for(int i=0;i<size;++i){
			printf("%02x ",(unsigned int)p[i]);
		}
		printf("\n");
	}

	enum h2_frame_type{
		H2_DATA = 0,
		H2_HEADERS = 0x01,
		H2_PRIORITY = 0x02,
		H2_RST_STREAM = 0x03,
		H2_SETTINGS = 0x04,
		H2_PUSH_PROMISE = 0x05,
		H2_PING = 0x06,
		H2_GOAWAY = 0x07,
		H2_WINDOW_UPDATE = 0x08,
		H2_CONTINUATION = 0x09,
		H2_ALTSVC = 0x0a,
		H2_MAGIC = 0x20,
	};

	enum h2_setting_header{
		H2_SETTINGS_HEADER_TABLE_SIZE		= 0x1,
		H2_SETTINGS_ENABLE_PUSH				= 0x2,
		H2_SETTINGS_MAX_CONCURRENT_STREAMS	= 0x3,
		H2_SETTINGS_INITIAL_WINDOW_SIZE		= 0x4,
		H2_SETTINGS_MAX_FRAME_SIZE			= 0x5,
		H2_SETTINGS_MAX_HEADER_LIST_SIZE	= 0x6,
	};

	const char* h2_frame_type_tocstr(h2_frame_type ftype)
	{
		switch(ftype){
		  case H2_DATA:
			return "DATA";
			break;
		  case H2_HEADERS:
			return "HEADERS";
			break;
		  case H2_PRIORITY:
			return "PRIORITY";
			break;
		  case H2_RST_STREAM:
			return "RST_STREAM";
			break;
		  case H2_SETTINGS:
			return "SETTINGS";
			break;
		  case H2_PUSH_PROMISE:
			return "PUSH_PROMISE";
			break;
		  case H2_PING:
			return "PING";
			break;
		  case H2_GOAWAY:
			return "GOAWAY";
			break;
		  case H2_WINDOW_UPDATE:
			return "WINDOW_UPDATE";
			break;
		  case H2_CONTINUATION:
			return "CONTINUATION";
			break;
		  case H2_ALTSVC:
			return "ALTSVC";
			break;
		  case H2_MAGIC:
			return "MAGIC";
			break;
		}
		return "UNKNOW";
	}


	struct h2_frame_header
	{
		unsigned int length:24;
		h2_frame_type type:8;
		union{
			unsigned char i;
			struct{
/*
				unsigned char reserve0:2;
				unsigned char proiority:1;
				unsigned char reserve1:1;
				unsigned char padded:1;
				unsigned char endheader:1;
				unsigned char reserve2:1;
				unsigned char endstream:1;
*/
				unsigned char endstream:1;
				unsigned char reserve2:1;
				unsigned char endheader:1;
				unsigned char padded:1;
				unsigned char reserve1:1;
				unsigned char proiority:1;
				unsigned char reserve0:2;
			};

			struct{
				unsigned char ack:1;
				unsigned char unused:7;
			};
		}flags;
		unsigned int reserved0:1;
		unsigned int stream_id:31;
		const unsigned char* payload;

		bool tobytes(unsigned char* p)
		{
			p[0] = (length >> 16) & 0xff;
			p[1] = (length >> 8) & 0xff;
			p[2] = (length >> 0) & 0xff;
			p[3] = type;
			p[4] = flags.i;
			p[5] = ((stream_id >> 24) & 0x7f) | (reserved0 << 8);
			p[6] = (stream_id >> 16) & 0xff;
			p[7] = (stream_id >> 8) & 0xff;
			p[8] = (stream_id >> 0) & 0xff;
			return true;
		}

		bool frombytes(const unsigned char* p,lyramilk::data::uint64 s,lyramilk::data::uint64* usedbytes)
		{
			length = p[0];
			length = length << 8 | p[1];
			length = length << 8 | p[2];
			if(s < length + (unsigned int)9){
				*usedbytes = 0;
				return false;
			}
			type = (h2_frame_type)p[3];
			flags.i = p[4];
			reserved0 = (p[5]&1);
			stream_id = p[5] &0x7f;
			stream_id = stream_id <<  7|  p[6];
			stream_id = stream_id << 8 | p[7];
			stream_id = stream_id << 8 | p[8];
			payload = p + 9;
			
			*usedbytes = length + 9;
			return true;
		}
	};


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
		stream_id = 0;
	}

	http_2_0::~http_2_0()
	{
	}

	void http_2_0::dtr()
	{
		lyramilk::teapoy::dtr((httpadapter*)this);
	}

	bool http_2_0::oninit(std::ostream& os)
	{
		return true;
	}

	bool http_2_0::onrequest(const char* cache,int size,std::ostream& os)
	{
		recvcache.append(cache,size);
		if(recvcache.size() < 9){
			return true;
		}
		if(recvcache[5] == H2_MAGIC){
			recvcache = recvcache.substr(24);
			//printf("跳过magic头\n");
		};


		while(recvcache.size() > 9){
			const unsigned char* p = (const unsigned char*)recvcache.c_str();
			h2_frame_header hd = {0};

			{
				lyramilk::data::uint64 usedbytes = 0;
				hd.frombytes(p,recvcache.size(),&usedbytes);
				recvcache.substr(usedbytes);
			}


			lyramilk::data::string fstr;
			if(hd.flags.proiority){
				fstr += "proiority,";
			}
			if(hd.flags.padded){
				fstr += "padded,";
			}
			if(hd.flags.endheader){
				fstr += "endheader,";
			}
			if(hd.flags.endstream){
				fstr += "endstream,";
			}
			printf("type=\x1b[33m%s\x1b[0m[%d],length=%u,flags=%u,reserve0=%u,stream_id=%u,flags=%s\n",h2_frame_type_tocstr((lyramilk::teapoy::h2_frame_type)hd.type),hd.type,hd.length,hd.flags,hd.reserved0,hd.stream_id,fstr.c_str());

			if(hd.type == H2_SETTINGS){
				for(int i=0;i<hd.length;i+=6){
					const unsigned char* p = hd.payload + i;
					unsigned short type = p[0];
					type |= type << 8 | p[1];

					unsigned int setting = p[2];
					setting |= setting << 8 | p[3];
					setting |= setting << 8 | p[4];
					setting |= setting << 8 | p[5];

					if(H2_SETTINGS_HEADER_TABLE_SIZE == type){
						hp.hard_header_table_size = setting;
					}
					printf("%u=%u\n",type,setting);
				}

			}

			if(hd.type == H2_PRIORITY){
				const unsigned char* p = hd.payload;
				unsigned int dep_stream_id = p[0];
				unsigned int exclusive = p[1]&1;
				dep_stream_id |= dep_stream_id << 7 | p[1];
				dep_stream_id |= dep_stream_id << 8 | p[2];
				dep_stream_id |= dep_stream_id << 8 | p[3];
				unsigned int weight = p[4];
				//printf("dep_stream_id=%u,weight=%u,exclusive=%u\n",dep_stream_id,weight,exclusive);
			}
			if(hd.type == H2_HEADERS){
				const unsigned char* p = hd.payload;
				unsigned int dep_stream_id = p[0];
				unsigned int exclusive = p[1]&1;
				dep_stream_id |= dep_stream_id << 7 | p[1];
				dep_stream_id |= dep_stream_id << 8 | p[2];
				dep_stream_id |= dep_stream_id << 8 | p[3];
				unsigned int weight = p[4];
				//printf("dep_stream_id=%u,weight=%u,exclusive=%u\n",dep_stream_id,weight,exclusive);

				hp.unpack(p + 5,hd.length - 5);

				lyramilk::data::string k,v;
				while(hp.next(&k,&v)){
					request->header[k] = v;
				}

				stream_id = hd.stream_id;
			}
			if(hd.type == H2_RST_STREAM){
				return false;
			}

			recvcache = recvcache.substr(9+hd.length);
			if(hd.flags.endheader){
				response->set("Connection",request->get("Connection"));

				//++stream_id;
				if(!service->call(request,response,this)){
					send_header_with_length(response,404,0);
				}

				lyramilk::data::string sconnect = response->get("Connection");
			
				if(sconnect != "close"){
					if(request_cache){
						delete request_cache;
						request_cache = nullptr;
					}
					if(recvcache.size() < 1){
						return true;
					}
					return onrequest(nullptr,0,os);
				}
				return false;
			}
		}



		/*
		dumpbytes(p+24,sizeof(16));
		unsigned int len = ph->length << 8;
		dumpbytes(&len,4);
		printf("type=%s[%d],length=%d\n",h2_frame_type_tocstr((lyramilk::teapoy::h2_frame_type)ph->type),ph->type,be32toh(len));
		*/
		return true;
	}

	bool http_2_0::call()
	{
		if(!service->call(request,response,this)){
			send_header_with_length(response,404,0);
		}
		return false;
	}


	bool http_2_0::send_header_with_chunk(httpresponse* response,lyramilk::data::uint32 code)
	{
		TODO();
	}

	bool http_2_0::send_header_with_length(httpresponse* response,lyramilk::data::uint32 code,lyramilk::data::uint64 content_length)
	{
		{
			const char* p = "\x00\x01\x00\x00\x10"  "\x00" "\x00\x02\x00\x00\x00"  "\x00" "\x00\x03\x00\x00\x00" "\xc8" "\x00\x04\x00\x00\xff" "\xff"  "\x00\x05\x00\x00\x40" "\x00"  "\x00\x06\x00\x04\x00" "\x00" ;
			unsigned int l = 36;

			h2_frame_header hd = {0};
			hd.type = H2_SETTINGS;
			hd.stream_id = 0;
			hd.length = l;
			unsigned char buff[9];
			if(!hd.tobytes(buff)) return false;
			dumpbytes(buff,9);
			os.write((const char*)buff,9);
			dumpbytes(p,l);
			os.write(p,l);
		}

		{
			h2_frame_header hd = {0};
			hd.type = H2_SETTINGS;
			hd.flags.ack = 1;
			hd.stream_id = 0;
			hd.length = 0;
			unsigned char buff[9];
			if(!hd.tobytes(buff)) return false;
			dumpbytes(buff,9);
			os.write((const char*)buff,9);
		}

		lyramilk::data::ostringstream oss;

		{
			char buff[64];
			snprintf(buff,sizeof(buff),"%llu",code);
			response->header[":status"] = buff;
			hpack::pack_kv(oss,":status",buff);
			response->header.erase(":status");
		}

		{
			char buff[64];
			snprintf(buff,sizeof(buff),"%llu",content_length);
			hpack::pack_kv(oss,"content-length",buff);
		}
		
		hpack::pack(oss,channel->default_response_header);
		hpack::pack(oss,response->header);

		lyramilk::data::string str = oss.str();

		const char* p = str.c_str();
		lyramilk::data::uint32 l = str.size();


		h2_frame_header hd = {0};
		hd.type = H2_HEADERS;
		hd.flags.endheader = 1;
		hd.stream_id = stream_id;
		hd.length = l;
		unsigned char buff[9];
		if(!hd.tobytes(buff)) return false;
		os.write((const char*)buff,9);
		os.write(p,l);
		return true;
	}


	bool http_2_0::send_bodydata(httpresponse* response,const char* p,lyramilk::data::uint32 l)
	{
		{
			lyramilk::data::string str;
			h2_frame_header hd = {0};
			hd.type = H2_DATA;
			hd.stream_id = stream_id;
			hd.length = l;
			unsigned char buff[9];
			if(!hd.tobytes(buff)) return false;
			os.write((const char*)buff,9);
			os.write(p,l);
COUT.write(p,l) << std::endl;
		}
		{
			lyramilk::data::string str;
			h2_frame_header hd = {0};
			hd.type = H2_DATA;
			hd.flags.endstream = 1;
			hd.stream_id = stream_id;
			hd.length = 0;
			unsigned char buff[9];
			if(!hd.tobytes(buff)) return false;
			os.write((const char*)buff,9);
			os.write(p,l);
		}
		return true;
	}

}}
