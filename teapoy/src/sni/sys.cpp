#include <libmilk/scriptengine.h>
#include <libmilk/log.h>
#include <libmilk/dict.h>
#include <libmilk/codes.h>
#include <libmilk/json.h>
#include <libmilk/sha1.h>
#include <libmilk/md5.h>
#include <libmilk/inotify.h>
#include "script.h"
#include "env.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/wait.h>

#include <sys/stat.h>

#include <time.h>  
#include <string.h>  
#include <errno.h>  

namespace lyramilk{ namespace teapoy{ namespace native
{
	static lyramilk::log::logss log(lyramilk::klog,"teapoy.native");

	lyramilk::data::var teapoy_import(const lyramilk::data::array& args,const lyramilk::data::map& senv)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);

		lyramilk::data::map::const_iterator it_env_eng = senv.find(lyramilk::script::engine::s_env_engine());
		if(it_env_eng != senv.end()){
			lyramilk::data::datawrapper* urd = it_env_eng->second.userdata();
			if(urd && urd->name() == lyramilk::script::engine_datawrapper::class_name()){
				lyramilk::script::engine_datawrapper* urdp = (lyramilk::script::engine_datawrapper*)urd;
				if(urdp->eng){
					// 在文件所在目录查找包含文件
					lyramilk::data::string filename = urdp->eng->filename();
					std::size_t pos = filename.rfind('/');
					if(pos != filename.npos){
						filename = filename.substr(0,pos + 1) + args[0].str();
					}

					// 在环境变量指定的目录中查找文件。
					struct stat st = {0};
					if(0 !=::stat(filename.c_str(),&st)){
						filename = env::get(urdp->eng->name() + ".require").str();
						if(!filename.empty() && filename.at(filename.size() - 1) != '/') filename.push_back('/');
						filename += args[0].str();
					}
					// 写入包含信息，防止重复载入
					lyramilk::data::var& v = urdp->eng->get_userdata("require");

					v.type(lyramilk::data::var::t_array);
					lyramilk::data::array& ar = v;

					lyramilk::data::array::iterator it = ar.begin();
					for(;it!=ar.end();++it){
						lyramilk::data::string str = *it;
						if(str == args[0].str()){
							//log(lyramilk::log::warning,__FUNCTION__) << D("请不要重复包含文件%s",str.c_str()) << std::endl;
							return false;
						}
					}
					ar.push_back(args[0].str());

					// 执行包含文件。
					if(urdp->eng->load_module(filename)){
						return true;
					}
				}
			}
		}

		return false;
	}

	struct engineitem
	{
		lyramilk::data::string filename;
		lyramilk::data::string type;
		lyramilk::data::array args;
		unsigned long long msec;
	};

	static void* thread_task(void* _p)
	{
		engineitem* p = (engineitem*)_p;
		engineitem ei = *p;
		delete p;

		struct stat st0;
		while(0 !=::stat(ei.filename.c_str(),&st0)){
			sleep(10);
		}


		lyramilk::script::engines* pool = engine_pool::instance()->get("js");

		lyramilk::script::engines::ptr eng = pool->get();
		if(!eng){
			log(lyramilk::log::error,"task") << D("获取启动脚本失败") << std::endl;
			return nullptr;
		}

		while(!eng->load_file(ei.filename)){
			sleep(10);
		}

		while(true){
			struct stat st1;
			while(0 !=::stat(ei.filename.c_str(),&st1)){
				sleep(10);
			}

			if(st1.st_mtime != st0.st_mtime){
				st0.st_mtime = st1.st_mtime;
				log(lyramilk::log::warning,"task") << D("重新加载%s",ei.filename.c_str()) << std::endl;
				eng->reset();
				eng->load_file(ei.filename);
			}
			lyramilk::data::var v;
			if(eng->call("ontimer",ei.args,&v)){
				if(v.type() == lyramilk::data::var::t_bool && (bool)v == false)break;
			}
			eng->gc();
			usleep(ei.msec * 1000);
		};

		pthread_exit(0);
		return nullptr;
	}

	static void* thread_once_task(void* _p)
	{
		engineitem* p = (engineitem*)_p;
		engineitem ei = *p;
		delete p;

		struct stat st0;
		while(0 !=::stat(ei.filename.c_str(),&st0)){
			sleep(10);
		}


		lyramilk::script::engine* eng = engine_pool::instance()->create_script_instance("js");

		if(!eng){
			log(lyramilk::log::error,"once_task") << D("获取启动脚本失败") << std::endl;
			return nullptr;
		}

		while(!eng->load_file(ei.filename)){
			sleep(10);
		}

		do{
			struct stat st1;
			while(0 !=::stat(ei.filename.c_str(),&st1)){
				sleep(10);
			}

			if(st1.st_mtime != st0.st_mtime){
				st0.st_mtime = st1.st_mtime;
				log(lyramilk::log::warning,"once_task") << D("重新加载%s",ei.filename.c_str()) << std::endl;
				eng->reset();
				eng->load_file(ei.filename);
			}
			lyramilk::data::var v;
			if(eng->call("onthread",ei.args,&v)){
				if(v.type() == lyramilk::data::var::t_bool && (bool)v == false)break;
			}
			eng->gc();
		}while(false);
		engine_pool::instance()->destory_script_instance("js",eng);
		pthread_exit(0);
		return nullptr;
	}


