#include "web.h"
#include <malloc.h>
#include <sys/epoll.h>
#include <libmilk/netio.h>

/*
void* operator new(std::size_t sz)
{
	void* p = malloc(sz);
	printf("%p malloc\n",p);
	return p;
}

void operator delete(void* p)
{
	printf("%p free\n",p);
	free(p);
}
*/


class su
{
  public:
	lyramilk::data::string s;
  	su(){}
  	virtual ~su(){}

};




class kq:public lyramilk::netio::aiosession,public su
{
	lyramilk::data::string e;
	lyramilk::netio::socket_ostream aos;
  public:
  	kq(){}
  	virtual ~kq(){}


	bool notify_in() 
	{
		char buff[4096];
		read(buff,4096);

		aos << s;
		aos.flush();
		//return pool->reset(this,EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP | EPOLLONESHOT);
		return false;
	}
	bool notify_out()
	{
		return true;
	}
	bool notify_hup()
	{
		return false;
	}
	bool notify_err()
	{
		return false;
	}
	bool notify_pri()
	{
		return false;
	}
	bool init()
	{
		aos.init(this);
		return true;
	}
};

class httpserver:public lyramilk::netio::aiolistener
{
	static void* ctr(const lyramilk::data::var::array& args)
	{
		return new httpserver();
	}
	static void dtr(void* p)
	{
		delete (httpserver*)p;
	}

	virtual lyramilk::netio::aiosession* create()
	{
		kq* ss = lyramilk::netio::aiosession::__tbuilder<kq>();
		ss->s = "HTTP/1.0 200 OK\r\nContent-Length: 3\r\n\r\nok\n";
		return ss;
	}
};

int main(int argc,char* argv[])
{
	lyramilk::io::aiopoll pool;

	httpserver* srv = new httpserver();

	srv->open(8412);
	pool.add(srv);
	pool.active(10);
	pool.svc();
	return 0;
}
