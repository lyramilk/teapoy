#include <libmilk/scriptengine.h>
#include <libmilk/log.h>
#include <libmilk/multilanguage.h>
#include "script.h"
#include <fstream>
#include <sys/stat.h>

#pragma push_macro("D")
#undef D
#include "dbconnpool.h"
#pragma pop_macro("D")

namespace lyramilk{ namespace teapoy{ namespace native
{
	static lyramilk::log::logss log(lyramilk::klog,"teapoy.native");

	lyramilk::data::var test(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env,void*)
	{
		if(args.size() > 0) return args[0];
		return "test";
	}

	class tester
	{
	  public:
		static void* ctr(const lyramilk::data::var::array& args)
		{
			return new tester();
		}
		static void dtr(void* p)
		{
			delete (tester*)p;
		}

		tester()
		{
		}

		~tester()
		{
		}
		lyramilk::data::var test(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			if(args.size() > 0) return args[0];
			return "test";
		}
		lyramilk::data::var clone(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			lyramilk::script::engine* e = (lyramilk::script::engine*)env.find(lyramilk::script::engine::s_env_engine())->second.userdata(lyramilk::script::engine::s_user_engineptr());
			lyramilk::data::var::array ar;
			return e->createobject("tester",ar);
		}
	  public:
		static int define(lyramilk::script::engine* p)
		{
			{
				lyramilk::script::engine::functional_map fn;
				fn["test"] = lyramilk::script::engine::functional<tester,&tester::test>;
				fn["clone"] = lyramilk::script::engine::functional<tester,&tester::clone>;
				p->define("tester",fn,tester::ctr,tester::dtr);
			}
			return 1;
		}
	};

	lyramilk::data::var echo(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env,void*)
	{
		lyramilk::data::string str;
		str.reserve(4096);
		for(lyramilk::data::var::array::const_iterator it = args.begin();it!=args.end();++it){
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

	lyramilk::data::var trace(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env,void*)
	{
		lyramilk::script::engine* e = (lyramilk::script::engine*)env.find(lyramilk::script::engine::s_env_engine())->second.userdata(lyramilk::script::engine::s_user_engineptr());
		lyramilk::data::string mod = "s=" + e->filename();

		lyramilk::data::string str;
		str.reserve(4096);
		for(lyramilk::data::var::array::const_iterator it = args.begin();it!=args.end();++it){
			str += it->str();
		}

		lyramilk::klog(lyramilk::log::trace,mod) << str << std::endl;
		return true;
	}

	lyramilk::data::var slog(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env,void*)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
		lyramilk::script::engine* e = (lyramilk::script::engine*)env.find(lyramilk::script::engine::s_env_engine())->second.userdata(lyramilk::script::engine::s_user_engineptr());
		lyramilk::data::string mod = "s=" + e->filename();
		lyramilk::data::string logtype = args[0];

		lyramilk::log::type t = lyramilk::log::trace;
		if(logtype == "debug"){
			t = lyramilk::log::debug;
		}else if(logtype == "trace"){
			t = lyramilk::log::trace;
		}else if(logtype == "warning"){
			t = lyramilk::log::warning;
		}else if(logtype == "error"){
			t = lyramilk::log::error;
		}else{
			throw lyramilk::exception(D("错误的日志类型"));
		}

		lyramilk::data::string str;
		for(lyramilk::data::var::array::const_iterator it = args.begin() + 1;it!=args.end();++it){
			str += it->str();
		}
		lyramilk::klog(t,mod) << str << std::endl;
		return true;
	}

	class file
	{
		lyramilk::log::logss log;
		FILE* fp;
		lyramilk::data::string filename;
	  public:
		static void* ctr(const lyramilk::data::var::array& args)
		{
			if(args.size() == 0) return new file();
			if(args.size() == 1){
				file* fp = new file();
				if(!fp) return nullptr;
				fp->open(args[0].str().c_str());
				return fp;
			}else if(args.size() == 2){
				file* fp = new file();
				if(!fp) return nullptr;
				fp->open(args[0].str().c_str(),args[1].str().c_str());
				return fp;
			}
			return nullptr;
		}
		static void dtr(void* p)
		{
			delete (file*)p;
		}

		file():log(lyramilk::klog,"teapoy.native.file")
		{
			fp = nullptr;
		}

		~file()
		{
			if(fp) fclose(fp);
		}

		bool open(const char* filename)
		{
			this->filename = filename;
			if(fp) fclose(fp);
			fp = fopen(filename,"r");
			return fp != nullptr;
		}


		bool open(const char* filename,const char* mode)
		{
			this->filename = filename;
			if(fp) fclose(fp);
			fp = fopen(filename,mode);
			return fp != nullptr;
		}

		lyramilk::data::var open(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			if(args.size() == 1){
				return open(args[0].str().c_str());
			}
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_str);
			return open(args[0].str().c_str(),args[1].str().c_str());
		}

		lyramilk::data::var isdir(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			struct stat st = {0};
			if(stat(filename.c_str(),&st) == -1){
				return false;
			}
			return (st.st_mode & S_IFDIR) != 0;
		}

		lyramilk::data::var close(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			::fclose(fp);
			return true;
		}

		lyramilk::data::var good(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			return fp != nullptr;
		}

		lyramilk::data::var read(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_uint);
			if(!fp) throw lyramilk::exception(D("文件未打开"));

			lyramilk::data::uint32 buffsize = args[0];
			lyramilk::data::chunk cb(buffsize,0);

			lyramilk::data::uint32 r = fread((char*)cb.c_str(),1,buffsize,fp);
			if(r < buffsize){
				cb.erase(cb.begin() + r,cb.end());
			}
			return cb;
		}

