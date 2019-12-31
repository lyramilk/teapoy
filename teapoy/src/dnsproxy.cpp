#include "dnsproxy.h"
#include <libmilk/log.h>
#include <libmilk/dict.h>
#include <libmilk/stringutil.h>
#include <unistd.h>
#include <memory.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <sys/epoll.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <algorithm>
#include <linux/if_ether.h>


namespace lyramilk{ namespace teapoy {
	unsigned int dns_packet::copy(char* buff,unsigned int size) const
	{
		unsigned int rsize = 0;
		{
			unsigned int tsize = std::min((unsigned int)sizeof(dns_header),size);

			dns_header hh = header;
			hh.question = htons(1);
			hh.answer = htons(answers.size());
			hh.auth = htons(0);
			hh.addition = htons(0);
			memcpy(buff,&hh,tsize);
			rsize += tsize;
		}

		//
		{
			lyramilk::data::strings ds = lyramilk::data::split(queries.name,".");
			lyramilk::data::strings::iterator it = ds.begin();
			for(;it!=ds.end();++it){
				unsigned char tsize = (unsigned char)it->size();
				buff[rsize] = tsize;
				++rsize;
				rsize += it->copy(buff+rsize,size - rsize);
			}
			*(unsigned short*)&buff[rsize] = htons(1);
			rsize += 2;
			*(unsigned short*)&buff[rsize] = htons(1);
			rsize += 2;
		}
		//
		for(std::vector<dns_addr_item>::const_iterator answerit = answers.begin();answerit!=answers.end();++answerit){
			lyramilk::data::strings ds = lyramilk::data::split(answerit->name,".");
			lyramilk::data::strings::iterator it = ds.begin();
			for(;it!=ds.end();++it){
				unsigned char tsize = (unsigned char)it->size();
				buff[rsize] = tsize;
				++rsize;
				rsize += it->copy(buff+rsize,size - rsize);
			}
			*(unsigned short*)&buff[rsize] = htons(answerit->type);
			rsize += 2;
			*(unsigned short*)&buff[rsize] = htons(answerit->classic);
			rsize += 2;
			*(unsigned int*)&buff[rsize] = htonl(answerit->ttl);
			rsize += 4;

			if(answerit->type == 5){
				lyramilk::data::strings ds = lyramilk::data::split(answerit->addr,".");
				lyramilk::data::string str;
				{
					lyramilk::data::strings::iterator it = ds.begin();
					for(;it!=ds.end();++it){
						unsigned char tsize = (unsigned char)it->size();
						str.push_back(tsize);
						str.append(*it);
					}
				}
				*(unsigned short*)&buff[rsize] = htons(str.size());
				rsize += 2;
				rsize += str.copy(buff+rsize,size - rsize);
			}else if(answerit->type == 1){
				*(unsigned short*)&buff[rsize] = htons(answerit->datalength);
				rsize += 2;
				rsize += answerit->addr.copy(buff + rsize,size - rsize);
			}else{
				*(unsigned short*)&buff[rsize] = htons(answerit->datalength);
				rsize += 2;
				rsize += answerit->addr.copy(buff + rsize,size - rsize);
			}

		}
		return rsize;
	}


	int read_host_tail(lyramilk::data::string* ret,const char* base,const char* buff,unsigned int size)
	{
		const char *e = base + size;
		const char *p = buff;
		while(p < e){
			char c = *p;
			if((c&0xc0) == 0xc0){
				lyramilk::data::string rsubstr;
				unsigned short len = ntohs(*(unsigned short*)p)&0x3fff;
				read_host_tail(&rsubstr,base,base + len,size);
				p += 2;
				ret->append(rsubstr);
				break;
			}else{
				++p;
				unsigned int sz = 0xff&c;
				if(sz == 0) break;
				ret->append(p,sz);
				ret->push_back('.');
				p += sz;
			}

		}
		return int(p - buff);
	}

	const char* dns_addr_item::type_typename(unsigned short dnstype)
	{
		switch(dnstype){
		  case 1:
			return "A";
		  case 2:
			return "NS";
		  case 5:
			return "CNAME";
		  case 6:
			return "SOA";
		  case 11:
			return "WKS";
		  case 12:
			return "PTR";
		  case 13:
			return "HINFO";
		  case 15:
			return "MX";
		  case 28:
			return "AAAA";
		  case 252:
			return "AXFR";
		  case 255:
			return "ANY";
		}
		return "UNKNOW";
	}

