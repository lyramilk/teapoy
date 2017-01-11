#include <libmilk/json.h>
#include <libmilk/scriptengine.h>
#include <libmilk/log.h>
#include <libmilk/multilanguage.h>

#include <unistd.h>
#include <signal.h>
#include <iostream>
#include <fstream>
#include <dlfcn.h>

#include <unistd.h>
#include <pwd.h>
#include <sys/wait.h>
#include <string.h>

#include <stdio.h>
#include <stdlib.h>

#include <libintl.h>

#include <algorithm>
#include <ttyent.h>

#include "script.h"

class teapoy_log_base;

bool ondaemon = false;
teapoy_log_base* logger = nullptr;
lyramilk::script::engine* eng_main = nullptr;
lyramilk::data::string logfile;
lyramilk::data::string launcher_script;
lyramilk::data::string dictfile;
lyramilk::data::string dictfile_missing;

std::map<lyramilk::data::string,bool> logswitch;

lyramilk::data::string get_env(const char* env,lyramilk::data::string def)
{
	lyramilk::data::string strenv;
	const char* cstr = getenv(env);
	if(cstr){
		strenv = cstr;
	}else{
		strenv = def;
	}
	return strenv;
}

class lang_dict:public lyramilk::data::multilanguage::dict
{
	lyramilk::data::multilanguage::dict* old;
	std::fstream fdict;
  public:
	lang_dict()
	{
		old = lyramilk::kdict.tie(this);
	}

	virtual ~lang_dict()
	{
		lyramilk::kdict.tie(old);
	}


	bool open(lyramilk::data::string filepath){
		fdict.open(filepath.c_str(),std::fstream::in|std::fstream::out|std::fstream::binary);
		if(!fdict){
			lyramilk::klog(lyramilk::log::warning,"teapoy.dict.open") << D("打开词典文件失败") << std::endl;
			return false;
		}
		return true;
	}

	virtual void notify(lyramilk::data::string str)
	{
		/*
		lyramilk::data::var::map m;

		lyramilk::data::var v;
		lyramilk::data::json j;
		if(j.open(udictfile)){
			try{
				lyramilk::data::var v = j.path("/dict");
				m = v.as<lyramilk::data::var::map>();
			}catch(lyramilk::data::var::type_invalid& e){
				j.clear();
			}
		}

		lyramilk::data::var &v = j.path("/dict");
		v.type(lyramilk::data::var::t_map);
		v.at(str) = str;
		j.save(udictfile);
		*/
	}
};

class teapoy_log_base:public lyramilk::log::logb
{
	lyramilk::log::logb* old;
  public:
	const bool *enable_log_debug;
	const bool *enable_log_trace;
	const bool *enable_log_warning;
	const bool *enable_log_error;

	teapoy_log_base()
	{
		old = lyramilk::klog.rebase(this);
		enable_log_debug = &logswitch["debug"];
		enable_log_trace = &logswitch["trace"];
		enable_log_warning = &logswitch["warning"];
		enable_log_error = &logswitch["error"];
	}

	virtual ~teapoy_log_base()
	{
		lyramilk::klog.rebase(old);
	}

	virtual bool ok()
	{
		return true;
	}
};

class teapoy_log_stdio:public teapoy_log_base
{
  public:
	virtual void log(time_t ti,lyramilk::log::type ty,lyramilk::data::string usr,lyramilk::data::string app,lyramilk::data::string module,lyramilk::data::string str) const
	{
		lyramilk::data::string cache;
		cache.reserve(1024);
		switch(ty){
		  case lyramilk::log::debug:
			if(!*enable_log_debug)return;
			cache.append("\x1b[36m");
			cache.append(strtime(ti));
			cache.append(" [");
			cache.append(module);
			cache.append("] ");
			cache.append(str);
			cache.append("\x1b[0m");
			fwrite(cache.c_str(),cache.size(),1,stdout);
			break;
		  case lyramilk::log::trace:
			if(!*enable_log_trace)return;
			cache.append("\x1b[37m");
			cache.append(strtime(ti));
			cache.append(" [");
			cache.append(module);
			cache.append("] ");
			cache.append(str);
			cache.append("\x1b[0m");
			fwrite(cache.c_str(),cache.size(),1,stdout);
			break;
		  case lyramilk::log::warning:
			if(!*enable_log_warning)return;
			cache.append("\x1b[33m");
			cache.append(strtime(ti));
			cache.append(" [");
			cache.append(module);
			cache.append("] ");
			cache.append(str);
			cache.append("\x1b[0m");
			fwrite(cache.c_str(),cache.size(),1,stdout);
			break;
		  case lyramilk::log::error:
			if(!*enable_log_error)return;
			cache.append("\x1b[31m");
			cache.append(strtime(ti));
			cache.append(" [");
			cache.append(module);
			cache.append("] ");
			cache.append(str);
			cache.append("\x1b[0m");
			fwrite(cache.c_str(),cache.size(),1,stdout);
			break;
		}
	}
};

