#include "url_dispatcher.h"
#include "stringutil.h"
#include "mimetype.h"
#include <libmilk/dict.h>
#include <libmilk/stringutil.h>
#include <sys/stat.h>
#include <errno.h>
#include "teapoy_gzip.h"
#include "util.h"

#include <fstream>

namespace lyramilk{ namespace teapoy {

	class url_selector_gzstatic:public url_regex_selector
	{
		bool nocache;
		lyramilk::data::string force_content_type;
	  public:
		url_selector_gzstatic()
		{
			nocache = false;
		}

		virtual ~url_selector_gzstatic()
		{}
		
		bool init(lyramilk::data::map& m)
		{
			force_content_type = m["content_type"].str();
			return url_regex_selector::init(m);
		}

		virtual dispatcher_check_status call(httprequest* request,httpresponse* response,httpadapter* adapter,const lyramilk::data::string& real)
		{
			url_selector_loger _("teapoy.web.static",adapter);
			vcall(request,response,adapter,path_simplify(real));
			return cs_ok;
		}

		void vcall(httprequest* request,httpresponse* response,httpadapter* adapter,const lyramilk::data::string& real)
		{
			struct stat st = {0};
			if(0 !=::stat(real.c_str(),&st)){
				if(errno == ENOENT){
					adapter->response->code = 404;
				}else if(errno == EACCES){
					adapter->response->code = 403;
				}else if(errno == ENAMETOOLONG){
					adapter->response->code = 400;
				}
				return ;
			}

			if(st.st_mode&S_IFDIR){
				adapter->response->code = 404;
				return ;
			}

			bool usecache = true;
			if(nocache){
				usecache = false;
			}
			lyramilk::data::string etag;
			//	取得 ETag
			if(usecache){
				{
					char buff_etag[100];
					long long unsigned filemtime = st.st_mtime;
					long long unsigned filesize = st.st_size;
					sprintf(buff_etag,"%llx-%llx",filemtime,filesize);
					etag = buff_etag;
				}

			}

			if(usecache){
				lyramilk::data::string sifetagnotmatch = request->get("If-None-Match");
				if(sifetagnotmatch == etag){
					response->set("Cache-Control","max-age=3600,public");
					adapter->response->code = 304;
					return ;
				}
			}
			
			lyramilk::data::string realwithgz = real;
			if(real.compare(real.size() - 3,3,".gz") == 0){
				realwithgz = real.substr(0,real.size() -3);
			}

			lyramilk::data::string mimetype = mimetype::getmimetype_byname(realwithgz);
			//	取得 Content-Type
			if(mimetype.empty()){
				mimetype = mimetype::getmimetype_byfile(real);
				if(mimetype.empty()){
					mimetype = "application/octet-stream";
				}
			}

			lyramilk::data::string sacceptencoding = request->get("Accept-Encoding");
			if(sacceptencoding.find("gzip") == sacceptencoding.npos){
				mimetype = "application/gzip; charset=binary";
				std::size_t pos = real.rfind("/");
				if(pos != real.npos){
					response->set("Content-Disposition","attachment; filename=\"" + realwithgz + "\"");
				}
			}

			// range支持
			lyramilk::data::uint64 filesize = st.st_size;
			lyramilk::data::int64 datacount = filesize;

			//准备发送响应头
			if(force_content_type.empty()){
				response->set("Content-Type",mimetype);
			}else{
				response->set("Content-Type",force_content_type);
			}
			response->set("Content-Encoding","gzip");

			if(nocache){
				response->set("Cache-Control","no-cache");
				response->set("pragma","no-cache");
			}else{
				response->set("Cache-Control","max-age=3600,public");
				if(!etag.empty()){
					response->set("ETag",etag);
				}
			}

			adapter->response->code = 200;
			{
				adapter->response->content_length = datacount;
				std::ifstream ifs;
				ifs.open(real.c_str(),std::ifstream::binary|std::ifstream::in);
				if(!ifs.is_open()){
					adapter->response->code = 403;
					return ;
				}
				char buff[2048];
				for(;ifs && datacount > 0;){
					ifs.read(buff,std::min(sizeof(buff),(std::size_t)datacount));
					lyramilk::data::int64 gcount = ifs.gcount();
					if(gcount > 0){
						datacount -= gcount;
						adapter->send_data(buff,gcount);
					}else{
						break;
					}
				}
				ifs.close();
			}
		}



		static url_selector* ctr(void*)
		{
			return new url_selector_gzstatic;
		}

		static void dtr(url_selector* p)
		{
			delete (url_selector_gzstatic*)p;
		}
	};



	static __attribute__ ((constructor)) void __init()
	{
		url_selector_factory::instance()->define("gzstatic",url_selector_gzstatic::ctr,url_selector_gzstatic::dtr);
	}

}}