	unsigned int dns_packet::from(const char* buff,unsigned int size)
	{
		memcpy(&header,buff,sizeof(header));

		queries.name.clear();
		const char* p = buff + sizeof(header);
		p += read_host_tail(&queries.name,buff,p,size);


		queries.type = ntohs(*(unsigned short*)p);
		queries.classic = ntohs(*(unsigned short*)(p+2));
		p += 4;


		int anser_count = ntohs(header.answer);
		for(int i =0;i<anser_count;++i){
			dns_addr_item ret;

			//读取域名
			p += read_host_tail(&ret.name,buff,p,size);
			//读取type和classic
			ret.type = ntohs(*(unsigned short*)p);
			ret.classic = ntohs(*(unsigned short*)(p + 2));
			p += 4;
			ret.ttl = ntohl(*(unsigned int*)(p));
			ret.datalength = ntohs(*(unsigned short*)(p + 4));
			p += 6;

			if(ret.type == 5){
				read_host_tail(&ret.addr,buff,p,size);
				p += ret.datalength;
			}else{
				ret.addr.assign((const char*)p,ret.datalength);
				p += ret.datalength;
			}


			answers.push_back(ret);
		}
		return int(p - buff);
	}

	inline bool check_read(int fd,unsigned int msec)
	{
		pollfd pfd;
		pfd.fd = fd;
		pfd.events = POLLIN;
		pfd.revents = 0;
		int ret = ::poll(&pfd,1,msec);
		if(ret > 0){
			if(pfd.revents & POLLIN){
				return true;
			}
		}
		return false;
	}

	bool dnsasync::notify_out()
	{
		return true;
	}

	bool dnsasync::notify_hup()
	{
		return true;
	}

	bool dnsasync::notify_err()
	{
		return true;
	}

	bool dnsasync::notify_pri()
	{
		return true;
	}

	bool dnsasync::notify_attach(lyramilk::io::aiopoll* container)
	{
		pool = container;
		return true;
	}

	bool dnsasync::notify_detach(lyramilk::io::aiopoll* container)
	{
		pool = nullptr;
		return true;
	}

	lyramilk::io::native_filedescriptor_type dnsasync::getfd()
	{
		return fd;
	}

	bool dnsasync::open(const char* host,unsigned short port)
	{
		hostent* h = gethostbyname(host);
		if(h == nullptr){
			lyramilk::klog(lyramilk::log::error,"lyramilk.teapoy.dnsasync.open") << lyramilk::kdict("绑定上游dns(%s:%u)失败：%s",host,port,strerror(errno)) << std::endl;
			return false;
		}

		in_addr* inaddr = (in_addr*)h->h_addr;
		if(inaddr == nullptr){
			lyramilk::klog(lyramilk::log::error,"lyramilk.teapoy.dnsasync.open") << lyramilk::kdict("绑定上游dns(%s:%u)失败：%s",host,port,strerror(errno)) << std::endl;
			return false;
		}

		addr.sin_addr.s_addr = inaddr->s_addr;
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);

		if (connect(fd,(struct sockaddr*)&addr,sizeof(struct sockaddr_in)) == -1){
			lyramilk::klog(lyramilk::log::error,"lyramilk.teapoy.dnsasync.open") << lyramilk::kdict("绑定上游dns(%s:%u)失败：%s",host,port,strerror(errno)) << std::endl;
		}
		return true;
	}

	bool dnsasync::open(const struct sockaddr* addr,socklen_t addr_len)
	{
		struct sockaddr_in* addr_in = (struct sockaddr_in*)addr;
		struct sockaddr_in saddr;
		saddr.sin_addr.s_addr = addr_in->sin_addr.s_addr;
		saddr.sin_family = addr_in->sin_family;
		saddr.sin_port = addr_in->sin_port;

		if (connect(fd,(struct sockaddr*)&saddr,addr_len) == -1){
			lyramilk::klog(lyramilk::log::error,"lyramilk.teapoy.dnsasync.open") << lyramilk::kdict("绑定上游dns(%s:%u)失败：%s",inet_ntoa(saddr.sin_addr),ntohs(saddr.sin_port),strerror(errno)) << std::endl;
			return true;
		}
		return false;
	}

	bool dnsasync::async_send(unsigned char *buf, int buf_size)
	{
		return ::send(fd,buf,buf_size,0);
	}


	dnsasync::dnsasync()
	{
		fd = -1;
	}

	dnsasync::~dnsasync()
	{
		if(fd != -1){
			::close(fd);
		}
	}

