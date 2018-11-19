#include "web.h"
#include "script.h"
#include "stringutil.h"
#include <libmilk/log.h>
#include <libmilk/dict.h>
#include <curl/curl.h>
#include <curl/easy.h>
#include <stdlib.h>

namespace lyramilk{ namespace teapoy { namespace web {


	class url_worker_proxy_loger
	{
		lyramilk::debug::nsecdiff td;
		lyramilk::teapoy::http::request* req;
		lyramilk::data::string prefix;
		lyramilk::data::string newuri;
	  public:
		url_worker_proxy_loger(lyramilk::data::string prefix,lyramilk::data::string newuri,lyramilk::teapoy::http::request* req)
		{
			td.mark();
			this->prefix = prefix;
			this->newuri = newuri;
			this->req = req;
		}
		~url_worker_proxy_loger()
		{
			lyramilk::klog(lyramilk::log::trace,prefix) << D("%s:%u-->%s 耗时%.3f(毫秒)",req->dest().c_str(),req->dest_port(),newuri.c_str(),double(td.diff()) / 1000000) << std::endl;
		}
	};




	class url_worker_proxy : public url_worker
	{
		lyramilk::data::string uriprefix;
		lyramilk::data::string fullhost;
		lyramilk::data::string host;
		lyramilk::data::uint16 port;

		bool call(lyramilk::teapoy::web::session_info* si) const
		{
			lyramilk::data::string url = uriprefix + si->req->entityframe->uri;
			url_worker_proxy_loger _("teapoy.web.proxy",url,si->req);
			lyramilk::data::stringstream ss;
			{
				ss << si->req->entityframe->method << " " << si->req->entityframe->uri << " HTTP/1.0\r\n";
				lyramilk::data::string header(si->req->entityframe->ptr(),si->req->entityframe->size());
				lyramilk::data::string lowercase_header = lyramilk::teapoy::lowercase(header);
				std::size_t pos = lowercase_header.find("host");
				if(pos != lowercase_header.npos){
					std::size_t pos_crln = lowercase_header.find("\r\n",pos);
					if(pos_crln != lowercase_header.npos){
						ss.write(header.c_str(),pos);
						ss << "Host: " << fullhost << "\r\n";
						ss.write(header.c_str() + pos_crln + 2,header.size() - pos_crln - 2);
					}else{
						ss.write(header.c_str(),pos);
					}
				}else{
					ss.write(si->req->entityframe->ptr(),si->req->entityframe->size());
				}

			}
			if(si->req->entityframe->body && si->req->entityframe->body->ptr() && si->req->entityframe->body->size()){
				ss.write(si->req->entityframe->body->ptr(),si->req->entityframe->body->size());
			}

			lyramilk::netio::client c;
			c.open(host,port);
			c.write(ss.str().c_str(),ss.str().size());

			char buff[65536];

			while(true){
				int r = c.read(buff,sizeof(buff));
				if(r > 0){
					si->rep->send(buff,r);
				}else{
					break;
				}
			}
			return true;
		}
		virtual bool init_extra(const lyramilk::data::var& extra)
		{
			const lyramilk::data::var& vhost = extra.path("/extend/host");
			if(vhost.type_like(lyramilk::data::var::t_int)){
				host = "127.0.0.1";
			}else{
				host = extra.path("/extend/host").str();
			}

			const lyramilk::data::var& vport = extra.path("/extend/port");
			if(vport.type_like(lyramilk::data::var::t_int)){
				port = 80;
			}else{
				port = extra.path("/extend/port");
			}

			lyramilk::data::stringstream ssurl;
			ssurl << host;
			if(port != 80 && port != 0){
				ssurl << ":" << port;
			}

			fullhost = ssurl.str();
			uriprefix = "http://" + fullhost;
			return true;
		}
	  public:
		url_worker_proxy()
		{
		}
		virtual ~url_worker_proxy()
		{
		}

		static url_worker* ctr(void*)
		{
			return new url_worker_proxy();
		}

		static void dtr(url_worker* p)
		{
			delete (url_worker_proxy*)p;
		}
	};



	static __attribute__ ((constructor)) void __init()
	{
		url_worker_master::instance()->define("proxy",url_worker_proxy::ctr,url_worker_proxy::dtr);
	}
}}}
