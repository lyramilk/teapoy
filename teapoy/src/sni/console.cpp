#include <libmilk/scriptengine.h>
#include <libmilk/log.h>
#include <libmilk/dict.h>
#include "script.h"
#include <fstream>
#include <sys/stat.h>

namespace lyramilk{ namespace teapoy
{
	static lyramilk::log::logss log(lyramilk::klog,"teapoy.console");

	inline lyramilk::script::engine* get_script_engine_by_envmap(const lyramilk::data::map& env)
	{
	
		lyramilk::data::map::const_iterator it_env_eng = env.find(lyramilk::script::engine::s_env_engine());
		if(it_env_eng != env.end()){
			lyramilk::data::datawrapper* urd = it_env_eng->second.userdata();
			if(urd && urd->name() == lyramilk::script::engine_datawrapper::class_name()){
				lyramilk::script::engine_datawrapper* urdp = (lyramilk::script::engine_datawrapper*)urd;
				return urdp->eng;
			}
		}
		return nullptr;
	}

	class console:public lyramilk::script::sclass
	{
	  public:

		static lyramilk::script::sclass* ctr(const lyramilk::data::array& args)
		{
			return new console();
		}
		static void dtr(lyramilk::script::sclass* p)
		{
			delete (console*)p;
		}

		console()
		{
		}

		virtual ~console()
		{}

		lyramilk::data::var log(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			lyramilk::data::string str;
			str.reserve(4096);
			for(lyramilk::data::array::const_iterator it = args.begin();it!=args.end();++it){
				str += it->str();
			}
			str.push_back('\n');

			long long sz = str.size();
			long long cur = 0;
			while(cur < sz){
				int wd = fwrite(str.c_str() + cur,1,sz - cur,stdout);
				if(wd > 0 && (wd + cur) <= sz){
					cur += wd;
				}else{
				}
			}
			return true;
		}

		lyramilk::data::var logx(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			if(args.size() == 0 ) return true;
			lyramilk::data::string clr = args[0];

			lyramilk::data::string str;

			bool usecolor = false;
			if(false){
			}else if(clr == "black"){
				usecolor = true;
				str += "\x1b[30m";
			}else if(clr == "red"){
				usecolor = true;
				str += "\x1b[31m";
			}else if(clr == "green"){
				usecolor = true;
				str += "\x1b[32m";
			}else if(clr == "yellow"){
				usecolor = true;
				str += "\x1b[33m";
			}else if(clr == "blue"){
				usecolor = true;
				str += "\x1b[34m";
			}else if(clr == "magenta"){
				usecolor = true;
				str += "\x1b[35m";
			}else if(clr == "cyan"){
				usecolor = true;
				str += "\x1b[36m";
			}else if(clr == "white"){
				usecolor = true;
				str += "\x1b[37m";
			}

			str.reserve(4096);
			for(lyramilk::data::array::const_iterator it = args.begin() + 1;it!=args.end();++it){
				str += it->str();
			}
			str.push_back('\n');

			long long sz = str.size();
			long long cur = 0;
			while(cur < sz){
				int wd = fwrite(str.c_str() + cur,1,sz - cur,stdout);
				if(wd > 0 && (wd + cur) <= sz){
					cur += wd;
				}else{
				}
			}

			if(usecolor){
				fwrite("\x1b[0m",1,sizeof("\x1b[0m") - 1,stdout);
			}
			return true;
		}

		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["log"] = lyramilk::script::engine::functional<console,&console::log>;
			fn["logx"] = lyramilk::script::engine::functional<console,&console::logx>;
			p->define("console",fn,console::ctr,console::dtr);
			return 1;
		}
	};

	class logger:public lyramilk::script::sclass
	{
	  public:
		static lyramilk::script::sclass* ctr(const lyramilk::data::array& args)
		{
			return new logger();
		}
		static void dtr(lyramilk::script::sclass* p)
		{
			delete (logger*)p;
		}

		logger()
		{
		}

		virtual ~logger()
		{}

		lyramilk::data::var printdebug(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			lyramilk::script::engine* e = get_script_engine_by_envmap(env);
			lyramilk::data::string mod = "logger@" + e->filename();

			lyramilk::data::string str;
			str.reserve(4096);
			for(lyramilk::data::array::const_iterator it = args.begin();it!=args.end();++it){
				str += it->str();
			}
			lyramilk::klog(lyramilk::log::debug,mod) << str << std::endl;
			return true;
		}

		lyramilk::data::var printtrace(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			lyramilk::script::engine* e = get_script_engine_by_envmap(env);
			lyramilk::data::string mod = "logger@" + e->filename();

			lyramilk::data::string str;
			str.reserve(4096);
			for(lyramilk::data::array::const_iterator it = args.begin();it!=args.end();++it){
				str += it->str();
			}
			lyramilk::klog(lyramilk::log::trace,mod) << str << std::endl;
			return true;
		}

		lyramilk::data::var printerror(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			lyramilk::script::engine* e = get_script_engine_by_envmap(env);
			lyramilk::data::string mod = "logger@" + e->filename();

			lyramilk::data::string str;
			str.reserve(4096);
			for(lyramilk::data::array::const_iterator it = args.begin();it!=args.end();++it){
				str += it->str();
			}
			lyramilk::klog(lyramilk::log::error,mod) << str << std::endl;
			return true;
		}

		lyramilk::data::var printwarning(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			lyramilk::script::engine* e = get_script_engine_by_envmap(env);
			lyramilk::data::string mod = "logger@" + e->filename();

			lyramilk::data::string str;
			str.reserve(4096);
			for(lyramilk::data::array::const_iterator it = args.begin();it!=args.end();++it){
				str += it->str();
			}
			lyramilk::klog(lyramilk::log::warning,mod) << str << std::endl;
			return true;
		}

		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["t"] = lyramilk::script::engine::functional<logger,&logger::printtrace>;
			fn["d"] = lyramilk::script::engine::functional<logger,&logger::printdebug>;
			fn["e"] = lyramilk::script::engine::functional<logger,&logger::printerror>;
			fn["w"] = lyramilk::script::engine::functional<logger,&logger::printwarning>;
			p->define("logger",fn,logger::ctr,logger::dtr);
			return 1;
		}
	};

	static int define(lyramilk::script::engine* p)
	{
		int i = 0;
		{
			i += console::define(p);
			i += logger::define(p);
		}
		return i;
	}

	static __attribute__ ((constructor)) void __init()
	{
		lyramilk::teapoy::script_interface_master::instance()->regist("console",define);
	}
}}