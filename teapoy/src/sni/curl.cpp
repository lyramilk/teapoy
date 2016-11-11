#include "script.h"
#include <libmilk/var.h>
#include <libmilk/log.h>
#include <libmilk/multilanguage.h>
#include <curl/curl.h>
#include <curl/easy.h>
#include <fstream>
#include <stdlib.h>
#include <sys/stat.h> 
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

namespace lyramilk{ namespace teapoy{ namespace native{

	class httpclient
	{
		lyramilk::log::logss log;
		lyramilk::data::string url;
		lyramilk::data::string lasterror;
	  public:
		static void* ctr(lyramilk::data::var::array args)
		{
			return new httpclient(args[0]);
		}
		static void dtr(void* p)
		{
			delete (httpclient*)p;
		}

		httpclient(lyramilk::data::string url):log(lyramilk::klog,"teapoy.native.httpclient")
		{
			this->url = url;
		}

		virtual ~httpclient()
		{
		}

		static size_t curl_writestream_callback(void* buff, size_t size, size_t nmemb, void* userp) {
			std::ostream* ss = reinterpret_cast<std::ostream*> (userp);
			size *= nmemb;
			ss->write((const char*)buff,size);
			return size;
		}

		lyramilk::data::var get(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			lyramilk::data::stringstream ss;
			CURL *c = curl_easy_init();
			curl_easy_setopt(c, CURLOPT_URL,url.c_str());
			curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curl_writestream_callback);
			curl_easy_setopt(c, CURLOPT_WRITEDATA, (std::ostream*)&ss);
			CURLcode res = curl_easy_perform(c);
			curl_easy_cleanup(c);

			if(res != CURLE_OK){
				lasterror = curl_easy_strerror(res);
				return lyramilk::data::var::nil;
			}
			return ss.str();
		}

		lyramilk::data::var download(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string downloaddir = args[0];
			if(downloaddir.empty()) throw lyramilk::exception(D("下载目录不能为空"));
			if(downloaddir[downloaddir.size() - 1] != '/'){
				downloaddir += "/%u";
			}else{
				downloaddir += "%u";
			}

			lyramilk::data::string filename;
			std::ofstream ofs;
			{
				int fd = 0;
				char buff[1024];
				srand(time(0));
				do{
					sprintf(buff,downloaddir.c_str(),(unsigned int)rand());
					fd = open(buff,O_CREAT | O_EXCL,S_IREAD|S_IWRITE);

					if(fd != -1){
						close(fd);
						ofs.open(buff,std::ofstream::out|std::ofstream::binary);
						filename = buff;
					}else if(errno != EEXIST){
						throw lyramilk::exception(D("文件错误%s",strerror(errno)));
					}
				}while(!ofs.good());
			}

			CURL *c = curl_easy_init();
			curl_easy_setopt(c, CURLOPT_URL,url.c_str());
			curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curl_writestream_callback);
			curl_easy_setopt(c, CURLOPT_WRITEDATA, (std::ostream*)&ofs);
			CURLcode res = curl_easy_perform(c);
			curl_easy_cleanup(c);


			if(res != CURLE_OK){
				lasterror = curl_easy_strerror(res);
				return lyramilk::data::var::nil;
			}
			return filename;
		}

		lyramilk::data::var upload(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_str);
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,2,lyramilk::data::var::t_str);

			lyramilk::data::string mimetype;
			if(args.size() > 3){
				MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,3,lyramilk::data::var::t_str);
				mimetype = args[3].str();
			}

			lyramilk::data::stringstream ss;
			lyramilk::data::string field = args[0];
			lyramilk::data::string path = args[1];
			lyramilk::data::string filename = args[2];
			struct curl_httppost *formpost=NULL;  
			struct curl_httppost *lastptr=NULL;  
			if(mimetype.empty()){
				curl_formadd(&formpost,&lastptr,CURLFORM_FILE, path.c_str(),CURLFORM_COPYNAME, field.c_str(),CURLFORM_FILENAME, filename.c_str(),CURLFORM_END);  
			}else{
				curl_formadd(&formpost,&lastptr,CURLFORM_FILE, path.c_str(),CURLFORM_COPYNAME, field.c_str(),CURLFORM_FILENAME, filename.c_str(),CURLFORM_CONTENTTYPE, "image/jpeg",CURLFORM_END);  
			}
			CURL *c = curl_easy_init();
			curl_easy_setopt(c, CURLOPT_URL,url.c_str());
			curl_easy_setopt(c, CURLOPT_HTTPPOST, formpost);
			curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curl_writestream_callback);
			curl_easy_setopt(c, CURLOPT_WRITEDATA, (std::ostream*)&ss);
			CURLcode res = curl_easy_perform(c);
			curl_easy_cleanup(c);

			curl_formfree(formpost);


			if(res != CURLE_OK){
				lasterror = curl_easy_strerror(res);
				return lyramilk::data::var::nil;
			}
			return ss.str();
		}

		lyramilk::data::var todo(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			TODO();
		}

		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["get"] = lyramilk::script::engine::functional<httpclient,&httpclient::get>;
			//fn["post"] = lyramilk::script::engine::functional<httpclient,&httpclient::post>;
			fn["download"] = lyramilk::script::engine::functional<httpclient,&httpclient::download>;
			fn["upload"] = lyramilk::script::engine::functional<httpclient,&httpclient::upload>;
			fn["todo"] = lyramilk::script::engine::functional<httpclient,&httpclient::todo>;
			fn["todo"] = lyramilk::script::engine::functional<httpclient,&httpclient::todo>;
			fn["todo"] = lyramilk::script::engine::functional<httpclient,&httpclient::todo>;
			p->define("HttpClient",fn,httpclient::ctr,httpclient::dtr);
			return 1;
		}
	};
	static int define(lyramilk::script::engine* p)
	{
		int i = 0;
		i+= httpclient::define(p);
		return i;
	}


	static __attribute__ ((constructor)) void __init()
	{
		lyramilk::teapoy::script2native::instance()->regist("curl",define);
	}
}}}
