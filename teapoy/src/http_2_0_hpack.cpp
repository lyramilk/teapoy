#include "http_2_0_hpack.h"
#include "stringutil.h"
#include <libmilk/hash.h>


namespace lyramilk{ namespace teapoy {
	struct hpack_static_entry
	{
		lyramilk::data::string k;
		lyramilk::data::string v;
	};
	#define MAKE_STATIC_ENT(K,V,I,H)	{K,V}

	static hpack_static_entry static_table[] = {
		MAKE_STATIC_ENT(":authority", "", 0, 3153725150u),
		MAKE_STATIC_ENT(":method", "GET", 1, 695666056u),
		MAKE_STATIC_ENT(":method", "POST", 1, 695666056u),
		MAKE_STATIC_ENT(":path", "/", 3, 3292848686u),
		MAKE_STATIC_ENT(":path", "/index.html", 3, 3292848686u),
		MAKE_STATIC_ENT(":scheme", "http", 5, 2510477674u),
		MAKE_STATIC_ENT(":scheme", "https", 5, 2510477674u),
		MAKE_STATIC_ENT(":status", "200", 7, 4000288983u),
		MAKE_STATIC_ENT(":status", "204", 7, 4000288983u),
		MAKE_STATIC_ENT(":status", "206", 7, 4000288983u),
		MAKE_STATIC_ENT(":status", "304", 7, 4000288983u),
		MAKE_STATIC_ENT(":status", "400", 7, 4000288983u),
		MAKE_STATIC_ENT(":status", "404", 7, 4000288983u),
		MAKE_STATIC_ENT(":status", "500", 7, 4000288983u),
		MAKE_STATIC_ENT("accept-charset", "", 14, 3664010344u),
		MAKE_STATIC_ENT("accept-encoding", "gzip, deflate", 15, 3379649177u),
		MAKE_STATIC_ENT("accept-language", "", 16, 1979086614u),
		MAKE_STATIC_ENT("accept-ranges", "", 17, 1713753958u),
		MAKE_STATIC_ENT("accept", "", 18, 136609321u),
		MAKE_STATIC_ENT("access-control-allow-origin", "", 19, 2710797292u),
		MAKE_STATIC_ENT("age", "", 20, 742476188u),
		MAKE_STATIC_ENT("allow", "", 21, 2930878514u),
		MAKE_STATIC_ENT("authorization", "", 22, 2436257726u),
		MAKE_STATIC_ENT("cache-control", "", 23, 1355326669u),
		MAKE_STATIC_ENT("content-disposition", "", 24, 3889184348u),
		MAKE_STATIC_ENT("content-encoding", "", 25, 65203592u),
		MAKE_STATIC_ENT("content-language", "", 26, 24973587u),
		MAKE_STATIC_ENT("content-length", "", 27, 1308181789u),
		MAKE_STATIC_ENT("content-location", "", 28, 2302364718u),
		MAKE_STATIC_ENT("content-range", "", 29, 3555523146u),
		MAKE_STATIC_ENT("content-type", "", 30, 4244048277u),
		MAKE_STATIC_ENT("cookie", "", 31, 2007449791u),
		MAKE_STATIC_ENT("date", "", 32, 3564297305u),
		MAKE_STATIC_ENT("etag", "", 33, 113792960u),
		MAKE_STATIC_ENT("expect", "", 34, 2530896728u),
		MAKE_STATIC_ENT("expires", "", 35, 1049544579u),
		MAKE_STATIC_ENT("from", "", 36, 2513272949u),
		MAKE_STATIC_ENT("host", "", 37, 2952701295u),
		MAKE_STATIC_ENT("if-match", "", 38, 3597694698u),
		MAKE_STATIC_ENT("if-modified-since", "", 39, 2213050793u),
		MAKE_STATIC_ENT("if-none-match", "", 40, 2536202615u),
		MAKE_STATIC_ENT("if-range", "", 41, 2340978238u),
		MAKE_STATIC_ENT("if-unmodified-since", "", 42, 3794814858u),
		MAKE_STATIC_ENT("last-modified", "", 43, 3226950251u),
		MAKE_STATIC_ENT("link", "", 44, 232457833u),
		MAKE_STATIC_ENT("location", "", 45, 200649126u),
		MAKE_STATIC_ENT("max-forwards", "", 46, 1826162134u),
		MAKE_STATIC_ENT("proxy-authenticate", "", 47, 2709445359u),
		MAKE_STATIC_ENT("proxy-authorization", "", 48, 2686392507u),
		MAKE_STATIC_ENT("range", "", 49, 4208725202u),
		MAKE_STATIC_ENT("referer", "", 50, 3969579366u),
		MAKE_STATIC_ENT("refresh", "", 51, 3572655668u),
		MAKE_STATIC_ENT("retry-after", "", 52, 3336180598u),
		MAKE_STATIC_ENT("server", "", 53, 1085029842u),
		MAKE_STATIC_ENT("set-cookie", "", 54, 1848371000u),
		MAKE_STATIC_ENT("strict-transport-security", "", 55, 4138147361u),
		MAKE_STATIC_ENT("transfer-encoding", "", 56, 3719590988u),
		MAKE_STATIC_ENT("user-agent", "", 57, 606444526u),
		MAKE_STATIC_ENT("vary", "", 58, 1085005381u),
		MAKE_STATIC_ENT("via", "", 59, 1762798611u),
		MAKE_STATIC_ENT("www-authenticate", "", 60, 779865858u),
	};

