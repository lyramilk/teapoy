#include <libmilk/scriptengine.h>
#include <libmilk/log.h>
#include <libmilk/multilanguage.h>
#include <libmilk/netaio.h>
#include "script.h"
#include "web.h"
#include "env.h"
#include "processer_master.h"
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

namespace lyramilk{ namespace teapoy{ namespace native
{
	class httpserver:public lyramilk::netio::aiolistener
	{
		lyramilk::log::logss log;
		lyramilk::data::string root;
		lyramilk::data::string cacheroot;

		web::processer_master proc_master;
	  public:

		static void* ctr(lyramilk::data::var::array)
		{
			return new httpserver();
		}
		static void dtr(void* p)
		{
			delete (httpserver*)p;
		}

		httpserver():log(lyramilk::klog,"teapoy.server.httpserver")
		{
		}

		~httpserver()
		{}

		lyramilk::data::var open(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_uint16);

			proc_master.define("jsx",web::processer_jsx::ctr,web::processer_jsx::dtr);
			proc_master.define("js",web::processer_js::ctr,web::processer_js::dtr);

			lyramilk::data::var::array keys = web::methodinvokers::instance()->keys();
			for(lyramilk::data::var::array::iterator it = keys.begin();it!=keys.end();++it){
				web::methodinvoker* invoker = web::methodinvokers::instance()->get(it->str());
				if(invoker){
					invoker->set_processer(&proc_master);
				}
			}
			return lyramilk::netio::aiolistener::open(args[0]);
		}

		lyramilk::data::var bind_url(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_array);
			lyramilk::data::var::array& ar = args[0];
			lyramilk::data::var::array::iterator it = ar.begin();
			for(;it!=ar.end();++it){
				lyramilk::data::string type = it->at("type");
				lyramilk::data::string pattern = it->at("pattern");
				lyramilk::data::string module = it->at("module");
				proc_master.mapping(type,pattern,module);
			}
			return true;
		}

		lyramilk::data::var set_root(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_str);
			lyramilk::data::string str = args[0].str();
			lyramilk::data::string str_cache = args[1].str();

			lyramilk::data::string rawdir;
			std::size_t pos_end = str.find_last_not_of("/");
			root = str.substr(0,pos_end + 1);

			rawdir = root;
			rawdir.push_back('/');

			lyramilk::data::string rawdir_cache;
			std::size_t pos_cache_end = str_cache.find_last_not_of("/");
			cacheroot = str_cache.substr(0,pos_cache_end + 1);

			rawdir = root;
			rawdir.push_back('/');

			struct stat st = {0};
			if(0 !=::stat(rawdir.c_str(),&st)){
				log(lyramilk::log::warning,__FUNCTION__) << D("设置网站根目录%s失败：%s",str.c_str(),strerror(errno)) << std::endl;
				return false;
			}

			if(S_ISDIR(st.st_mode) || S_ISLNK(st.st_mode)){
				return true;
			}

			log(lyramilk::log::warning,__FUNCTION__) << D("设置网站根目录%s失败：该路径不是目录",str.c_str()) << std::endl;
			return false;
		}

		lyramilk::data::var set_index(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_array);
			env::set_config("web.index",args[0]);
			return true;
		}

		virtual lyramilk::netio::aiosession* create()
		{
			lyramilk::teapoy::web::aiohttpsession* ss = lyramilk::netio::aiosession::__tbuilder<lyramilk::teapoy::web::aiohttpsession>();
			ss->root = root;
			//ss->cacheroot = cacheroot;
			return ss;
		}
		
		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["open"] = lyramilk::script::engine::functional<httpserver,&httpserver::open>;
			fn["bind_url"] = lyramilk::script::engine::functional<httpserver,&httpserver::bind_url>;
			fn["set_root"] = lyramilk::script::engine::functional<httpserver,&httpserver::set_root>;
			fn["set_index"] = lyramilk::script::engine::functional<httpserver,&httpserver::set_index>;
			p->define("httpserver",fn,httpserver::ctr,httpserver::dtr);
			return 1;
		}

		static __attribute__ ((constructor)) void __init()
		{
			lyramilk::teapoy::script2native::instance()->regist("http",define);
		}
	};

}}}