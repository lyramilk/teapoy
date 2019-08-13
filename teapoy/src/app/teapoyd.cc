#include <libmilk/json.h>
#include <libmilk/scriptengine.h>
#include <libmilk/log.h>
#include <libmilk/dict.h>

#include <signal.h>
#include <iostream>
#include <fstream>
#include <dlfcn.h>

#include <sys/wait.h>
#include <libintl.h>
#include <algorithm>

#include <errno.h>
#include <curl/curl.h>
#include <pwd.h>

#include "config.h"
#include "script.h"
#include "webservice.h"
#include "js_extend.h"

class teapoy_log_base;

bool ondaemon = false;
bool no_exit_mode;
teapoy_log_base* logger = nullptr;
lyramilk::script::engine* eng_main = nullptr;
lyramilk::data::string logfile;
lyramilk::data::string launcher_script;
lyramilk::data::string dictfile;
lyramilk::data::string dictfile_missing;

bool enable_log_debug = true;
bool enable_log_trace = true;
bool enable_log_warning = true;
bool enable_log_error = true;


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

class lang_dict:public lyramilk::data::dict
{
	lyramilk::data::dict* old;
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
		lyramilk::data::map m;

		lyramilk::data::var v;
		lyramilk::data::json j;
		if(j.open(udictfile)){
			try{
				lyramilk::data::var v = j.path("/dict");
				m = v.as<lyramilk::data::map>();
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

class teapoy_log_logfile:public lyramilk::log::logf
{
  public:
	teapoy_log_logfile(lyramilk::data::string logfilepathfmt):lyramilk::log::logf(logfilepathfmt)
	{
	}
	virtual ~teapoy_log_logfile()
	{
	}

	virtual void log(time_t ti,lyramilk::log::type ty,const lyramilk::data::string& usr,const lyramilk::data::string& app,const lyramilk::data::string& module,const lyramilk::data::string& str) const
	{
		switch(ty){
		  case lyramilk::log::debug:
			if(!enable_log_debug)return;
			break;
		  case lyramilk::log::trace:
			if(!enable_log_trace)return;
			break;
		  case lyramilk::log::warning:
			if(!enable_log_warning)return;
			break;
		  case lyramilk::log::error:
			if(!enable_log_error)return;
			break;
		}
		lyramilk::log::logf::log(ti,ty,usr,app,module,str);
	}
};

class teapoy_loader:public lyramilk::script::sclass
{
	lyramilk::log::logss log;
	std::vector<lyramilk::script::engine*> libs;
	std::vector<void*> solibs;
	lyramilk::data::string newroot;
  public:
	static lyramilk::script::sclass* ctr(const lyramilk::data::array& args)
	{
		return new teapoy_loader();
	}
	static void dtr(lyramilk::script::sclass* p)
	{
		delete (teapoy_loader*)p;
	}

	teapoy_loader():log(lyramilk::klog,"teapoy.loader")
	{
		newroot = "/";
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

	lyramilk::data::var loadso(const lyramilk::data::array& args,const lyramilk::data::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
		lyramilk::script::engine* e = (lyramilk::script::engine*)env.find(lyramilk::script::engine::s_env_engine())->second.userdata(lyramilk::script::engine::s_env_engine());

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

	lyramilk::data::var enable_log(const lyramilk::data::array& args,const lyramilk::data::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_bool);
		lyramilk::data::string logtype = args[0];
		bool isenable = args[1];

		if(logtype == "debug")   enable_log_debug = isenable;
		else if(logtype == "trace")   enable_log_trace = isenable;
		else if(logtype == "warning") enable_log_warning = isenable;
		else if(logtype == "error")   enable_log_error = isenable;

		if(isenable){
			log(__FUNCTION__) << D("允许日志<%s>",logtype.c_str()) << std::endl;
		}else{
			log(__FUNCTION__) << D("禁止日志<%s>",logtype.c_str()) << std::endl;
		}
		return true;
	}

	lyramilk::data::var set_log_file(const lyramilk::data::array& args,const lyramilk::data::map& env)
	{
		if(!logfile.empty()){
			return false;
		}
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
		logfile = args[0].str();

		lyramilk::log::logf* lf = new teapoy_log_logfile(logfile);

		if(lf && lf->ok()){
			lyramilk::klog.rebase(lf);
			log(lyramilk::log::debug,__FUNCTION__) << D("切换日志成功") << std::endl;
			return true;
		}

		if(lf) delete lf;

		log(lyramilk::log::error,__FUNCTION__) << D("切换日志失败:%s",logfile.c_str()) << std::endl;
		return false;
	}


	
	lyramilk::data::var chroot(const lyramilk::data::array& args,const lyramilk::data::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
		newroot = args[0].str();
		if( 0 == ::chroot(newroot.c_str()) ){
			::chdir("/");
			log(lyramilk::log::debug,__FUNCTION__) << D("切换根目录到%s成功",newroot.c_str()) << std::endl;
			return true;
		}
		log(lyramilk::log::warning,__FUNCTION__) << D("切换根目录到%s失败:原因%s",newroot.c_str(),strerror(errno)) << std::endl;
		return false;
	}
	
	lyramilk::data::var su(const lyramilk::data::array& args,const lyramilk::data::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
		lyramilk::data::string username = args[0];
		// useradd -s /sbin/nologin username
		struct passwd *pw = getpwnam(username.c_str());
		if(pw){
			if(setgid(pw->pw_gid) == 0 && setuid(pw->pw_uid) == 0){
				log(__FUNCTION__) << D("切换到用户[%s]",username.c_str()) << std::endl;
				log(__FUNCTION__) << D("切换到用户组[%s]",username.c_str()) << std::endl;
				return true;
			}
		}
		return false;
	}
	
	lyramilk::data::var root(const lyramilk::data::array& args,const lyramilk::data::map& env)
	{
		return newroot;
	}

	lyramilk::data::var noexit(const lyramilk::data::array& args,const lyramilk::data::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_bool);
		no_exit_mode = args[0];
		return no_exit_mode;
	}

	lyramilk::data::var is_daemon(const lyramilk::data::array& args,const lyramilk::data::map& env)
	{
		return ondaemon;
	}
};

void useage(lyramilk::data::string selfname)
{
	std::cout << gettext("useage:") << selfname << " [optional] <file>" << std::endl;
	std::cout << "version: " << TEAPOY_VERSION << std::endl;
	std::cout << "\t-s <file>\t" << gettext("start by script <file>") << std::endl;
	std::cout << "\t-e <type>\t" << gettext("use engine type(eg. js,lua,...),default depending on extension name.") << std::endl;
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

	lyramilk::data::string launcher_script_type;
	{
		lyramilk::data::string selfname = argv[0];
		int oc;
		while((oc = getopt(argc, argv, "s:dct:u:l:e:?")) != -1){
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
			  case 'e':
				launcher_script_type = optarg;
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
		::daemon(1,0);
		int pid = 0;
		do{
			pid = fork();
			if(pid == 0){
				ondaemon = true;
				break;
			}
			sleep(1);
		}while(waitpid(pid,NULL,0));
	}else{
		ondaemon = getppid() == 1;
	}

	if(!dictfile.empty()){
		if(!lyramilk::kdict.load(dictfile)){
			lyramilk::klog(lyramilk::log::error,"teapoy.loader") << "load dictionary " << dictfile << " failed." << std::endl;
		}else{
			enable_log_debug = true;
			lyramilk::klog(lyramilk::log::debug,"teapoy.loader") << "load dictionary " << dictfile << " success." << std::endl;
		}
	}

	lyramilk::log::logss log(lyramilk::klog,"teapoy.loader");

	if(ondaemon){
		lyramilk::log::logf* lf = new teapoy_log_logfile(logfile);
		if(lf && lf->ok()){
			lyramilk::klog.rebase(lf);
			log(lyramilk::log::debug,__FUNCTION__) << D("切换日志成功") << std::endl;
		}else{
			if(lf) delete lf;
			log(lyramilk::log::error,__FUNCTION__) << D("切换日志失败:%s",logfile.c_str()) << std::endl;
		}
	}else{
		log(lyramilk::log::debug,__FUNCTION__) << D("控制台模式，自动忽略日志文件。") << std::endl;
	}

	signal(SIGPIPE, SIG_IGN);

	lyramilk::teapoy::httpadapter::init();

	lyramilk::teapoy::sni::js_extend::engine_load("js");
	lyramilk::teapoy::sni::jshtml::engine_load("jshtml");

	// 确定启动脚本的类型
	if(launcher_script_type.empty()){
		launcher_script_type = get_engine_type(launcher_script);
	}
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
		fn["loadLibrary"] = lyramilk::script::engine::functional<teapoy_loader,&teapoy_loader::loadso>;
		fn["enableLog"] = lyramilk::script::engine::functional<teapoy_loader,&teapoy_loader::enable_log>;
		fn["setLogFile"] = lyramilk::script::engine::functional<teapoy_loader,&teapoy_loader::set_log_file>;
		fn["chroot"] = lyramilk::script::engine::functional<teapoy_loader,&teapoy_loader::chroot>;
		fn["su"] = lyramilk::script::engine::functional<teapoy_loader,&teapoy_loader::su>;
		fn["root"] = lyramilk::script::engine::functional<teapoy_loader,&teapoy_loader::root>;
		fn["noexit"] = lyramilk::script::engine::functional<teapoy_loader,&teapoy_loader::noexit>;
		fn["isDaemon"] = lyramilk::script::engine::functional<teapoy_loader,&teapoy_loader::is_daemon>;
		eng_main->define("Teapoy",fn,teapoy_loader::ctr,teapoy_loader::dtr);
	}

	lyramilk::teapoy::script_interface_master::instance()->apply(eng_main);
	eng_main->load_file(launcher_script);

	lyramilk::data::array ar;
	for(int i=0;i<argc;++i){
		ar.push_back(argv[i]);
	}
	lyramilk::data::var result = eng_main->call("main",ar);

	if(no_exit_mode){
		log(lyramilk::log::trace) << D("设置了noexit标志，程序将不会随主脚本返回而退出。") << std::endl;
		while(no_exit_mode){
			sleep(3600);
		}
	}
	log(lyramilk::log::trace) << D("执行完毕。") << std::endl;
	return 0;
}