	//////////////////////////////////////////////////
	void dnsasynccache::ondestory()
	{
		lyramilk::klog(lyramilk::log::error,"lyramilk.teapoy.dnsasynccache.ondestory") << lyramilk::kdict("不应该到达这里") << std::endl;
		delete this;
	}
	bool dnsasynccache::notify_in()
	{
		char buff[65536];
		int r = ::recv(getfd(),buff,sizeof(buff),0);
		if(r > 0){
			dns_packet repcache;
			repcache.from(buff,r);
			lyramilk::klog(lyramilk::log::debug,"lyramilk.teapoy.dnsasynccache.onrequest") << lyramilk::kdict("响应：%s (异步透传)",repcache.queries.name.c_str()) << std::endl;
			lyramilk::threading::mutex_sync _(lock.w());
			dnscache[repcache.queries.name] = repcache;
		}
		return true;
	}

	dnsasynccache::dnsasynccache(lyramilk::threading::mutex_rw& l,dnscache_type& c):lock(l),dnscache(c)
	{
		fd = ::socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	}

	dnsasynccache::~dnsasynccache()
	{
	}

	//////////////////////////////////////////////////
	union ptr2short{
		char* p;
		unsigned short* s;
	};

	class dnsasynccall:public dnsasync
	{
		lyramilk::io::native_filedescriptor_type clientfd;
		struct sockaddr addr;
		socklen_t addr_len;
	  protected:
		lyramilk::threading::mutex_rw& lock;
		dnscache_type& dnscache;
	  protected:
		virtual void ondestory()
		{
			delete this;
		}
		virtual bool notify_in()
		{
			char buff[65536];
			int r = ::recv(getfd(),buff,sizeof(buff),0);
			if(r > 0){
				ptr2short s;
				s.p = buff;
				unsigned short dns_tcp_bytes = ntohs(*s.s);

				while(r < dns_tcp_bytes){
					int w = ::recv(getfd(),buff + r,sizeof(buff) - r,0);
					if(w > 0){
						r += w;
					}else{
						return false;
					}
				}
				r = sendto(clientfd,buff + 2,r - 2,0,(sockaddr*)&addr,addr_len);
				dns_packet repcache;
				repcache.from(buff + 2,r - 2);
				lyramilk::klog(lyramilk::log::debug,"lyramilk.teapoy.dnsasynccall.onrequest") << lyramilk::kdict("响应：%s (异步透传)",repcache.queries.name.c_str()) << std::endl;
				lyramilk::threading::mutex_sync _(lock.w());
				dnscache[repcache.queries.name] = repcache;
			}
			return false;
		}

	  public:
		dnsasynccall(lyramilk::io::native_filedescriptor_type cfd,const sockaddr* caddr,socklen_t addr_len,lyramilk::threading::mutex_rw& l,dnscache_type& c):lock(l),dnscache(c)
		{
			clientfd = cfd;
			this->addr_len = addr_len;
			memcpy(&addr,caddr,addr_len);
			fd = ::socket(PF_INET, SOCK_STREAM, 0);
		}

		bool async_send(unsigned char *buf, int buf_size)
		{
			unsigned short k_buf_size = htons(buf_size);
			::send(fd,&k_buf_size,sizeof(k_buf_size),0);
			return ::send(fd,buf,buf_size,0);
		}

		virtual ~dnsasynccall()
		{
		}
	};

	class dnsasynccall_nocache:public dnsasync
	{
		lyramilk::io::native_filedescriptor_type clientfd;
		struct sockaddr addr;
		socklen_t addr_len;
	  protected:
		virtual void ondestory()
		{
			delete this;
		}
		virtual bool notify_in()
		{
			char buff[65536];
			int r = ::recv(getfd(),buff,sizeof(buff),0);
			if(r > 0){
				ptr2short s;
				s.p = buff;
				unsigned short dns_tcp_bytes = ntohs(*s.s);

				while(r < dns_tcp_bytes){
					int w = ::recv(getfd(),buff + r,sizeof(buff) - r,0);
					if(w > 0){
						r += w;
					}else{
						return false;
					}
				}
				r = sendto(clientfd,buff + 2,r - 2,0,(sockaddr*)&addr,addr_len);
				dns_packet repcache;
				repcache.from(buff + 2,r - 2);
				lyramilk::klog(lyramilk::log::debug,"lyramilk.teapoy.dnsasynccall_nocache.onrequest") << lyramilk::kdict("响应：%s (异步透传)",repcache.queries.name.c_str()) << std::endl;
			}
			return false;
		}