class teapoy_log_logfile:public teapoy_log_base
{
	FILE* fp;
	lyramilk::data::string pidstr;
  public:
	teapoy_log_logfile(lyramilk::data::string logfilepath)
	{
		fp = fopen(logfilepath.c_str(),"ab");
		assert(fp);
		pid_t pid = getpid();
		lyramilk::data::stringstream ss;
		ss << pid << " ";
		pidstr = ss.str();

	}
	virtual ~teapoy_log_logfile()
	{
		fclose(fp);
	}

	virtual bool ok()
	{
		return fp != nullptr;
	}

	virtual void log(time_t ti,lyramilk::log::type ty,lyramilk::data::string usr,lyramilk::data::string app,lyramilk::data::string module,lyramilk::data::string str) const
	{
		lyramilk::data::string cache;
		cache.reserve(1024);
		switch(ty){
		  case lyramilk::log::debug:
			if(!*enable_log_debug)return;
			cache.append(pidstr);
			cache.append("[debug] ");
			cache.append(strtime(ti));
			cache.append(" [");
			cache.append(module);
			cache.append("] ");
			cache.append(str);
			fwrite(cache.c_str(),cache.size(),1,fp);
			fflush(fp);
			break;
		  case lyramilk::log::trace:
			if(!*enable_log_trace)return;
			cache.append(pidstr);
			cache.append("[trace] ");
			cache.append(strtime(ti));
			cache.append(" [");
			cache.append(module);
			cache.append("] ");
			cache.append(str);
			fwrite(cache.c_str(),cache.size(),1,fp);
			fflush(fp);
			break;
		  case lyramilk::log::warning:
			if(!*enable_log_warning)return;
			cache.append(pidstr);
			cache.append("[warning] ");
			cache.append(strtime(ti));
			cache.append(" [");
			cache.append(module);
			cache.append("] ");
			cache.append(str);
			fwrite(cache.c_str(),cache.size(),1,fp);
			fflush(fp);
			break;
		  case lyramilk::log::error:
			if(!*enable_log_error)return;
			cache.append(pidstr);
			cache.append("[error] ");
			cache.append(strtime(ti));
			cache.append(" [");
			cache.append(module);
			cache.append("] ");
			cache.append(str);
			fwrite(cache.c_str(),cache.size(),1,fp);
			fflush(fp);
			break;
		}
	}
};

class teapoy_loader
{
	lyramilk::log::logss log;
	std::vector<lyramilk::script::engine*> libs;
	std::vector<void*> solibs;
  public:
	static void* ctr(const lyramilk::data::var::array& args)
	{
		return new teapoy_loader();
	}
	static void dtr(void* p)
	{
		delete (teapoy_loader*)p;
	}

	teapoy_loader():log(lyramilk::klog,"teapoy.loader")
	{
	}

	void init_log()
	{
		if(logger == nullptr){
			teapoy_log_base* logbase = new teapoy_log_stdio();
			if(logbase && logbase->ok()){
				if(logger)delete logger;
				logger = logbase;
				lyramilk::klog(lyramilk::log::debug,"teapoy.log") << D("切换日志成功") << std::endl;
			}else{
				lyramilk::klog(lyramilk::log::error,"teapoy.log") << D("切换日志失败") << std::endl;
			}
		}
	}

