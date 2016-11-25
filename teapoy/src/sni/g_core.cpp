#include <libmilk/scriptengine.h>
#include <libmilk/log.h>
#include <libmilk/multilanguage.h>
#include <libmilk/codes.h>
#include <libmilk/json.h>
#include <libmilk/sha1.h>
#include <libmilk/md5.h>
#include "script.h"
#include "env.h"

#include <stdio.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/wait.h>

namespace lyramilk{ namespace teapoy{ namespace native
{
	static lyramilk::log::logss log(lyramilk::klog,"teapoy.native");

	lyramilk::data::var teapoy_import(const lyramilk::data::var::array& args,const lyramilk::data::var::map& senv)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);

		lyramilk::data::string filename = env::get_config("js.require");
		if(!filename.empty() && filename.at(filename.size() - 1) != '/') filename.push_back('/');
		filename += args[0].str();
		lyramilk::script::engine* e = (lyramilk::script::engine*)senv.find(lyramilk::script::engine::s_env_engine())->second.userdata(lyramilk::script::engine::s_user_engineptr());

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
		if(e->load_file(false,filename)){
			return true;
		}
		return false;
	}
/*
	lyramilk::data::var daemon(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
	{
		//::daemon(1,0);
		int pid = 0;
		do{
			pid = fork();
			if(pid == 0){
				break;
			}
		}while(waitpid(pid,NULL,0));
		return true;
	}*/

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
		lyramilk::data::var tmpv = args[0];
		lyramilk::data::json j(tmpv);
		return j.str();
	}

	lyramilk::data::var json_parse(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);

		lyramilk::data::var v;
		lyramilk::data::json j(v);
		j.str(args[0]);
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
		lyramilk::data::var cfg = env::get_config("config");
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

		if(realm.empty() || nonce.empty() || cnonce.empty() || nc.empty() || uri.empty() || method.empty() || username.empty() || password.empty()){
			return false;
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
			if(body.empty()) return false;
			HD = nonce + ":" + nc + ":" + cnonce + ":" + qop;
			HA2 = method + ":" + uri + ":" + md5(body);
		}else if(qop == ""){
			HD = nonce;
			HA2 = method + ":" + uri;
		}else if(qop == "auth"){
			HD = nonce + ":" + nc + ":" + cnonce + ":" + qop;
			HA2 = method + ":" + uri;
		}else{
			return false;
		}

		return md5(md5(HA1) + ":" + HD + ":" + md5(HA2));
	}

	static int define(bool permanent,lyramilk::script::engine* p)
	{
		int i = 0;
		{
			p->define(permanent,"teapoy_import",teapoy_import);++i;
			p->define(permanent,"require",teapoy_import);++i;
			p->define(permanent,"su",su);++i;
			p->define(permanent,"add_require_dir",add_require_dir);++i;
			p->define(permanent,"serialize",serialize);++i;
			p->define(permanent,"deserialize",deserialize);++i;
			p->define(permanent,"json_stringify",json_stringify);++i;
			p->define(permanent,"json_parse",json_parse);++i;
			p->define(permanent,"system",system);++i;
			p->define(permanent,"env",env);++i;
			p->define(permanent,"set_config",set_config);++i;
			p->define(permanent,"config",get_config);++i;
			p->define(permanent,"decode",decode);++i;
			p->define(permanent,"encode",encode);++i;
			p->define(permanent,"sha1",sha1);++i;
			p->define(permanent,"md5_16",md5_16);++i;
			p->define(permanent,"md5_32",md5_32);++i;
			p->define(permanent,"http_digest_authentication",http_digest_authentication);++i;
		}
		return i;
	}

	static __attribute__ ((constructor)) void __init()
	{
		lyramilk::teapoy::script2native::instance()->regist("g.core",define);
	}
}}}