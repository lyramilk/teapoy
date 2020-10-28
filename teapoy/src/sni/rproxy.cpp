#include <libmilk/scriptengine.h>
#include <libmilk/log.h>
#include <libmilk/dict.h>
#include <libmilk/netaio.h>
#include <libmilk/netproxy.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include "script.h"
#include "sni_selector.h"

#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>


namespace lyramilk{ namespace teapoy{ namespace native
{
	lyramilk::log::logss static log(lyramilk::klog,"teapoy.native.rproxy");
	const char RPROXY_MAGIC[] = "28f988dd-1dea-4df9-a6fd-c2fb8bfb2cab";

	class rproxy_aioserver;


	class rproxyserver_aiosession:public lyramilk::netio::aioproxysession
	{
	  public:
		unsigned int thread_idx;

		rproxy_aioserver* pubserver;
		rproxyserver_aiosession()
		{
			flag = EPOLLPRI | EPOLLERR | EPOLLHUP | EPOLLRDHUP | EPOLLONESHOT;
		}

	  	virtual ~rproxyserver_aiosession();

		virtual bool oninit(lyramilk::data::ostream& os);
		virtual bool onrequest(const char* cache, int size,int* bytesused, lyramilk::data::ostream& os);

	};


	class rproxy_aioserver:public lyramilk::netio::aioserver<rproxyserver_aiosession>
	{
		std::set<lyramilk::netio::socket*> masters;
		lyramilk::threading::mutex_rw lock;
		lyramilk::threading::mutex_os session_lock;

		std::set<rproxyserver_aiosession*> sessions;

	  public:
		virtual lyramilk::netio::aiosession* create()
		{
			rproxyserver_aiosession* client_session = lyramilk::netio::aiosession::__tbuilder<rproxyserver_aiosession>();
			client_session->pubserver = this;
			return client_session;
		}

		virtual bool add_master(lyramilk::netio::socket* mgr)
		{
			lyramilk::threading::mutex_sync _(lock.w());
			masters.insert(mgr);
			return true;
		}

		virtual bool remove_master(lyramilk::netio::socket* mgr)
		{
			lyramilk::threading::mutex_sync _(lock.w());
			std::set<lyramilk::netio::socket*>::iterator it = masters.find(mgr);
			if(it!=masters.end()){
				masters.erase(it);
				return true;
			}
			return false;
		}

		virtual bool add_session(rproxyserver_aiosession* session)
		{
			lyramilk::threading::mutex_sync _(session_lock);
			sessions.insert(session);
			return true;
		}

		virtual bool remove_session(rproxyserver_aiosession* session)
		{
			lyramilk::threading::mutex_sync _(session_lock);

			std::set<rproxyserver_aiosession*>::iterator it = sessions.find(session);
			if(it!=sessions.end()){
				sessions.erase(it);
				return true;
			}
			return false;
		}
		virtual bool accept_new_session(rproxyserver_aiosession* p)
		{
			if(!add_session(p)) return false;


			lyramilk::threading::mutex_sync _(lock.r());
			std::set<lyramilk::netio::socket*>::iterator it = masters.begin();
			if(it == masters.end()){
				remove_session(p);
				return false;
			}

			lyramilk::netio::socket* mgr = *it;
			unsigned long long lptr = (unsigned long long)reinterpret_cast<void*>(p);
			mgr->write((const char*)&lptr,sizeof(lptr));
			return true;
		}
	};

	bool rproxyserver_aiosession::oninit(lyramilk::data::ostream& os)
	{
		thread_idx = pool->get_thread_idx();
		return pubserver->accept_new_session(this);
	}

	rproxyserver_aiosession::~rproxyserver_aiosession()
	{
		pubserver->remove_session(this);
	}


	bool rproxyserver_aiosession::onrequest(const char* cache, int size,int* bytesused, lyramilk::data::ostream& os)
	{
		*bytesused = 0;
		return true;
	}

