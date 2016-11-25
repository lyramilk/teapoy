#include "processer_master.h"
#include "script.h"
#include <libmilk/log.h>
#include <libmilk/multilanguage.h>
#include <libmilk/testing.h>

namespace lyramilk{ namespace teapoy { namespace web {
	/********** engine_master_lua ***********/

	class engine_master_lua:public lyramilk::script::engines
	{
	  public:
		engine_master_lua()
		{
		}

		virtual ~engine_master_lua()
		{
		}

		static engine_master_lua* instance()
		{
			static engine_master_lua _mm;
			return &_mm;
		}

		virtual lyramilk::script::engine* underflow()
		{
			lyramilk::script::engine* eng_tmp = lyramilk::script::engine::createinstance("lua");
			if(!eng_tmp){
				lyramilk::klog(lyramilk::log::error,"teapoy.web.engine_master_lua.underflow") << D("创建引擎对象失败(%s)","lua") << std::endl;
				return nullptr;
			}

			lyramilk::teapoy::script2native::instance()->fill(true,eng_tmp);
			return eng_tmp;
		}

		virtual void onfire(lyramilk::script::engine* o)
		{
			o->reset();
		}

		virtual void onremove(lyramilk::script::engine* o)
		{
			lyramilk::script::engine::destoryinstance("lua",o);
		}
	};


	/********** processer_lua ***********/
	processer* processer_lua::ctr(void* args)
	{
		return new processer_lua;
	}

	void processer_lua::dtr(processer* ptr)
	{
		delete ptr;
	}

	processer_lua::processer_lua()
	{}

	processer_lua::~processer_lua()
	{}
	
	bool processer_lua::invoke(lyramilk::data::string proc_file,methodinvoker* invoker,lyramilk::teapoy::http::request* req,std::ostream& os)
	{
		//debug
		lyramilk::data::string k = D("%s -> %s ",req->url.c_str(),proc_file.c_str());
		lyramilk::debug::nsecdiff td;
		lyramilk::debug::clocktester _d(td,lyramilk::klog(lyramilk::log::debug),k);

		engine_master_lua::ptr p = engine_master_lua::instance()->get();

		if(!p->load_file(false,proc_file)){
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
		lyramilk::data::var vret = p->call(false,"onrequest",ar);
		if(vret.type_like(lyramilk::data::var::t_bool)){
			return vret;
		}
		return false;
	}
}}}
