#include <libmilk/scriptengine.h>
#include <libmilk/log.h>
#include <libmilk/multilanguage.h>
#include <libmilk/codes.h>
#include <libmilk/json.h>
#include <libmilk/sha1.h>
#include <libmilk/md5.h>
#include <libmilk/inotify.h>
#include "script.h"
#include "env.h"

#include <stdio.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/wait.h>

#include <sys/stat.h>

#include <time.h>  

namespace lyramilk{ namespace teapoy{ namespace native
{
	static lyramilk::log::logss log(lyramilk::klog,"teapoy.native");

	lyramilk::data::var teapoy_import(const lyramilk::data::var::array& args,const lyramilk::data::var::map& senv)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
		lyramilk::script::engine* e = (lyramilk::script::engine*)senv.find(lyramilk::script::engine::s_env_engine())->second.userdata(lyramilk::script::engine::s_user_engineptr());

		// 在文件所在目录查找包含文件
		lyramilk::data::string filename = e->filename();
		std::size_t pos = filename.rfind('/');
		if(pos != filename.npos){
			filename = filename.substr(0,pos + 1) + args[0].str();
		}

		// 在环境变量指定的目录中查找文件。
		struct stat st = {0};
		if(0 !=::stat(filename.c_str(),&st)){
			filename = env::get_config(e->name() + ".require").str();
			if(!filename.empty() && filename.at(filename.size() - 1) != '/') filename.push_back('/');
			filename += args[0].str();
		}

		// 写入包含信息，防止重复载入
		lyramilk::data::var& vm = e->get("clearonreset");
		vm.type(lyramilk::data::var::t_map);
		lyramilk::data::var& v = vm["require"];

		v.type(lyramilk::data::var::t_array);
		lyramilk::data::var::array& ar = v;

		lyramilk::data::var::array::iterator it = ar.begin();
		for(;it!=ar.end();++it){
			lyramilk::data::string str = *it;
			if(str == args[0].str()){
				//log(lyramilk::log::warning,__FUNCTION__) << D("请不要重复包含文件%s",str.c_str()) << std::endl;
				return false;
			}
		}
		ar.push_back(args[0].str());

		// 执行包含文件。
		if(e->load_file(filename)){
			return true;
		}
		return false;
	}

	struct engineitem
	{
		lyramilk::data::string filename;
		lyramilk::data::string type;
	};

	void* thread_task(void* _p)
	{
		engineitem* p = (engineitem*)_p;
		engineitem ei = *p;
		delete p;

		lyramilk::data::inotify_file iff(ei.filename);

		lyramilk::script::engine* eng = lyramilk::script::engine::createinstance(ei.type);
		if(!eng){
			log(lyramilk::log::error,"task") << D("获取启动脚本失败") << std::endl;
			return nullptr;
		}
		lyramilk::teapoy::script2native::instance()->fill(eng);
		while(!eng->load_file(ei.filename)){
			sleep(10);
		}
		while(true){
			lyramilk::data::inotify_file::status st = iff.check();
			if(st != lyramilk::data::inotify_file::s_keep){
				log(lyramilk::log::warning,"task") << D("重新加载%s",ei.filename.c_str()) << std::endl;
				eng->set("clearonreset",lyramilk::data::var::map());
				eng->reset();
				eng->load_file(ei.filename);
			}
			lyramilk::data::var v = eng->call("ontimer");
			if(v.type() == lyramilk::data::var::t_bool && v == false)break;
			eng->gc();
			sleep(1);
		};
		lyramilk::script::engine::destoryinstance(ei.type,eng);

		pthread_exit(0);
		return nullptr;
	}  
	lyramilk::data::var task(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_str);
		pthread_t id_1;
		engineitem* p = new engineitem;
		p->type = args[0].str();
		p->filename = args[1].str();
		int ret = pthread_create(&id_1,NULL,thread_task,p);
		pthread_detach(id_1);
		return 0 == ret;
	}