	class rproxyserver_aiosession_master:public lyramilk::netio::aioproxysession
	{
		bool is_upstream_master;
		lyramilk::data::string cache;
	  public:
		rproxy_aioserver* pubserver;


		rproxyserver_aiosession_master()
		{
			is_upstream_master = false;
		}

	  	virtual ~rproxyserver_aiosession_master()
		{
			if(is_upstream_master){
				pubserver->remove_master(this);
			}
		}

		virtual bool onrequest(const char* cache, int size,int* bytesused, lyramilk::data::ostream& os)
		{

			if((unsigned int)size >= sizeof(RPROXY_MAGIC) && memcmp(cache,RPROXY_MAGIC,sizeof(RPROXY_MAGIC)) == 0){
				is_upstream_master = true;
				*bytesused = sizeof(RPROXY_MAGIC);
				pubserver->add_master(this);
				return true;
			}

			if(size >= 8){
				*bytesused = 8;
				unsigned long long lptr = *(unsigned long long*)cache;
				rproxyserver_aiosession* client_session = reinterpret_cast<rproxyserver_aiosession*>(lptr);
				if(pubserver->remove_session(client_session)){

					pool->detach(client_session);

					combine(client_session);
					client_session->start_proxy();
					//pool->add_to_thread(get_thread_idx(),client_session,-1);
				}
			}else if(size >= 4){
				*bytesused = 4;
				//"PING"
			}

			return true;
		}

	};

	class rproxy_aioserver_master:public lyramilk::netio::aioserver<rproxyserver_aiosession_master>
	{
		rproxy_aioserver* pubserver;
		lyramilk::data::string upstream_host;
		unsigned short upstream_port;
	  public:
		virtual bool attach(const lyramilk::data::string& host,unsigned short port,rproxy_aioserver* srv)
		{
			upstream_host = host;
			upstream_port = port;
			pubserver = srv;
			return pubserver != nullptr;
		}
		virtual lyramilk::netio::aiosession* create()
		{
			rproxyserver_aiosession_master* master = lyramilk::netio::aiosession::__tbuilder<rproxyserver_aiosession_master>();
			master->pubserver = pubserver;
			return master;
		}
	};




	class rproxy_aioclient:public lyramilk::netio::aioproxysession_speedy
	{
		lyramilk::data::string server_host;
		unsigned short server_port;
	  public:
		rproxy_aioclient()
		{}
	  	virtual ~rproxy_aioclient()
		{}

		virtual bool open(const lyramilk::data::string& host,unsigned short port)
		{
			server_host = host;
			server_port = port;

			if(fd() >= 0){
				log(lyramilk::log::error,"open") << lyramilk::kdict("打开套接字(%s:%u)失败：%s",host.c_str(),port,"套接字己经打开。") << std::endl;
				return false;
			}
			hostent* h = gethostbyname(host.c_str());
			if(h == nullptr){
				log(lyramilk::log::error,"open") << lyramilk::kdict("打开套接字(%s:%u)失败：%s",host.c_str(),port,strerror(errno)) << std::endl;
				return false;
			}

			in_addr* inaddr = (in_addr*)h->h_addr;
			if(inaddr == nullptr){
				log(lyramilk::log::error,"open") << lyramilk::kdict("打开套接字(%s:%u)失败：%s",host.c_str(),port,strerror(errno)) << std::endl;
				return false;
			}

			int tmpsock = ::socket(AF_INET,SOCK_STREAM, IPPROTO_IP);
			if(tmpsock < 0){
				log(lyramilk::log::error,"open") << lyramilk::kdict("打开套接字(%s:%u)失败：%s",host.c_str(),port,strerror(errno)) << std::endl;
				return false;
			}

			sockaddr_in addr = {0};
			addr.sin_addr.s_addr = inaddr->s_addr;
			addr.sin_family = AF_INET;
			addr.sin_port = htons(port);


			if(0 == ::connect(tmpsock,(const sockaddr*)&addr,sizeof(addr))){
				this->fd(tmpsock);
				return true;
			}
			log(lyramilk::log::error,"open") << lyramilk::kdict("打开套接字(%s:%u)失败：%s",host.c_str(),port,strerror(errno)) << std::endl;
			::close(tmpsock);
			return false;
		}
	};

