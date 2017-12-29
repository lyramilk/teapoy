#include <teapoy/web.h>


class sssssssssssssss : public lyramilk::teapoy::web::url_worker
{
	bool call(lyramilk::teapoy::web::session_info* si) const
	{
		lyramilk::data::stringstream ss;
		ss << "Method=" << si->req->entityframe->method << "访问的url是：" << si->req->entityframe->uri << "参数是：" << si->req->entityframe->params();;
		si->rep->set("Content-Type","text/html;charset=utf-8");
		si->rep->send_header_and_body(200,ss.str().c_str(),ss.str().size());
		return true;
	}
	virtual bool init_extra(const lyramilk::data::var& extra)
	{
		return true;
	}
  public:
	sssssssssssssss()
	{
	}
	virtual ~sssssssssssssss()
	{
	}

	static url_worker* ctr(void*)
	{
		return new sssssssssssssss();
	}

	static void dtr(url_worker* p)
	{
		delete (sssssssssssssss*)p;
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
		lyramilk::teapoy::web::aiohttpsession* ss = lyramilk::netio::aiosession::__tbuilder<lyramilk::teapoy::web::aiohttpsession>();
		ss->worker = &worker;
		ss->sessionmgr = lyramilk::teapoy::web::sessions::defaultinstance();
		return ss;
	}
  public:
	lyramilk::teapoy::web::website_worker worker;
};

int main(int argc,char* argv[])
{
	lyramilk::teapoy::web::url_worker_master::instance()->define("ag3",sssssssssssssss::ctr,sssssssssssssss::dtr);

	lyramilk::io::aiopoll pool;

	httpserver* srv = new httpserver();


	lyramilk::teapoy::web::url_worker *w = lyramilk::teapoy::web::url_worker_master::instance()->create("ag3");

	lyramilk::data::var::array index;
	//index.push_back("index.html");
	w->init("GET,POST","/.*","",index,nullptr);

	srv->worker.lst.push_back(w);
	srv->open(80);
	pool.add(srv);
	pool.active(10);
	pool.svc();
	return 0;
}