	virtual ~teapoy_loader()
	{
		{
			std::vector<lyramilk::script::engine*>::iterator it = libs.begin();
			for(;it!=libs.end();++it){
				lyramilk::script::engine*& eng = *it;
				lyramilk::script::engine::destoryinstance(eng->name(),eng);
			}
		}
		{
			std::vector<void*>::iterator it = solibs.begin();
			for(;it!=solibs.end();++it){
				dlclose(*it);
			}
		}
	}

	lyramilk::data::var load(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);

		lyramilk::data::string filename = args[0];

		lyramilk::data::var::array parameter(1);
		parameter[0].assign("engine",eng_main);

		lyramilk::script::engine* eng_tmp = lyramilk::script::engine::createinstance(filename);
		if(!eng_tmp){
			log(lyramilk::log::error,__FUNCTION__) << D("加载%s失败",filename.c_str()) << std::endl;
			return false;
		}

		eng_tmp->load_file(filename);
		int ret = eng_tmp->call("onload",parameter);
		libs.push_back(eng_tmp);
		return 0 == ret;
	}

	lyramilk::data::var loadso(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
		lyramilk::script::engine* e = (lyramilk::script::engine*)env.find(lyramilk::script::engine::s_env_engine())->second.userdata(lyramilk::script::engine::s_user_engineptr());

		lyramilk::data::string filename = args[0];
		void *handle = dlopen(filename.c_str(), RTLD_NOW);
		if(handle == nullptr){
			log(lyramilk::log::error,__FUNCTION__) << D("加载%s失败：%s",filename.c_str(),dlerror()) << std::endl;
			return false;
		}
		solibs.push_back(handle);
		bool (*pinit)(void*) = (bool (*)(void*))dlsym(handle,"init");
		if(pinit == nullptr){
			log(lyramilk::log::error,__FUNCTION__) << D("从%s中查找init失败：%s",filename.c_str(),dlerror()) << std::endl;
			return false;
		}
		if(pinit(e)){
			log(lyramilk::log::debug,__FUNCTION__) << D("加载%s成功",filename.c_str()) << std::endl;
			return true;
		}else{
			log(lyramilk::log::warning,__FUNCTION__) << D("加载%s成功，但是初始化失败",filename.c_str()) << std::endl;
		}
		return false;
	}
	lyramilk::data::var enable_log(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
	{
		if(logger == nullptr) init_log();
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_bool);
		lyramilk::data::string logtype = args[0];
		bool isenable = args[1];
		std::map<lyramilk::data::string,bool>::iterator it = logswitch.find(logtype);
		logswitch[logtype] = isenable;
		if(isenable){
			log(__FUNCTION__) << D("允许日志<%s>",logtype.c_str()) << std::endl;
		}else{
			log(__FUNCTION__) << D("禁止日志<%s>",logtype.c_str()) << std::endl;
		}
		return true;
	}

	lyramilk::data::var set_log_file(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
	{
		if(!ondaemon){
			log(lyramilk::log::debug,__FUNCTION__) << D("控制台模式，自动忽略日志文件。") << std::endl;
			return false;
		}
		if(!logfile.empty()){
			return false;
		}
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
		logfile = args[0].str();

		teapoy_log_base* logbase = new teapoy_log_logfile(logfile);
		if(logbase && logbase->ok()){
			if(logger)delete logger;
			logger = logbase;
			log(lyramilk::log::debug,__FUNCTION__) << D("切换日志成功") << std::endl;
		}else{
			log(lyramilk::log::error,__FUNCTION__) << D("切换日志失败:%s",logfile.c_str()) << std::endl;
		}
		return true;
	}
};

void useage(lyramilk::data::string selfname)
{
	std::cout << gettext("useage:") << selfname << " [-s] <file> -[dtul?] ..." << std::endl;
	std::cout << "\t-s <file>\t" << gettext("start by script <file>") << std::endl;
	std::cout << "\t-d       \t" << gettext("start as daemon") << std::endl;
	std::cout << "\t-t <file>\t" << gettext("translate string by <file>") << std::endl;
	std::cout << "\t-u <file>\t" << gettext("which string can't be translate record to <file>") << std::endl;
	std::cout << "\t-l <file>\t" << gettext("record log to <file>") << std::endl;
}

