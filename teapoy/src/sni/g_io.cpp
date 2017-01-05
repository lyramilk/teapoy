#include <libmilk/scriptengine.h>
#include <libmilk/log.h>
#include <libmilk/multilanguage.h>
#include "script.h"
#include <fstream>
#include <sys/stat.h>

namespace lyramilk{ namespace teapoy{ namespace native
{
	static lyramilk::log::logss log(lyramilk::klog,"teapoy.native");

	lyramilk::data::var test(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
	{
		return true;
	}

	lyramilk::data::var echo(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
	{
		for(lyramilk::data::var::array::const_iterator it = args.begin();it!=args.end();++it){
			std::cout << it->str();
		}
		std::cout << std::endl;
		return true;
	}

	lyramilk::data::var trace(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
	{
		lyramilk::script::engine* e = (lyramilk::script::engine*)env.find(lyramilk::script::engine::s_env_engine())->second.userdata(lyramilk::script::engine::s_user_engineptr());
		lyramilk::data::string mod = "s=" + e->filename();

		lyramilk::klog(lyramilk::log::trace,mod);
		for(lyramilk::data::var::array::const_iterator it = args.begin();it!=args.end();++it){
			lyramilk::klog << it->str();
		}
		lyramilk::klog << std::endl;
		return true;
	}

	lyramilk::data::var slog(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
		lyramilk::script::engine* e = (lyramilk::script::engine*)env.find(lyramilk::script::engine::s_env_engine())->second.userdata(lyramilk::script::engine::s_user_engineptr());
		lyramilk::data::string mod = "s=" + e->filename();
		lyramilk::data::string logtype = args[0];

		if(logtype == "debug"){
			lyramilk::klog(lyramilk::log::debug,mod);
		}else if(logtype == "trace"){
			lyramilk::klog(lyramilk::log::trace,mod);
		}else if(logtype == "warning"){
			lyramilk::klog(lyramilk::log::warning,mod);
		}else if(logtype == "error"){
			lyramilk::klog(lyramilk::log::error,mod);
		}else{
			throw lyramilk::exception(D("错误的日志类型"));
		}


		lyramilk::data::var::array::const_iterator it = args.begin();
		if(it!=args.end()){
			++it;
			for(;it!=args.end();++it){
				lyramilk::klog << it->str();
			}
		}
		lyramilk::klog << std::endl;
		return true;
	}

	class binarychunk
	{
		static void* ctr(const lyramilk::data::var::array& args)
		{
			return new lyramilk::data::chunk();
		}
		static void dtr(void* p)
		{
			delete (lyramilk::data::chunk*)p;
		}
	  public:
		static int define(lyramilk::script::engine* p)
		{
			{
				lyramilk::script::engine::functional_map fn;
				p->define("BinaryData",fn,binarychunk::ctr,binarychunk::dtr);
			}
			return 1;
		}
	};

	class file
	{
		lyramilk::log::logss log;
		std::ifstream ifs;
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
			}

			return nullptr;
		}
		static void dtr(void* p)
		{
			delete (file*)p;
		}

		file():log(lyramilk::klog,"teapoy.native.file")
		{}

		~file()
		{
		}

		bool open(const char* filename)
		{
			this->filename = filename;
			ifs.open(filename,std::ifstream::in|std::ifstream::binary);
			return ifs.good();
		}



		lyramilk::data::var open(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			return open(args[0].str().c_str());
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
			ifs.close();
			return true;
		}

		lyramilk::data::var good(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			return ifs.good();
		}

		lyramilk::data::var read(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_uint32);
			lyramilk::data::chunk cb((lyramilk::data::uint32)args[0],0);
			ifs.read((char*)cb.c_str(),cb.size());
			if(ifs.gcount() < (unsigned int)cb.size()){
				cb.erase(cb.begin() + ifs.gcount(),cb.end());
			}
			return cb;
		}

		lyramilk::data::var readstr(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			if(args.size() > 0){
				MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_uint32);
				lyramilk::data::string cb((lyramilk::data::uint32)args[0],0);
				ifs.read((char*)cb.c_str(),cb.size());
				if(ifs.gcount() < (unsigned int)cb.size()){
					cb.erase(cb.begin() + ifs.gcount(),cb.end());
				}
				return cb;
			}

			ifs.seekg(0,std::ifstream::end);
			std::size_t size = ifs.tellg();
			ifs.seekg(0,std::ifstream::beg);

			lyramilk::data::string cb(size,0);
			ifs.read((char*)cb.c_str(),cb.size());
			return cb;
		}

		lyramilk::data::var write(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_uint32);
			lyramilk::data::chunk cb((lyramilk::data::uint32)args[0],0);
			ifs.read((char*)cb.c_str(),cb.size());
			if(ifs.gcount() < (unsigned int)cb.size()){
				cb.erase(cb.begin() + ifs.gcount(),cb.end());
			}
			return cb;
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
				fn["write"] = lyramilk::script::engine::functional<file,&file::write>;
				fn["readstr"] = lyramilk::script::engine::functional<file,&file::readstr>;
				p->define("File",fn,file::ctr,file::dtr);
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
			i += file::define(p);
			i += binarychunk::define(p);
		}
		return i;
	}

	static __attribute__ ((constructor)) void __init()
	{
		lyramilk::teapoy::script2native::instance()->regist("g.io",define);
	}
}}}	