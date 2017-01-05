#include "processer_master.h"
#include "script.h"
#include <libmilk/log.h>
#include <libmilk/multilanguage.h>
#include <libmilk/testing.h>

namespace lyramilk{ namespace teapoy { namespace web {
	/********** engine_master_php ***********/

	class engine_master_php:public lyramilk::script::engines
	{
	  public:
		engine_master_php()
		{
		}

		virtual ~engine_master_php()
		{
		}

		static engine_master_php* instance()
		{
			static engine_master_php _mm;
			return &_mm;
		}

		virtual lyramilk::script::engine* underflow()
		{
			lyramilk::script::engine* eng_tmp = lyramilk::script::engine::createinstance("php");
			if(!eng_tmp){
				lyramilk::klog(lyramilk::log::error,"teapoy.web.engine_master_php.underflow") << D("创建引擎对象失败(%s)","php") << std::endl;
				return nullptr;
			}

			lyramilk::teapoy::script2native::instance()->fill(eng_tmp);
			return eng_tmp;
		}

		virtual void onfire(lyramilk::script::engine* o)
		{
			o->set("clearonreset",lyramilk::data::var::map());
			o->reset();
		}

		virtual void onremove(lyramilk::script::engine* o)
		{
			lyramilk::script::engine::destoryinstance("php",o);
		}
	};


	/********** processer_php ***********/
	processer* processer_php::ctr(void* args)
	{
		return new processer_php;
	}

	void processer_php::dtr(processer* ptr)
	{
		delete ptr;
	}

	processer_php::processer_php()
	{}

	processer_php::~processer_php()
	{}
	
	bool processer_php::invoke(lyramilk::data::string proc_file,methodinvoker* invoker,lyramilk::teapoy::http::request* req,std::ostream& os)
	{
		//debug
		lyramilk::data::string k = D("%s -> %s ",req->url.c_str(),proc_file.c_str());
		lyramilk::debug::nsecdiff td;
		lyramilk::debug::clocktester _d(td,lyramilk::klog(lyramilk::log::debug,"teapoy.web.php"),k);

		engine_master_php::ptr p = engine_master_php::instance()->get();

		if(!p->load_file(proc_file)){
			COUT << "加载文件" << proc_file << "失败" << std::endl;
		}

		processer_args args(proc_file,invoker,req,os);

		lyramilk::data::var::array ar;
		{
			lyramilk::data::var var_processer_args("__http_processer_args",&args);

			lyramilk::data::var::array args;
			args.push_back(var_processer_args);

			ar.push_back(p->createobject("HttpRequest",args));
			ar.push_back(p->createobject("HttpResponse",args));

		}
		lyramilk::data::var vret = p->call("onrequest",ar);
		if(vret.type_like(lyramilk::data::var::t_bool)){
			return vret;
		}
		return false;
	}
}}}