	  public:
		dnsasynccall_nocache(lyramilk::io::native_filedescriptor_type cfd,const sockaddr* caddr,socklen_t addr_len)
		{
			clientfd = cfd;
			this->addr_len = addr_len;
			memcpy(&addr,caddr,addr_len);
			fd = ::socket(PF_INET, SOCK_STREAM, 0);
		}

		bool async_send(unsigned char *buf, int buf_size)
		{
			unsigned short k_buf_size = htons(buf_size);
			::send(fd,&k_buf_size,sizeof(k_buf_size),0);
			return ::send(fd,buf,buf_size,0);
		}

		virtual ~dnsasynccall_nocache()
		{
		}
	};

	//////////////////////////////////////////////////

	dnsproxy::dnsproxy()
	{
		dch = nullptr;
		serverfd = -1;
		attachdch = false;
	}

	dnsproxy::~dnsproxy()
	{
		if(serverfd != -1){
			::close(serverfd);
		}
		if(dch){
			delete dch;
		}
	}

	bool dnsproxy::onrequest(const char* cache, int size, lyramilk::data::ostream& os,const struct sockaddr* addr,socklen_t addr_len)
	{
		if(!attachdch){

			if(pool && dch){
				if(pool->add(dch,EPOLLIN|EPOLLONESHOT)){
					attachdch = true;
				}
			}
		}

		dns_packet req;
		req.from(cache,size);
		if(req.queries.type == 1){
			{
				lyramilk::threading::mutex_sync _(lock.r());
				dnscache_type::iterator it = dnscache.find(req.queries.name);
				if(it != dnscache.end()){
					it->second.header.transation_id = req.header.transation_id;
					it->second.header.rcode = 0;
					it->second.header.qr = 1;
					it->second.header.rd = 1;
					it->second.header.ra = 1;

					char buff[4096];
					int r = it->second.copy(buff,sizeof(buff));
					os.write(buff,r);
					if(dch)dch->async_send((unsigned char*)cache,size);
					lyramilk::klog(lyramilk::log::debug,"lyramilk.teapoy.dnsproxy.onrequest") << lyramilk::kdict("响应：%s %d条结果 (缓存)",it->second.queries.name.c_str(),it->second.answers.size()) << std::endl;
					return true;
				}
			}

			std::vector<dns_ipv4_result> ipv4_result;
			if(get_ipv4_by_host(req.queries.name,&ipv4_result)){
				dns_packet rep;
				rep.header.transation_id = req.header.transation_id;
				rep.header.rcode = 0;
				rep.header.qr = 1;
				rep.header.rd = 1;
				rep.header.ra = 1;

				rep.queries.name = req.queries.name;

				std::vector<dns_ipv4_result>::iterator it = ipv4_result.begin();
				for(;it!=ipv4_result.end();++it){
					dns_addr_item item;
					item.name = req.queries.name;
					item.addr.assign((const char*)&it->rdata.i,4);
					item.ttl = it->ttl;
					rep.answers.push_back(item);
				}


				char buff[4096];
				int rc = rep.copy(buff,sizeof(buff));
				if(rc > 0){
					lyramilk::klog(lyramilk::log::debug,"lyramilk.teapoy.dnsproxy.onrequest") << lyramilk::kdict("响应：%s %d条结果 (代理)",req.queries.name.c_str(),rep.answers.size()) << std::endl;
					os.write(buff,rc);
					return true;
				}
			}

			dnsasynccall* p = new dnsasynccall(getfd(),addr,addr_len,lock,dnscache);
			if(p->open(host.c_str(),port)){
				p->async_send((unsigned char*)cache,size);
				pool->add(p,EPOLLIN|EPOLLONESHOT);
				return true;
			}
			delete p;
		}

		dnsasynccall_nocache* p = new dnsasynccall_nocache(getfd(),addr,addr_len);
		if(p->open(host.c_str(),port)){
			p->async_send((unsigned char*)cache,size);
			pool->add(p,EPOLLIN|EPOLLONESHOT);
			return true;
		}else{
			delete p;
			char buff[4096];
			{
				int r = sync_call(req.queries.name,cache,size,buff,sizeof(buff));
				if(r > 0){
					os.write(buff,r);
					lyramilk::klog(lyramilk::log::debug,"lyramilk.teapoy.dnsproxy.onrequest") << lyramilk::kdict("响应：%s (透传)",req.queries.name.c_str()) << std::endl;
					return true;
				}
			}
			dns_packet rep;
			rep.header.transation_id = req.header.transation_id;
			rep.header.qr = 1;
			rep.header.rcode = 3;
			int r = rep.copy(buff,sizeof(buff));
			if(r > 0){
				lyramilk::klog(lyramilk::log::debug,"lyramilk.teapoy.dnsproxy.onrequest") << lyramilk::kdict("响应：%s 失败",req.queries.name.c_str()) << std::endl;
				os.write(buff,r);
			}
		}
		return true;
	}

	
	int dnsproxy::sync_call(const lyramilk::data::string& host,const char *buf, int buf_size,char *rdata,int rdata_size)
	{
		dns_header* qh = (dns_header*)buf;
		dns_header* ph = (dns_header*)rdata;

		::send(serverfd,buf,buf_size,0);
		while(true){
			if(check_read(serverfd,60)){
				int r = ::recv(serverfd,rdata,rdata_size,0);
				if(r > 0){
					if(qh->transation_id == ph->transation_id){
						return r;
					}
				}
			}else{
				break;
			}
		}

		return -1;
	}

