#include "mimetype.h"
#include "stringutil.h"

namespace lyramilk{ namespace teapoy {
	static lyramilk::data::unordered_map<lyramilk::data::string,lyramilk::data::string>& mimetypes()
	{
		static lyramilk::data::unordered_map<lyramilk::data::string,lyramilk::data::string> _mm;
		return _mm;
	}
	lyramilk::data::string mimetype::getmimetype_byname(lyramilk::data::string filename)
	{
		std::size_t pos = filename.find_last_of('.');
		if(filename.npos == pos) return "";
		lyramilk::data::string ext = lyramilk::teapoy::lowercase(filename.substr(pos + 1));

		const lyramilk::data::unordered_map<lyramilk::data::string,lyramilk::data::string>& m = mimetypes();
		lyramilk::data::unordered_map<lyramilk::data::string,lyramilk::data::string>::const_iterator it = m.find(ext);
		if(it == m.end()) return "";
		return it->second;
	}

	lyramilk::data::string mimetype::getmimetype_bydata(const unsigned char* data,std::size_t size)
	{
#ifdef LIBFILE_FOUND
		magic_t magic = magic_open(MAGIC_MIME_TYPE);
		if(magic == NULL)
		{
			log(lyramilk::log::warning,__FUNCTION__) << D("分析MIME类型时发生错误") << std::endl;
			return "application/octet-stream";
		}
		if (0 != magic_load(magic, "/usr/share/misc/magic")) 
		{
			log(lyramilk::log::warning,__FUNCTION__) << D("分析MIME类型时发生错误") << std::endl;
			return "application/octet-stream";
		}
		lyramilk::data::string mimetype = magic_buffer(magic,data,size);
		magic_close(magic);
		return mimetype;
#else
		return "";
#endif
	}

	lyramilk::data::string mimetype::getmimetype_byfile(lyramilk::data::string filepathname)
	{
#ifdef LIBFILE_FOUND
		magic_t magic = magic_open(MAGIC_MIME_TYPE);
		if(magic == NULL)
		{
			log(lyramilk::log::warning,__FUNCTION__) << D("分析MIME类型时发生错误") << std::endl;
			return "application/octet-stream";
		}
		if (0 != magic_load(magic, "/usr/share/misc/magic")) 
		{
			log(lyramilk::log::warning,__FUNCTION__) << D("分析MIME类型时发生错误") << std::endl;
			return "application/octet-stream";
		}
		lyramilk::data::string mimetype = magic_file(magic,filepathname.c_str());
		magic_close(magic);
		return mimetype;
#else
		return "";
#endif
	}

	void mimetype::define_mimetype_fileextname(lyramilk::data::string extname,lyramilk::data::string mimetype)
	{
		lyramilk::data::unordered_map<lyramilk::data::string,lyramilk::data::string>& m = mimetypes();
		m[extname] = mimetype;
	}

	static __attribute__ ((constructor)) void __init()
	{
		lyramilk::data::unordered_map<lyramilk::data::string,lyramilk::data::string>& m = mimetypes();
		m["*"] = "application/octet-stream";
		m["htm"] = "text/html";
		m["html"] = "text/html";
		m["txt"] = "text/plain";
		m["css"] = "text/css";
		m["js"] = "application/javascript";
		m["jpg"] = "image/jpeg";
		m["jpeg"] = "image/jpeg";
		m["gif"] = "image/gif";
		m["bmp"] = "image/bitmap";
		m["png"] = "image/png";
		m["tiff"] = "image/tiff";
		m["avi"] = "video/avi";
		m["mp3"] = "audio/mp3";
		m["mp4"] = "video/mp4";
		m["flv"] = "video/x-flv";
		m["swf"] = "application/x-shockwave-flash";
		m["wav"] = "audio/wav";
		m["ico"] = "image/x-icon";
		m["pac"] = "application/x-ns-proxy-autoconfig";
		m["zip"] = "application/zip";
		m["gz"] = "application/gzip";
	}
}}