	#include "http_2_0_hpack_huffman.hpp"


	extern hpack_static_entry static_table[];
	const unsigned int static_table_bound = sizeof(static_table)/sizeof(static_table[0]);


	void nghttp2_hd_huff_decode_context_init(nghttp2_hd_huff_decode_context *ctx) {
	  ctx->state = 0;
	  ctx->accept = 1;
	}

	lyramilk::data::string nghttp2_hd_huff_decode(nghttp2_hd_huff_decode_context *ctx,const uint8_t *src,size_t srclen, int final) {

		lyramilk::data::string result;
		size_t i;
		for (i = 0; i < srclen; ++i) {
			const nghttp2_huff_decode *t;

			t = &huff_decode_table[ctx->state][src[i] >> 4];
			if (t->flags & NGHTTP2_HUFF_FAIL) {
				throw "NGHTTP2_ERR_HEADER_COMP";
			}
			if (t->flags & NGHTTP2_HUFF_SYM) {
				result.push_back(t->sym);
			}

			t = &huff_decode_table[t->state][src[i] & 0xf];
			if (t->flags & NGHTTP2_HUFF_FAIL) {
				throw "NGHTTP2_ERR_HEADER_COMP";
			}
			if (t->flags & NGHTTP2_HUFF_SYM) {
				result.push_back(t->sym);
			}

			ctx->state = t->state;
			ctx->accept = (t->flags & NGHTTP2_HUFF_ACCEPTED) != 0;
		}
		if (final && !ctx->accept) {
			throw "NGHTTP2_ERR_HEADER_COMP";
		}
		//return (ssize_t)i;
		return result;
	}

	hpack::hpack()
	{
		hard_header_table_size = 200;
	}

	hpack::~hpack()
	{

	}

	void hpack::unpack(const unsigned char* p,lyramilk::data::uint64 l)
	{
		cache.append(p,l);
		cursor = cache.c_str();
	}


	bool hpack::next(lyramilk::data::string* k,lyramilk::data::string* v)
	{
		if(cursor < cache.c_str() + cache.size()){
			unsigned char c = *cursor;
			if(c & 0x80){
				//	6.1索引
				lyramilk::data::uint64 idx = read_int(7);
				return read_table(idx,k,v);
			}else if( (c & 0xc0) == 0x40){
				//	6.2.1
				lyramilk::data::uint64 idx = read_int(6);
				if(idx == 0){
					*k = read_string();
					*v = read_string();
				}else{
					read_table(idx,k,nullptr);
					*v = read_string();
				}
				dynamic_table.push_back(std::pair<lyramilk::data::string,lyramilk::data::string>(*k,*v));
COUT << "添加索引" << *k << "==" << *v << std::endl;
				//dynamic_table
				return true;
			}else if( (c & 0xf0) == 0x00){
				//	6.2.2
				lyramilk::data::uint64 idx = read_int(4);
				if(idx == 0){
					*k = read_string();
					*v = read_string();
				}else{
					read_table(idx,k,nullptr);
					*v = read_string();
				}
				return true;
			}else if( (c & 0xf0) == 0x10){
				//	6.2.3
				lyramilk::data::uint64 idx = read_int(4);
				if(idx == 0){
					*k = read_string();
					*v = read_string();
				}else{
					read_table(idx,k,nullptr);
					*v = read_string();
				}
				return true;
			}else if( (c & 0xe0) == 0x20){
				dynamic_table_size = read_int(7);
COUT << "动态太大小 " << dynamic_table_size << std::endl;
				return true;
			}else{
				TODO();
			}
		}
		return false;
	}