lyramilk::data::string get_engine_type(lyramilk::data::string filename)
{
	std::size_t pos = filename.find_last_of('.');
	if(pos == filename.npos){
		return "";
	}

	lyramilk::data::string scripttypename = filename.substr(pos + 1);
	std::transform(scripttypename.begin(),scripttypename.end(),scripttypename.begin(),tolower);
	if(scripttypename == "so") return "elf";
	return scripttypename;
}

int main(int argc,char* argv[])
{
	bool isdaemon = false;
	{
		lyramilk::data::string selfname = argv[0];
		int oc;
		while((oc = getopt(argc, argv, "s:dt:u:l:?")) != -1){
			switch(oc)
			{
			  case 's':
				launcher_script = optarg;
				break;
			  case 'd':
				isdaemon = true;
				break;
			  case 't':
				dictfile = optarg;
				break;
			  case 'u':
				dictfile_missing = optarg;
				break;
			  case 'l':
				logfile = optarg;
				break;
			  case '?':
			  default:
				useage(selfname);
				return 0;
			}
		}
	}

	for(int argi = optind;argi < argc;++argi){
		launcher_script = argv[argi];
	}
	if(isdaemon){
		::daemon(1,1);
		int pid = 0;
		do{
			pid = fork();
			if(pid == 0){
				ondaemon = true;
				break;
			}
		}while(waitpid(pid,NULL,0));
	}else{
		ondaemon = getppid() == 1;
	}

	if(!dictfile.empty()){
		if(!lyramilk::kdict.load(dictfile)){
			lyramilk::klog(lyramilk::log::error,"teapoy.loader") << "load dictionary " << dictfile << " failed." << std::endl;
		}else{
			lyramilk::klog(lyramilk::log::debug,"teapoy.loader") << "load dictionary " << dictfile << " success." << std::endl;
		}
	}

	lyramilk::log::logss log(lyramilk::klog,"teapoy.loader");

	if(ondaemon){
		logswitch["debug"] = false;
		logswitch["trace"] = true;
		logswitch["warning"] = true;
		logswitch["error"] = true;
		if(!logfile.empty()){
			teapoy_log_base* logbase = new teapoy_log_logfile(logfile);
			if(logbase && logbase->ok()){
				if(logger)delete logger;
				logger = logbase;
				log(lyramilk::log::debug,__FUNCTION__) << D("切换日志成功") << std::endl;
			}else{
				log(lyramilk::log::error,__FUNCTION__) << D("切换日志失败:%s",logfile.c_str()) << std::endl;
			}
		}
	}else{
		log(lyramilk::log::debug,__FUNCTION__) << D("控制台模式，自动忽略日志文件。") << std::endl;
	}

	signal(SIGPIPE, SIG_IGN);

	// 确定启动脚本的类型
	lyramilk::data::string launcher_script_type = get_engine_type(launcher_script);
	if(launcher_script_type.empty()){
		log(lyramilk::log::error) << D("无法确定启动脚本的类型。") << std::endl;
		return -1;
	}

	log(lyramilk::log::trace) << D("启动脚本%s(类型为：%s)",launcher_script.c_str(),launcher_script_type.c_str()) << std::endl;
	eng_main = lyramilk::script::engine::createinstance(launcher_script_type);
	if(!eng_main){
		log(lyramilk::log::error) << D("获取启动脚本失败") << std::endl;
		return -1;
	}

	{
		lyramilk::script::engine::functional_map fn;
		fn["load"] = lyramilk::script::engine::functional<teapoy_loader,&teapoy_loader::load>;
		fn["loadso"] = lyramilk::script::engine::functional<teapoy_loader,&teapoy_loader::loadso>;
		fn["enable_log"] = lyramilk::script::engine::functional<teapoy_loader,&teapoy_loader::enable_log>;
		fn["set_log_file"] = lyramilk::script::engine::functional<teapoy_loader,&teapoy_loader::set_log_file>;
		eng_main->define("teapoy",fn,teapoy_loader::ctr,teapoy_loader::dtr);
	}

	lyramilk::teapoy::script2native::instance()->fill(eng_main);
	eng_main->load_file(launcher_script);
	log(lyramilk::log::trace) << "执行完毕。" << std::endl;
	return 0;
}
