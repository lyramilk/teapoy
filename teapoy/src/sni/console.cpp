#include <libmilk/scriptengine.h>
#include <libmilk/log.h>
#include <libmilk/dict.h>
#include "script.h"
#include <fstream>
#include <sys/stat.h>

namespace lyramilk{ namespace teapoy
{
	static lyramilk::log::logss log(lyramilk::klog,"teapoy.console");

	lyramilk::data::var test(const lyramilk::data::array& args,const lyramilk::data::map& env)
	{
		if(args.size() > 0) return args[0];
		return "test";
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

	lyramilk::data::var echo(const lyramilk::data::array& args,const lyramilk::data::map& env)
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



	class file:public lyramilk::script::sclass
	{
		lyramilk::log::logss log;
		FILE* fp;
		lyramilk::data::string filename;
	  public:
		static lyramilk::script::sclass* ctr(const lyramilk::data::array& args)
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
		static void dtr(lyramilk::script::sclass* p)
		{
			delete (file*)p;
		}

		file():log(lyramilk::klog,"teapoy.native.file")
		{
			fp = nullptr;
		}

		virtual ~file()
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

		lyramilk::data::var open(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			if(args.size() == 1){
				return open(args[0].str().c_str());
			}
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_str);
			return open(args[0].str().c_str(),args[1].str().c_str());
		}

		lyramilk::data::var isdir(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			struct stat st = {0};
			if(stat(filename.c_str(),&st) == -1){
				return false;
			}
			return (st.st_mode & S_IFDIR) != 0;
		}

		lyramilk::data::var close(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			::fclose(fp);
			return true;
		}

		lyramilk::data::var good(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			return fp != nullptr;
		}

		lyramilk::data::var read(const lyramilk::data::array& args,const lyramilk::data::map& env)
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

		lyramilk::data::var reads(const lyramilk::data::array& args,const lyramilk::data::map& env)
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

		lyramilk::data::var readline(const lyramilk::data::array& args,const lyramilk::data::map& env)
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

		lyramilk::data::var write(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_bin);
			if(!fp) throw lyramilk::exception(D("文件未打开"));
			lyramilk::data::chunk cb = args[0];
			return fwrite(cb.c_str(),1,cb.size(),fp);
		}

		lyramilk::data::var writes(const lyramilk::data::array& args,const lyramilk::data::map& env)
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


	static int define(lyramilk::script::engine* p)
	{
		int i = 0;
		{
			p->define("print",echo);++i;
			i += console::define(p);
			i += file::define(p);
		}
		return i;
	}

	static __attribute__ ((constructor)) void __init()
	{
		lyramilk::teapoy::script_interface_master::instance()->regist("console",define);
	}
}}