		lyramilk::data::var reads(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_uint);
			if(!fp) throw lyramilk::exception(D("文件未打开"));

			lyramilk::data::uint32 buffsize = args[0];
			lyramilk::data::string cb(buffsize,0);

			lyramilk::data::uint32 r = fread((char*)cb.c_str(),1,buffsize,fp);
			if(r < buffsize){
				cb.erase(cb.begin() + r,cb.end());
			}
			return cb;
		}

		lyramilk::data::var readline(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			if(!fp) throw lyramilk::exception(D("文件未打开"));
			lyramilk::data::string ret;
			char* p = nullptr;
			char buff[4096] = {0};
			do{
				p = fgets(buff,sizeof(buff),fp);
				if(p){
					ret.append(p);
				}else if(ret.empty()){
					return lyramilk::data::var::nil;
				}
			}while(p && (!ret.empty()) && ret[ret.size() - 1] != '\n');
			return ret;
		}

		lyramilk::data::var write(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_bin);
			if(!fp) throw lyramilk::exception(D("文件未打开"));
			lyramilk::data::chunk cb = args[0];
			return fwrite(cb.c_str(),1,cb.size(),fp);
		}

		lyramilk::data::var writes(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			if(!fp) throw lyramilk::exception(D("文件未打开"));
			lyramilk::data::string cb = args[0];
			return fwrite(cb.c_str(),1,cb.size(),fp);
		}
	  public:
		static int define(lyramilk::script::engine* p)
		{
			{
				lyramilk::script::engine::functional_map fn;
				fn["open"] = lyramilk::script::engine::functional<file,&file::open>;
				fn["isdir"] = lyramilk::script::engine::functional<file,&file::isdir>;
				fn["close"] = lyramilk::script::engine::functional<file,&file::close>;
				fn["ok"] = lyramilk::script::engine::functional<file,&file::good>;
				fn["read"] = lyramilk::script::engine::functional<file,&file::read>;
				fn["reads"] = lyramilk::script::engine::functional<file,&file::reads>;
				fn["readline"] = lyramilk::script::engine::functional<file,&file::readline>;
				fn["write"] = lyramilk::script::engine::functional<file,&file::write>;
				fn["writes"] = lyramilk::script::engine::functional<file,&file::writes>;
				p->define("File",fn,file::ctr,file::dtr);
			}
			return 1;
		}
	};


	class logfile
	{
		filelogers* fpm;
	  public:
		static void* ctr(const lyramilk::data::var::array& args)
		{
			if(args.size() == 1){
				const void* p = args[0].userdata("__loger_filepointer");
				if(p){
					filelogers* pp = (filelogers*)p;
					return new logfile(pp);
				}
				throw lyramilk::exception(D("loger错误： 无法从池中获取到命名对象。"));
			}
			throw lyramilk::exception(D("loger错误： 无法从池中获取到命名对象。"));
		}
		static void dtr(void* p)
		{
			delete (logfile*)p;
		}

		logfile(filelogers* argfpm)
		{
			fpm = argfpm;
		}

		~logfile()
		{
		}

		lyramilk::data::var log(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			lyramilk::data::string str;
			str.reserve(4096);
			for(lyramilk::data::var::array::const_iterator it = args.begin();it!=args.end();++it){
				str += it->str();
			}
			str += "\n";

			return fpm->write(str.c_str(),str.size());
		}

	  public:
		static int define(lyramilk::script::engine* p)
		{
			{
				lyramilk::script::engine::functional_map fn;
				fn["log"] = lyramilk::script::engine::functional<logfile,&logfile::log>;
				p->define("Logfile",fn,logfile::ctr,logfile::dtr);
			}
			return 1;
		}
	};


	static int define(lyramilk::script::engine* p)
	{
		int i = 0;
		{
			p->define("test",test);++i;
			p->define("trace",trace);++i;
			p->define("echo",echo);++i;
			p->define("log",slog);++i;
			i += tester::define(p);
			i += file::define(p);
			i += logfile::define(p);
		}
		return i;
	}

	static __attribute__ ((constructor)) void __init()
	{
		lyramilk::teapoy::script2native::instance()->regist("g.io",define);
	}
}}}	