	lyramilk::data::var daemon(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
	{
		::daemon(1,0);
		return true;
	}

	lyramilk::data::var msleep(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_int);
		unsigned long long usecond = args[0];
		return usleep(usecond * 1000);
	}

	lyramilk::data::var su(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
		bool permanent = false;
		if(args.size() > 1){
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_bool);
			permanent = args[1];
		}

		lyramilk::data::string username = args[0];
		// useradd -s /sbin/nologin username
		struct passwd *pw = getpwnam(username.c_str());
		if(pw){
			if(permanent){
				if(seteuid(pw->pw_uid) == 0){
					log(__FUNCTION__) << D("切换到用户[%s]",username.c_str()) << std::endl;
					return true;
				}
			}else{
				if(setuid(pw->pw_uid) == 0){
					log(__FUNCTION__) << D("切换到用户[%s]",username.c_str()) << std::endl;
					return true;
				}
			}
		}
		log(lyramilk::log::warning,__FUNCTION__) << D("切换到用户[%s]失败",username.c_str()) << std::endl;
		return false;
	}

	lyramilk::data::var add_require_dir(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
		TODO();
		return true;
	}

	lyramilk::data::var serialize(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
	{
		if(args.size() < 1) throw lyramilk::exception(D("%s参数不足",__FUNCTION__));
		lyramilk::data::var v = args[0];
		lyramilk::data::stringstream ss;
		v.serialize(ss);

		lyramilk::data::string str = ss.str();
		lyramilk::data::chunk bstr((const unsigned char*)str.c_str(),str.size());
		return bstr;
	}

	lyramilk::data::var deserialize(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_bin);
		lyramilk::data::string seq = args[0];
		lyramilk::data::var v;
		lyramilk::data::stringstream ss(seq);
		v.deserialize(ss);
		return v;
	}

	lyramilk::data::var json_stringify(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
	{
		if(args.size() < 1)  throw lyramilk::exception(D("%s参数不足",__FUNCTION__));
		lyramilk::data::string jsonstr;
		lyramilk::data::var v = args[0];
		lyramilk::data::json::stringify(v,jsonstr);
		return jsonstr;
	}

	lyramilk::data::var json_parse(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
		lyramilk::data::var v;
		lyramilk::data::json::parse(args[0],v);
		return v;
	}

	lyramilk::data::var system(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
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

	lyramilk::data::var env(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);

		if(args.size() > 1){
			env::set_config(args[0],args[1]);
			return true;
		}
		return env::get_config(args[0]);
	}

	lyramilk::data::var set_config(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_map);
		env::set_config("config",args[0]);
		return true;
	}

	lyramilk::data::var get_config(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
		const lyramilk::data::var& cfg = env::get_config("config");
		lyramilk::data::string item = args[0].str();
		return cfg.path(item);
	}

	lyramilk::data::var decode(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_str);
		lyramilk::data::string type = args[0];
		lyramilk::data::string str = args[1];
		return lyramilk::data::codes::instance()->decode(type,str);
	}

	lyramilk::data::var encode(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_str);
		lyramilk::data::string type = args[0];
		lyramilk::data::string str = args[1];
		return lyramilk::data::codes::instance()->encode(type,str);
	}

	lyramilk::data::var sha1(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
		lyramilk::data::string str = args[0];
		lyramilk::cryptology::sha1 c1;
		c1 << str;
		return c1.get_key().str();
	}

	lyramilk::data::var md5_16(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
		lyramilk::data::string str = args[0];
		lyramilk::cryptology::md5 c1;
		c1 << str;
		return c1.get_key().str16();
	}

	lyramilk::data::var md5_32(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
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

	lyramilk::data::var http_digest_authentication(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_map);
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,2,lyramilk::data::var::t_str);
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,3,lyramilk::data::var::t_str);

		lyramilk::data::string algorithm = args[0];
		lyramilk::data::var v = args[1];
		lyramilk::data::var vrealm = v["realm"];
		lyramilk::data::var vnonce = v["nonce"];
		lyramilk::data::var vcnonce = v["cnonce"];
		lyramilk::data::var vnc = v["nc"];
		lyramilk::data::var vqop = v["qop"];
		lyramilk::data::var vuri = v["uri"];
		lyramilk::data::var vmethod = v["method"];
		lyramilk::data::string username = args[2];
		lyramilk::data::string password = args[3];

		lyramilk::data::string realm = vrealm.type_like(lyramilk::data::var::t_str)?vrealm.str():"";
		lyramilk::data::string nonce = vnonce.type_like(lyramilk::data::var::t_str)?vnonce.str():"";
		lyramilk::data::string cnonce = vcnonce.type_like(lyramilk::data::var::t_str)?vcnonce.str():"";
		lyramilk::data::string nc = vnc.type_like(lyramilk::data::var::t_str)?vnc.str():"";
		lyramilk::data::string qop = vqop.type_like(lyramilk::data::var::t_str)?vqop.str():"";
		lyramilk::data::string uri = vuri.type_like(lyramilk::data::var::t_str)?vuri.str():"";
		lyramilk::data::string method = vmethod.type_like(lyramilk::data::var::t_str)?vmethod.str():"";

		if(realm.empty() || nonce.empty() || cnonce.empty() || nc.empty() || /*uri.empty() || */method.empty() || username.empty() || password.empty()){
			return lyramilk::data::var::nil;
		}

		lyramilk::data::string HA1;
		lyramilk::data::string HA2;
		lyramilk::data::string HD;

		if(algorithm == "MD5"){
			HA1 = username + ":" + realm + ":" + password;
		}else if(algorithm == "MD5-sess"){
			HA1 = md5(username + ":" + realm + ":" + password) + ":" + nonce + ":" + cnonce;
		}

		if(qop == "auth-int"){
			lyramilk::data::var vbody = v["body"];
			lyramilk::data::string body = vbody.type_like(lyramilk::data::var::t_str)?vbody.str():"";
			if(body.empty()) return lyramilk::data::var::nil;
			HD = nonce + ":" + nc + ":" + cnonce + ":" + qop;
			if(uri.empty()){
			
			}else{
				HA2 = method + ":" + uri + ":" + md5(body);
			}
		}else if(qop == ""){
			HD = nonce;
			if(uri.empty()){
				HA2 = method;
			}else{
				HA2 = method + ":" + uri;
			}
		}else if(qop == "auth"){
			HD = nonce + ":" + nc + ":" + cnonce + ":" + qop;
			if(uri.empty()){
				HA2 = method;
			}else{
				HA2 = method + ":" + uri;
			}

		}else{
			return lyramilk::data::var::nil;
		}

		return md5(md5(HA1) + ":" + HD + ":" + md5(HA2));
	}

	static int define(lyramilk::script::engine* p)
	{
		int i = 0;
		{
			p->define("teapoy_import",teapoy_import);++i;
			p->define("require",teapoy_import);++i;
			p->define("task",task);++i;
			p->define("daemon",daemon);++i;
			p->define("msleep",msleep);++i;
			p->define("su",su);++i;
			p->define("add_require_dir",add_require_dir);++i;
			p->define("serialize",serialize);++i;
			p->define("deserialize",deserialize);++i;
			p->define("json_stringify",json_stringify);++i;
			p->define("json_parse",json_parse);++i;
			p->define("system",system);++i;
			p->define("env",env);++i;
			p->define("set_config",set_config);++i;
			p->define("config",get_config);++i;
			p->define("decode",decode);++i;
			p->define("encode",encode);++i;
			p->define("sha1",sha1);++i;
			p->define("md5_16",md5_16);++i;
			p->define("md5_32",md5_32);++i;
			p->define("http_digest_authentication",http_digest_authentication);++i;
		}
		return i;
	}

	static __attribute__ ((constructor)) void __init()
	{
		lyramilk::teapoy::script2native::instance()->regist("g.core",define);
	}
}}}