	// rproxyserver
	class rproxyserver:public epoll_selector
	{
		rproxy_aioserver ins;
		rproxy_aioserver_master upmaster;
	  public:
		static lyramilk::script::sclass* ctr(const lyramilk::data::array& args)
		{
			rproxyserver* p = new rproxyserver();
			return p;
		}

		static void dtr(lyramilk::script::sclass* p)
		{
			delete (rproxyserver*)p;
		}

		rproxyserver()
		{
		}

		virtual ~rproxyserver()
		{
		}

		lyramilk::data::var open_master(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			if(args.size() == 1){
				MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_int);
				return upmaster.open(args[0]) && upmaster.attach("0.0.0.0",args[0],&ins);
			}else if(args.size() == 2){
				MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
				MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_int);
				return upmaster.open(args[0].str(),args[1]) && upmaster.attach(args[0].str(),args[1],&ins);
			}

			return lyramilk::data::var::nil;
		}

		lyramilk::data::var open(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			if(args.size() == 1){
				MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_int);
				return ins.open(args[0]);
			}else if(args.size() == 2){
				MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
				MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_int);
				return ins.open(args[0].str(),args[1]);
			}

			return lyramilk::data::var::nil;
		}


	  	virtual bool onadd(lyramilk::io::aiopoll_safe* poll)
		{
			return poll->add(&upmaster) && poll->add(&ins);
		}
	};















	class rproxy_aiocmdclient:public lyramilk::netio::client
	{
	  public:
		lyramilk::io::aiopoll_safe* pool;
		lyramilk::data::string server_host;
		unsigned short server_port;
		lyramilk::data::string upstream_host;
		unsigned short upstream_port;
	  public:
		virtual bool init(const lyramilk::data::string& host,unsigned short port,const lyramilk::data::string& uhost,unsigned short uport)
		{
			server_host = host;
			server_port = port;
			upstream_host = uhost;
			upstream_port = uport;
			return true;
		}
		virtual bool reopen()
		{
			if(fd() >= 0){
				log(lyramilk::log::error,"init") << lyramilk::kdict("打开套接字(%s:%u)失败：%s",server_host.c_str(),server_port,"套接字己经打开。") << std::endl;
				return false;
			}
			hostent* h = gethostbyname(server_host.c_str());
			if(h == nullptr){
				log(lyramilk::log::error,"init") << lyramilk::kdict("打开套接字(%s:%u)失败：%s",server_host.c_str(),server_port,strerror(errno)) << std::endl;
				return false;
			}

			in_addr* inaddr = (in_addr*)h->h_addr;
			if(inaddr == nullptr){
				log(lyramilk::log::error,"init") << lyramilk::kdict("打开套接字(%s:%u)失败：%s",server_host.c_str(),server_port,strerror(errno)) << std::endl;
				return false;
			}

			int tmpsock = ::socket(AF_INET,SOCK_STREAM, IPPROTO_IP);
			if(tmpsock < 0){
				log(lyramilk::log::error,"init") << lyramilk::kdict("打开套接字(%s:%u)失败：%s",server_host.c_str(),server_port,strerror(errno)) << std::endl;
				return false;
			}

			sockaddr_in addr = {0};
			addr.sin_addr.s_addr = inaddr->s_addr;
			addr.sin_family = AF_INET;
			addr.sin_port = htons(server_port);


			if(0 == ::connect(tmpsock,(const sockaddr*)&addr,sizeof(addr))){
				this->fd(tmpsock);
				return true;
			}
			log(lyramilk::log::error,"init") << lyramilk::kdict("打开套接字(%s:%u)失败：%s",server_host.c_str(),server_port,strerror(errno)) << std::endl;
			::close(tmpsock);
			return false;
		}
	};

	static void* thread_task(void* _p)
	{
		rproxy_aiocmdclient* ins = (rproxy_aiocmdclient*)_p;


		while(true){
			if(!ins->reopen()){
				sleep(2);
				continue;
			}

			ins->write(RPROXY_MAGIC,sizeof(RPROXY_MAGIC));

			while(true){
				if(!ins->check_read(10000)){
					log(lyramilk::log::error,"debug") << lyramilk::kdict("check_read:通过") << std::endl;
					if(ins->write("PING",4) != 4){
						ins->close();
						break;

					}
					continue;
				}
				log(lyramilk::log::error,"debug") << lyramilk::kdict("check_read:通过") << std::endl;

				char cache[8];
				int size = ins->read(cache,sizeof(cache));
				if(size!=8){
					log(lyramilk::log::error,"debug") << lyramilk::kdict("close:size=%d,err=%s",size,strerror(errno)) << std::endl;
					ins->close();
					break;
				}
				rproxy_aioclient* c = rproxy_aioclient::__tbuilder<rproxy_aioclient>();
				if(!c->open(ins->server_host.c_str(),ins->server_port)){
					delete c;
					continue;
				}
				c->setnodelay(true);
				c->setkeepalive(20,3);
				c->setnoblock(true);

				c->write(cache,size);

				rproxy_aioclient* uc = rproxy_aioclient::__tbuilder<rproxy_aioclient>();
				if(!uc->open(ins->upstream_host.c_str(),ins->upstream_port)){
					delete uc;
					delete c;
					continue;
				}
				uc->setnodelay(true);
				uc->setkeepalive(20,3);
				uc->setnoblock(true);

				c->endpoint = uc;
				uc->endpoint = c;
				ins->pool->add(uc,0);
				ins->pool->add_to_thread(uc->get_thread_idx(),c,-1);
			}
		}



		pthread_exit(0);
		return nullptr;
	}

	// rproxyclient
	class rproxyclient:public epoll_selector
	{
		rproxy_aiocmdclient* ins;
		bool inited;
	  public:
		static lyramilk::script::sclass* ctr(const lyramilk::data::array& args)
		{
			rproxyclient* p = new rproxyclient();
			return p;
		}

		static void dtr(lyramilk::script::sclass* p)
		{
			delete (rproxyclient*)p;
		}

		rproxyclient()
		{
			ins = new rproxy_aiocmdclient;
			selector = nullptr;
			inited = false;
		}

		virtual ~rproxyclient()
		{
		}

		lyramilk::data::var open(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_int);
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,2,lyramilk::data::var::t_str);
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,3,lyramilk::data::var::t_int);
			if(inited) return false;

			if(ins->init(args[0].str(),args[1],args[2].str(),args[3])){
				inited = true;
				pthread_t id_1;
				pthread_create(&id_1,NULL,thread_task,ins);
				pthread_detach(id_1);
				return true;
			}
			return false;
		}


	  	virtual bool onadd(lyramilk::io::aiopoll_safe* poll)
		{
			ins->pool = poll;
			return true;
		}
	};

	static int define(lyramilk::script::engine* p)
	{
		{
			lyramilk::script::engine::functional_map fn;
			fn["open"] = lyramilk::script::engine::functional<rproxyserver,&rproxyserver::open>;
			fn["open_master"] = lyramilk::script::engine::functional<rproxyserver,&rproxyserver::open_master>;
			p->define("RProxyServer",fn,rproxyserver::ctr,rproxyserver::dtr);
		}
		{
			lyramilk::script::engine::functional_map fn;
			fn["open"] = lyramilk::script::engine::functional<rproxyclient,&rproxyclient::open>;
			p->define("RProxyUpstream",fn,rproxyclient::ctr,rproxyclient::dtr);
		}
		return 2;
	}

	static __attribute__ ((constructor)) void __init()
	{
		lyramilk::teapoy::script_interface_master::instance()->regist("rproxy",define);
	}
}}}