	lyramilk::data::var task(const lyramilk::data::array& args,const lyramilk::data::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_int);
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,2,lyramilk::data::var::t_str);
		pthread_t id_1;
		engineitem* p = new engineitem;
		p->type = args[0].str();
		p->msec = args[1];
		p->filename = args[2].str();

		if(args.size() > 3){
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,3,lyramilk::data::var::t_array);
			p->args = args[3];
		}

		int ret = pthread_create(&id_1,NULL,thread_task,p);
		pthread_detach(id_1);
		return 0 == ret;
	}

	lyramilk::data::var task_once(const lyramilk::data::array& args,const lyramilk::data::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_str);
		pthread_t id_1;
		engineitem* p = new engineitem;
		p->type = args[0].str();
		p->filename = args[1].str();

		if(args.size() > 2){
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,2,lyramilk::data::var::t_array);
			p->args = args[2];
		}

		int ret = pthread_create(&id_1,NULL,thread_once_task,p);
		pthread_detach(id_1);
		return 0 == ret;
	}

	lyramilk::data::var daemon(const lyramilk::data::array& args,const lyramilk::data::map& env)
	{
		::daemon(1,0);
		return true;
	}

	lyramilk::data::var crand(const lyramilk::data::array& args,const lyramilk::data::map& env)
	{
		if(args.size() > 0){
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_int);
			unsigned int seed = args[0];
			srand(seed);
		}
		return rand();
	}

	lyramilk::data::var msleep(const lyramilk::data::array& args,const lyramilk::data::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_int);
		unsigned long long usecond = args[0];
		return usleep(usecond * 1000);
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


		log(lyramilk::log::warning,__FUNCTION__) << D("切换到用户[%s]失败%s",username.c_str(),strerror(errno)) << std::endl;
		return false;
	}

	lyramilk::data::var add_require_dir(const lyramilk::data::array& args,const lyramilk::data::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
		TODO();
		return true;
	}

	lyramilk::data::var serialize(const lyramilk::data::array& args,const lyramilk::data::map& env)
	{
		if(args.size() < 1) throw lyramilk::exception(D("%s参数不足",__FUNCTION__));
		lyramilk::data::var v = args[0];
		lyramilk::data::stringstream ss;
		v.serialize(ss);

		lyramilk::data::string str = ss.str();
		lyramilk::data::chunk bstr((const unsigned char*)str.c_str(),str.size());
		return bstr;
	}

	lyramilk::data::var deserialize(const lyramilk::data::array& args,const lyramilk::data::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_bin);
		lyramilk::data::string seq = args[0];
		lyramilk::data::var v;
		lyramilk::data::stringstream ss(seq);
		v.deserialize(ss);
		return v;
	}

	lyramilk::data::var json_stringify(const lyramilk::data::array& args,const lyramilk::data::map& env)
	{
		if(args.size() < 1)  throw lyramilk::exception(D("%s参数不足",__FUNCTION__));
		lyramilk::data::string jsonstr;
		lyramilk::data::var v = args[0];
		lyramilk::data::json::stringify(v,&jsonstr);
		return jsonstr;
	}

	lyramilk::data::var json_parse(const lyramilk::data::array& args,const lyramilk::data::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
		lyramilk::data::var v;
		lyramilk::data::json::parse(args[0],&v);
		return v;
	}

	lyramilk::data::var system(const lyramilk::data::array& args,const lyramilk::data::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
		lyramilk::data::string str = args[0];

		lyramilk::data::string ret;
		FILE *pp = popen(str.c_str(), "r");
		char tmp[1024];
		while (fgets(tmp, sizeof(tmp), pp) != NULL) {
			ret.append(tmp);
		}
		pclose(pp);
		return ret;
	}

	lyramilk::data::var decode(const lyramilk::data::array& args,const lyramilk::data::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_str);
		lyramilk::data::string type = args[0];
		lyramilk::data::string str = args[1];
		return lyramilk::data::codes::instance()->decode(type,str);
	}

	lyramilk::data::var encode(const lyramilk::data::array& args,const lyramilk::data::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_str);
		lyramilk::data::string type = args[0];
		lyramilk::data::string str = args[1];
		return lyramilk::data::codes::instance()->encode(type,str);
	}

	lyramilk::data::var sha1(const lyramilk::data::array& args,const lyramilk::data::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
		lyramilk::data::string str = args[0];
		lyramilk::cryptology::sha1 c1;
		c1 << str;
		return c1.get_key().str();
	}

	lyramilk::data::var md5_16(const lyramilk::data::array& args,const lyramilk::data::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
		lyramilk::data::string str = args[0];
		lyramilk::cryptology::md5 c1;
		c1 << str;
		return c1.get_key().str16();
	}

	lyramilk::data::var md5_32(const lyramilk::data::array& args,const lyramilk::data::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
		lyramilk::data::string str = args[0];
		lyramilk::cryptology::md5 c1;
		c1 << str;
		return c1.get_key().str32();
	}

	lyramilk::data::string inline md5(lyramilk::data::string str)
	{
		lyramilk::cryptology::md5 c1;
		c1 << str;
		return c1.get_key().str32();
	}

	lyramilk::data::var http_digest_authentication(const lyramilk::data::array& args,const lyramilk::data::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_map);

		lyramilk::data::string emptystr;
		lyramilk::data::string algorithm = args[0];
		lyramilk::data::map m = args[1];

		lyramilk::data::string realm = m["realm"].conv(emptystr);
		if(realm.empty()) return lyramilk::data::var::nil;

		lyramilk::data::string nonce = m["nonce"].conv(emptystr);
		if(nonce.empty()) return lyramilk::data::var::nil;

		lyramilk::data::string cnonce = m["cnonce"].conv(emptystr);
		if(cnonce.empty()) return lyramilk::data::var::nil;

		lyramilk::data::string nc = m["nc"].conv(emptystr);
		if(nc.empty()) return lyramilk::data::var::nil;

		lyramilk::data::string qop = m["qop"].conv(emptystr);
		if(qop.empty()) return lyramilk::data::var::nil;

		lyramilk::data::string uri = m["uri"].conv(emptystr);
		if(uri.empty()) return lyramilk::data::var::nil;

		lyramilk::data::string method = m["method"].conv(emptystr);
		if(method.empty()) return lyramilk::data::var::nil;


		lyramilk::data::string HA1 = m["HA1"].conv(emptystr);
		lyramilk::data::string HA2 = m["HA2"].conv(emptystr);
		lyramilk::data::string HD = m["HD"].conv(emptystr);

		if(HA1.empty()){
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,2,lyramilk::data::var::t_str);
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,3,lyramilk::data::var::t_str);
			lyramilk::data::string username = args[2];
			lyramilk::data::string password = args[3];

			if(algorithm == "MD5"){
				HA1 = md5(username + ":" + realm + ":" + password);
			}else if(algorithm == "MD5-sess"){
				HA1 = md5(md5(username + ":" + realm + ":" + password) + ":" + nonce + ":" + cnonce);
			}
		}

		if(qop == "auth-int"){
			lyramilk::data::string body = m["body"].conv(emptystr);
			if(body.empty()) return lyramilk::data::var::nil;
			if(HD.empty()) HD = nonce + ":" + nc + ":" + cnonce + ":" + qop;
			if(uri.empty()){
			
			}else{
				if(HA2.empty()) HA2 = md5(method + ":" + uri + ":" + md5(body));
			}
		}else if(qop == ""){
			if(HD.empty()) HD = nonce;
			if(uri.empty()){
				if(HA2.empty()) HA2 = md5(method);
			}else{
				if(HA2.empty()) HA2 = md5(method + ":" + uri);
			}
		}else if(qop == "auth"){
			if(HD.empty()) HD = nonce + ":" + nc + ":" + cnonce + ":" + qop;
			if(uri.empty()){
				if(HA2.empty()) HA2 = md5(method);
			}else{
				if(HA2.empty()) HA2 = md5(method + ":" + uri);
			}

		}else{
			return lyramilk::data::var::nil;
		}

		return md5(HA1 + ":" + HD + ":" + HA2);
	}

	lyramilk::data::var bin2str(const lyramilk::data::array& args,const lyramilk::data::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_bin);
		return args[0].str();
	}

	lyramilk::data::var srand(const lyramilk::data::array& args,const lyramilk::data::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_int);
		int seed = args[0];
		::srand(seed);
		return true;
	}

	lyramilk::data::var rand(const lyramilk::data::array& args,const lyramilk::data::map& env)
	{
		return ::rand();
	}

	static int define(lyramilk::script::engine* p)
	{
		int i = 0;
		{
			p->define("require",teapoy_import);++i;
			p->define("task",task);++i;
			p->define("create_thread",task_once);++i;
			p->define("daemon",daemon);++i;
			p->define("crand",crand);++i;
			p->define("msleep",msleep);++i;
			p->define("su",su);++i;
			p->define("add_require_dir",add_require_dir);++i;
			p->define("serialize",serialize);++i;
			p->define("deserialize",deserialize);++i;
			p->define("json_stringify",json_stringify);++i;
			p->define("json_parse",json_parse);++i;
			p->define("system",system);++i;
			p->define("decode",decode);++i;
			p->define("encode",encode);++i;
			p->define("sha1",sha1);++i;
			p->define("md5_16",md5_16);++i;
			p->define("md5_32",md5_32);++i;
			p->define("md5",md5_32);++i;
			p->define("bin2str",bin2str);++i;
			p->define("srand",srand);++i;
			p->define("rand",rand);++i;
			p->define("http_digest_authentication",http_digest_authentication);++i;
		}
		return i;
	}

	static __attribute__ ((constructor)) void __init()
	{
		lyramilk::teapoy::script_interface_master::instance()->regist("sys",define);
	}
}}}