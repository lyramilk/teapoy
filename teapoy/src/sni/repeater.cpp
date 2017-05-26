/*
#include <libmilk/scriptengine.h>
#include <libmilk/netaio.h>
#include "repeatersession.h"
#include "web.h"
#include "env.h"
#include "processer_master.h"
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
*/
#include <libmilk/log.h>
#include <libmilk/multilanguage.h>
#include <libmilk/thread.h>
#include <sys/poll.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include "repeatersession.h"
#include "script.h"

#include <unistd.h>
#include <netdb.h>

#include <errno.h>
#include <string.h>

namespace lyramilk{ namespace teapoy{ namespace native
{
	lyramilk::log::logss static log(lyramilk::klog,"lyramilk.teapoy.native.repeater");
	struct repeater_args
	{
		lyramilk::data::string srcip;
		lyramilk::data::string destip;
		lyramilk::data::uint16 destport;
		lyramilk::data::int32 expire;
		bool autoclose;
		int listener;
	};


	static int thread_task(repeater_args* p)
	{
		lyramilk::data::string srcip = p->srcip;
		lyramilk::data::string destip = p->destip;
		lyramilk::data::uint16 destport = p->destport;
		bool autoclose = p->autoclose;
		time_t tm_end = time(0) + p->expire;
		delete p;

		std::vector<pollfd> pfds;
		pfds.reserve(3);
		pollfd pfd;
		pfd.fd = p->listener;
		pfd.events = POLLIN;
		pfd.revents = 0;
		pfds.push_back(pfd);

		while(time(0) < tm_end){
			int ret = ::poll(pfds.data(),pfds.size(),200);
			if(ret > 0){
				for(std::size_t i=0;i<pfds.size();++i){
					pollfd &pfd = pfds[i];
					if(pfd.revents & POLLIN){
						if(i == 0){
							sockaddr_in addr;
							socklen_t addr_size = sizeof(addr);
							int sock = ::accept(pfd.fd,(sockaddr*)&addr,&addr_size);
							hostent* h = gethostbyname(destip.c_str());
							if(h == nullptr){
								::close(sock);
								TODO();
								return 0;
							}
							in_addr* inaddr = (in_addr*)h->h_addr;
							if(inaddr == nullptr){
								::close(sock);
								TODO();
								return 0;
							}
							int tmpsock = ::socket(AF_INET,SOCK_STREAM, IPPROTO_IP);
							sockaddr_in addr2 = {0};
							addr2.sin_addr.s_addr = inaddr->s_addr;
							addr2.sin_family = AF_INET;
							addr2.sin_port = htons(destport);

							if(0 != ::connect(tmpsock,(const sockaddr*)&addr2,sizeof(addr2))){
								::close(sock);
								::close(tmpsock);
								TODO();
								return 0;
							}
							{
								pollfd pfd;
								pfd.fd = sock;
								pfd.events = POLLIN;
								pfd.revents = 0;
								pfds.push_back(pfd);
							}
							{
								pollfd pfd;
								pfd.fd = tmpsock;
								pfd.events = POLLIN;
								pfd.revents = 0;
								pfds.push_back(pfd);
							}

							if(autoclose){
								::close(pfd.fd);
							}
						}else{
							int r = (i&1)?i+1:i-1;
						
							char buf[4096];
							int bytes = ::recv(pfds[i].fd,buf,sizeof(buf),0);
							if(bytes < 1){
								::close(pfds[i].fd);
								::close(pfds[r].fd);
							}else{
								::send(pfds[r].fd,buf,bytes,0);
							}
						
						}
					}
				}
			}
		}

		return 0;
	}

	lyramilk::data::var create_repeater(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env,void*)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_uint);	//监听端口
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_str);		//源ip
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,2,lyramilk::data::var::t_str);		//目标ip
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,3,lyramilk::data::var::t_uint);	//目标端口
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,4,lyramilk::data::var::t_bool);		//是否一次性
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,5,lyramilk::data::var::t_int);	//存活时间，秒

		unsigned short listenport = args[0];
		repeater_args* p = new repeater_args;
		p->srcip = args[1].str();
		p->destip = args[2].str();
		p->destport = args[3];
		p->autoclose = args[4];
		p->expire = args[5];

		int sock = ::socket(AF_INET,SOCK_STREAM, IPPROTO_IP);
		{
			sockaddr_in addr = {0};
			addr.sin_addr.s_addr = htonl(INADDR_ANY);
			addr.sin_family = AF_INET;
			addr.sin_port = htons(listenport);
			int opt = 1;
			setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
			if(::bind(sock,(const sockaddr*)&addr,sizeof(addr))){
				::close(sock);
				delete p;
				return false;
			}
			unsigned int argp = 1;
			ioctl(sock,FIONBIO,&argp);
			int ret = ::listen(sock,1);
			if(ret != 0){
				::close(sock);
				delete p;
				return false;
			}
		}

		p->listener = sock;

		pthread_t thread;
		if(pthread_create(&thread,NULL,(void* (*)(void*))thread_task,p) == 0){
			pthread_detach(thread);
			return true;
		}
		::close(sock);
		delete p;
		return false;
	}

	static int define(lyramilk::script::engine* p)
	{
		p->define("create_repeater",create_repeater);
		return 1;
	}

	static __attribute__ ((constructor)) void __init()
	{
		lyramilk::teapoy::script2native::instance()->regist("repeater",define);
	}

}}}