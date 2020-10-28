#include "url_dispatcher.h"
#include "stringutil.h"
#include "mimetype.h"
#include <libmilk/codes.h>
#include <libmilk/dict.h>
#include <libmilk/stringutil.h>
#include <libmilk/xml.h>
#include <libmilk/dir.h>
#include <sys/stat.h>
#include <errno.h>
#include "teapoy_gzip.h"
#include "util.h"
#include "webdav.h"

#include <fstream>
#include <dirent.h>
#include <sys/file.h>
#include <string.h>

namespace lyramilk{ namespace teapoy {

	class url_selector_webdav:public url_regex_selector
	{
	  public:
		url_selector_webdav()
		{
			//allowmethod = "GET,POST,HEADER,PROPFIND,PROPPATCH,MKCOL,COPY,MOVE,LOCK,UNLOCK";
		}

		virtual ~url_selector_webdav()
		{}

		virtual dispatcher_check_status call(httprequest* request,httpresponse* response,httpadapter* adapter,const lyramilk::data::string& real)
		{
			url_selector_loger _("teapoy.web.webdav",adapter);
			return vcall(request,response,adapter,path_simplify(real));
		}

		static bool cbk_del_file(struct dirent *ent,const lyramilk::data::string& filename,void*userdata){
			remove(filename.c_str());
			return true;
		}

		dispatcher_check_status vcall(httprequest* request,httpresponse* response,httpadapter* adapter,const lyramilk::data::string& real)
		{
			lyramilk::data::string url = request->url();

			if(url[url.size() - 1] == '/'){
				url.erase(url.end()-1);
			}

			lyramilk::data::string method = request->get(":method");


			std::size_t pos = url.rfind('/');
			lyramilk::data::string updir = url.substr(0,pos);
			if("OPTIONS" == method){
				response->code = 200;
				//response->set("Allow","GET,POST,HEADER,PROPFIND,PROPPATCH,MKCOL,COPY,MOVE,LOCK,UNLOCK");
				response->set("Allow",this->allowmethod);
				return cs_ok;
			}


			if("PROPFIND" == method){
				struct stat st = {0};
				if(0 !=::stat(real.c_str(),&st) && errno == ENOENT){
					response->code = 404;
					return cs_ok;
				}

				response->code = 207;
				response->set("Content-Type","application/xml");

				lyramilk::webdav::filelist flist;

				{
					lyramilk::ptr<lyramilk::webdav::file> f = new lyramilk::webdav::fileemulator(url,real,"..");
					flist.append(f);
				}
				static lyramilk::data::coding* coder = lyramilk::data::codes::instance()->getcoder("url");

				DIR* d = opendir(real.c_str());
				if(d){
					struct dirent *ent;
					while((ent=readdir(d))!=nullptr){
						if(ent->d_name[0] == '.'){
							if(ent->d_name[1] == '.'){
								if(ent->d_name[2] == 0) continue;
							}else if(ent->d_name[1] == 0){
								continue;
							}
						}
						lyramilk::ptr<lyramilk::webdav::file> f = new lyramilk::webdav::fileemulator(coder->encode(url + "/" + ent->d_name),real + "/" + ent->d_name,ent->d_name);
						flist.append(f);
					}
				}

				lyramilk::data::string xmlstr = flist.to_xml();
				adapter->send_data(xmlstr.c_str(),xmlstr.size());
				return cs_ok;
			}else if("MKCOL" == method){
				int r = mkdir(real.c_str(),0755);
				if(r == -1){
					if(errno == ENOENT){
						response->code = 404;
						return cs_ok;
					}else if(errno == EACCES){
						response->code = 403;
						return cs_ok;
					}
					response->code = 500;
					return cs_ok;
				}
				response->code = 201;
				return cs_ok;
			}else if("PUT" == method){
				int fd = open(real.c_str(),O_WRONLY | O_CREAT | O_TRUNC,0644);
				if(fd == -1){
					if(errno == ENOENT){
						response->code = 404;
						return cs_ok;
					}else if(errno == EACCES){
						response->code = 403;
						return cs_ok;
					}
					response->code = 500;
					return cs_ok;
				}
				write(fd,request->get_body_ptr(),request->get_body_size());
				::close(fd);
				response->code = 201;
				return cs_ok;
			}else if("DELETE" == method){
				lyramilk::io::scan_dir(real.c_str(),cbk_del_file,nullptr,true);

				int r = remove(real.c_str());
				if(r == -1){
					response->code = 403;
					lyramilk::data::string xmlstr = strerror(errno);
					adapter->send_data(xmlstr.c_str(),xmlstr.size());
				}else{
					response->code = 205;
				}
				return cs_ok;
			}else if("MOVE" == method){
				lyramilk::data::string desturl = request->get("Destination");

				lyramilk::data::string host = request->scheme() + "://" + request->get("Host");

				if(desturl.compare(0,host.size(),host) == 0){
					desturl = desturl.substr(host.size());


					static lyramilk::data::coding* code_url = lyramilk::data::codes::instance()->getcoder("url");
					std::size_t pos_path_qmask = desturl.find('?');
					if(pos_path_qmask == desturl.npos){
						desturl = code_url->decode(desturl);
					}else{
						if(code_url){
							lyramilk::data::string path1 = desturl.substr(0,pos_path_qmask);
							lyramilk::data::string path2 = desturl.substr(pos_path_qmask + 1);
							desturl = code_url->decode(path1) + "?" + path2;
						}
					}
				}

				lyramilk::data::string dest;
				if(!url_to_localtion(desturl,&dest)){
					response->code = 403;
					return cs_ok;
				}

				if(rename(real.c_str(),dest.c_str()) ==0){
					response->code = 205;
					return cs_ok;
				}else{
					if(errno == ENOENT){
						response->code = 404;
						return cs_ok;
					}else if(errno == EACCES){
						response->code = 403;
						return cs_ok;
					}
					response->code = 500;
					return cs_ok;
				}
			}
			return cs_pass;
		}

		dispatcher_check_status hittest(httprequest* request,httpresponse* response,httpadapter* adapter)
		{
			dispatcher_check_status cs = cs_pass;
			if(!allowmethod.empty() && allowmethod.find("*") == allowmethod.npos){
				if(allowmethod.find(request->get(":method")) == allowmethod.npos){
					return cs_pass;
				}		
			}

			lyramilk::data::string realfile;
			if(!url_to_localtion(request->url(),&realfile)){
				return cs_pass;
			}

			cs = check_auth(request,response,adapter,realfile);
			if(cs == cs_pass){
				return call(request,response,adapter,realfile);
			}
			return cs;
		}
		static url_selector* ctr(void*)
		{
			return new url_selector_webdav;
		}

		static void dtr(url_selector* p)
		{
			delete (url_selector_webdav*)p;
		}
	};



	static __attribute__ ((constructor)) void __init()
	{
		url_selector_factory::instance()->define("webdav",url_selector_webdav::ctr,url_selector_webdav::dtr);
	}

}}
