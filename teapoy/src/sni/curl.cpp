#include "script.h"
#include "httpclient.h"
#include <libmilk/var.h>
#include <libmilk/log.h>
#include <libmilk/dict.h>
#include <libmilk/codes.h>

#include <curl/curl.h>
#include <curl/easy.h>

#include <fstream>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h> 
#include <errno.h>
#include <string.h>

namespace lyramilk{ namespace teapoy{ namespace native{


	lyramilk::log::logss static log_curl(lyramilk::klog,"teapoy.native.curl");

	class httpclient:public lyramilk::script::sclass
	{
		lyramilk::log::logss log;
		lyramilk::data::string url;
		lyramilk::data::string lasterror;
	  public:
		static lyramilk::script::sclass* ctr(const lyramilk::data::array& args)
		{
			return new httpclient(args[0]);
		}
		static void dtr(lyramilk::script::sclass* p)
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

		lyramilk::data::var get(const lyramilk::data::array& args,const lyramilk::data::map& env)
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

		lyramilk::data::var post(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string postdata = args[0];
			lyramilk::data::stringstream ss;
			CURL *c = curl_easy_init();
			//curl_easy_setopt(c, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0);
			curl_easy_setopt(c, CURLOPT_URL,url.c_str());
			curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curl_writestream_callback);
			curl_easy_setopt(c, CURLOPT_WRITEDATA, (std::ostream*)&ss);
			curl_easy_setopt(c, CURLOPT_POST, 1);
			curl_easy_setopt(c, CURLOPT_POSTFIELDS, postdata.c_str());
			CURLcode res = curl_easy_perform(c);
			curl_easy_cleanup(c);

			if(res != CURLE_OK){
				lasterror = curl_easy_strerror(res);
				return lyramilk::data::var::nil;
			}
			return ss.str();
		}

		lyramilk::data::var download(const lyramilk::data::array& args,const lyramilk::data::map& env)
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

		lyramilk::data::var upload(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_str);
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,2,lyramilk::data::var::t_str);

			lyramilk::data::string mimetype;
			if(args.size() > 3){
				MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,3,lyramilk::data::var::t_str);
				mimetype = args[3].str();
			}

			lyramilk::data::datastream ss;
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

		lyramilk::data::var todo(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			TODO();
		}


		static lyramilk::data::var curl_return_bin(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			/*
			MILK_CHECK_SCRIPT_ARGS_LOG(log_curl,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string url = args[0].str();
			if(args.size() > 1){
				MILK_CHECK_SCRIPT_ARGS_LOG(log_curl,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_map);
				const lyramilk::data::map& params = args[1];

				lyramilk::data::string fullurl = lyramilk::teapoy::httpclient::makeurl(url.c_str(),params);
				return lyramilk::teapoy::httpclient::rcallb(fullurl.c_str());
			}
			return lyramilk::teapoy::httpclient::rcallb(url.c_str());*/

			lyramilk::data::var v = curl_return_str(args,env);
			v.type(lyramilk::data::var::t_bin);
			return v;
		}


		static lyramilk::data::var curl_return_str(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			static lyramilk::data::coding* urlcomponent = lyramilk::data::codes::instance()->getcoder("urlcomponent");

			MILK_CHECK_SCRIPT_ARGS_LOG(log_curl,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string url = args[0].str();

			lyramilk::data::stringstream ssurl;
			ssurl << url;
			int timeout_msec = 2000;

			if(args.size() > 1 && args[1].type() == lyramilk::data::var::t_map){
				const lyramilk::data::map& params = args[1];
				if(!params.empty()){
					lyramilk::data::map::const_iterator it = params.begin();
					if( url.find("?") == url.npos){
						if(it!=params.end()){
							ssurl << "?" << urlcomponent->encode(it->first) << "=" << urlcomponent->encode(it->second);
							++it;
						}
					}
					for(;it!=params.end();++it){
						ssurl << "&" << urlcomponent->encode(it->first) << "=" << urlcomponent->encode(it->second);
					}
				}
				if(args.size() > 2 && args[2].type_like(lyramilk::data::var::t_int)){
					timeout_msec = args[2].conv(2000);
				}
			}

			lyramilk::data::string newurl = ssurl.str();
			lyramilk::data::ostringstream ss;
			CURL *c = curl_easy_init();
			curl_easy_setopt(c, CURLOPT_TIMEOUT_MS, timeout_msec);
			curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1 );
			curl_easy_setopt(c, CURLOPT_URL,newurl.c_str());
			curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curl_writestream_callback);
			curl_easy_setopt(c, CURLOPT_WRITEDATA, (std::ostream*)&ss);
			
			struct curl_slist* headers = NULL;
			if(args.size() > 3){
				MILK_CHECK_SCRIPT_ARGS_LOG(log_curl,lyramilk::log::warning,__FUNCTION__,args,3,lyramilk::data::var::t_map);
				const lyramilk::data::map& header = args[3];


				if(!header.empty()){
					lyramilk::data::map::const_iterator it = header.begin();
					for(;it!=header.end();++it){
						lyramilk::data::string q = it->first + ": " + it->second.str();
						headers=curl_slist_append(headers, q.c_str());
					}
					if(headers){
						curl_easy_setopt(c, CURLOPT_HTTPHEADER, headers);
					}
				}
			}


			CURLcode res = curl_easy_perform(c);
			curl_easy_cleanup(c);

			if(headers){
				curl_slist_free_all(headers);
			}

			if(res != CURLE_OK){
				log_curl(lyramilk::log::error,"curl") << "err=" << curl_easy_strerror(res) << ",url=" << newurl << std::endl;
				return "";
			}
			return ss.str();
		}

		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["get"] = lyramilk::script::engine::functional<httpclient,&httpclient::get>;
			fn["post"] = lyramilk::script::engine::functional<httpclient,&httpclient::post>;
			fn["download"] = lyramilk::script::engine::functional<httpclient,&httpclient::download>;
			fn["upload"] = lyramilk::script::engine::functional<httpclient,&httpclient::upload>;
			fn["todo"] = lyramilk::script::engine::functional<httpclient,&httpclient::todo>;
			fn["todo"] = lyramilk::script::engine::functional<httpclient,&httpclient::todo>;
			fn["todo"] = lyramilk::script::engine::functional<httpclient,&httpclient::todo>;
			p->define("HttpClient",fn,httpclient::ctr,httpclient::dtr);
			p->define("curl",curl_return_str);
			p->define("curl_rcall",curl_return_bin);
			return 2;
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
		lyramilk::teapoy::script_interface_master::instance()->regist("http",define);
	}
}}}