	bool hpack::read_table(lyramilk::data::uint64 idx,lyramilk::data::string* k,lyramilk::data::string* v)
	{
		if(idx < static_table_bound){
			if(k) *k = static_table[idx - 1].k;
			if(v) *v = static_table[idx - 1].v;
			return true;
		}

		unsigned int index = idx-static_table_bound;
		if(index < dynamic_table.size()){
			if(k) *k = dynamic_table[index].first;
			if(v) *v = dynamic_table[index].second;
			return true;
		}

COUT << "未识别的动态表项 " << index << std::endl;
		return false;
	}

	lyramilk::data::string hpack::read_string()
	{
		bool huffman = false;
		if(((*cursor) & 0x80) == 0x80) huffman = true;
		lyramilk::data::uint64 slen = read_int(7);
		if(!huffman){
			lyramilk::data::string result((const char*)cursor,slen);
			cursor += slen;
			return result;
		}



		nghttp2_hd_huff_decode_context ctx;
		nghttp2_hd_huff_decode_context_init(&ctx);

		lyramilk::data::string result = nghttp2_hd_huff_decode(&ctx,cursor,slen, 1);
		cursor += slen;
		return result;
	}


	lyramilk::data::uint64 hpack::read_int(unsigned int n)
	{
		unsigned int mask = (1<<n) - 1;

		unsigned char c = *cursor;
		unsigned int idx = c & mask;
		if(idx != mask){
			++cursor;
			return idx;
		}

		lyramilk::data::uint64 result = mask;
		unsigned int m = 0;
		do{
			c = *--cursor;
			lyramilk::data::uint64 tmp = c&0x7f;
			result += result + tmp << m;
			m += 7;
		}while(c & 0x80 == 0x80);

		return result;
	}

	bool hpack::pack(std::ostream& os,const lyramilk::data::stringdict& m)
	{
		lyramilk::data::stringdict::const_iterator it = m.begin();
		for(;it != m.end();++it){
			if(it->first != ":status") continue;
			if(it->first.empty() || it->second.empty()) continue;
			if(!pack_kv(os,it->first,it->second)){
				return false;
			}
		}

		it = m.begin();
		for(;it != m.end();++it){
			if(it->first == ":status") continue;
			if(it->first.empty() || it->second.empty()) continue;
			if(!pack_kv(os,it->first,it->second)){
				return false;
			}
		}
		return true;
	}


	bool hpack::pack_kv(std::ostream& os,const lyramilk::data::string& k,const lyramilk::data::string& v)
	{
		unsigned int hash = lyramilk::cryptology::hash32::fnv(k.c_str(),k.size());

		for(int i=0;i<static_table_bound;++i){
			/*if(static_table[0].v.empty()){
			
			}else */if(k == static_table[i].k && v == static_table[i].v){
				os.put(0x80|((i+1)&0x7f));
				return true;
			}
		}


		{
			os.put(0x10);
			pack_str(os,lowercase(k));
			pack_str(os,v);
		}

		return true;
	}

	bool hpack::pack_str(std::ostream& os,const lyramilk::data::string& s)
	{
		if(s.size() < 128){
			unsigned char c = s.size() & 0x7f;
			os.put(c);
			os.write(s.c_str(),s.size());
		}else{
			unsigned char c = 0x7f;
			os.put(c);
			unsigned int i = s.size() - 127;
			while(i>=128){
				c = i % 0x80 | 0x80;
				os.put(c);
				i = i / 128;
			}
			c = i % 0x80;
			os.put(c);
			os.write(s.c_str(),s.size());
		}
	}

}}
