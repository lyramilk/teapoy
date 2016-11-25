#ifndef _lyramilk_teapoy_mime_h_
#define _lyramilk_teapoy_mime_h_

#include <libmilk/var.h>

namespace lyramilk{ namespace teapoy {

	class mime
	{
	  protected:
		//解析相关
		enum{
			s0,
			sbody,
			schunked,
			sbad,
			spart,
		}s;
		lyramilk::data::string mimeheader;
		bool ismultipart;
		bool usefilecache;
		lyramilk::data::string bodycache;
		lyramilk::data::string boundary;
		lyramilk::data::int64 body_length;
		std::vector<mime> childs;
		//文件映射相关
		FILE* tmpbodyfile;
		const void* offset_body;
		lyramilk::data::int64 rlength_body;

		//参数相关
		//mutipart相关
		const char* ptr_part;
		lyramilk::data::int64 len_part;
		const char* ptr_body;
	  protected:
		virtual bool parsebody(const char* buf,int size,int* remain);
		virtual bool parsebodydata(const char* buf,int size);
		virtual void parsepart(const char* buf,int size);
		virtual void parsemimeheader(const lyramilk::data::string& mimeheaderstr);
	  public:
		lyramilk::data::var::map header;
	  public:
		mime();
		virtual ~mime();

		virtual bool parse(const char* buf,int size,int* remain);
		virtual bool bad();
		virtual void reset();

		lyramilk::data::var get(lyramilk::data::string k) const;
		void set(lyramilk::data::string k,const lyramilk::data::var& v);
		lyramilk::data::var& operator[](lyramilk::data::string k);

		const char* getbodyptr();
		lyramilk::data::int64 getbodylength();

		int getmultipartcount() const;

		mime& selectpart(int index);
		const char* getpartptr();
		lyramilk::data::int64 getpartlength();

		static lyramilk::data::string getmimetype_byname(lyramilk::data::string filename);
		static lyramilk::data::string getmimetype_bydata(lyramilk::data::string filedata);
		static lyramilk::data::string getmimetype_byfile(lyramilk::data::string filepathname);
	};

}}

#endif