	bool dnsproxy::bind_upstream_dns(const lyramilk::data::string& host,unsigned short port)
	{
		hostent* h = gethostbyname(host.c_str());
		if(h == nullptr){
			lyramilk::klog(lyramilk::log::error,"lyramilk.teapoy.dnsasynccache.open") << lyramilk::kdict("绑定上游dns(%s:%u)失败：%s",host.c_str(),port,strerror(errno)) << std::endl;
			return false;
		}

		in_addr* inaddr = (in_addr*)h->h_addr;
		if(inaddr == nullptr){
			lyramilk::klog(lyramilk::log::error,"lyramilk.teapoy.dnsasynccache.open") << lyramilk::kdict("绑定上游dns(%s:%u)失败：%s",host.c_str(),port,strerror(errno)) << std::endl;
			return false;
		}

		upstream.sin_addr.s_addr = inaddr->s_addr;
		upstream.sin_family = AF_INET;
		upstream.sin_port = htons(port);

		if(serverfd != -1) ::close(serverfd);
		serverfd = ::socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

		if (connect(serverfd,(struct sockaddr*)&upstream,sizeof(struct sockaddr_in)) == -1){
			lyramilk::klog(lyramilk::log::error,"lyramilk.teapoy.dnsasynccache.open") << lyramilk::kdict("绑定上游dns(%s:%u)失败：%s",host.c_str(),port,strerror(errno)) << std::endl;
		}

		if(dch) delete dch;
		dch = new dnsasynccache(lock,dnscache);
		if(dch->open(host.c_str(),port)){
			this->host = host;
			this->port = port;
			return true;
		}
		if(dch) delete dch;
		dch = nullptr;
		return false;
	}

	bool dnsproxy::get_ipv4_by_host(const lyramilk::data::string& host,std::vector<dns_ipv4_result>* ipv4arr)
	{
		/*
		if(host == "www.baidu.com."){

			dns_ipv4_result r;
			r.ttl = 30;
			r.rdata.a = 180;
			r.rdata.b = 101;
			r.rdata.c = 49;
			r.rdata.d = 12;
			ipv4arr->push_back(r);
			return true;
		}
		if(host == "www.kuwo.cn."){
			dns_ipv4_result r;
			r.ttl = 30;
			r.rdata.a = 115;
			r.rdata.b = 28;
			r.rdata.c = 132;
			r.rdata.d = 183;
			ipv4arr->push_back(r);
			return true;
		}
		if(host == "m.kuwo.cn."){

			dns_ipv4_result r;
			r.ttl = 30;
			r.rdata.a = 115;
			r.rdata.b = 28;
			r.rdata.c = 132;
			r.rdata.d = 183;
			ipv4arr->push_back(r);
			return true;
		}
		if(host == "abcdefg."){

			dns_ipv4_result r;
			r.ttl = 30;
			r.rdata.a = 115;
			r.rdata.b = 28;
			r.rdata.c = 132;
			r.rdata.d = 183;
			ipv4arr->push_back(r);
			return true;
		}
		if(host == "resource.mudis.milk."){

			dns_ipv4_result r;
			r.ttl = 30;
			r.rdata.a = 192;
			r.rdata.b = 168;
			r.rdata.c = 220;
			r.rdata.d = 98;
			ipv4arr->push_back(r);
			return true;
		}
		*/
		return false;
	}
}}
