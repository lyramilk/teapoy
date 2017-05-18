#include "script_jsx.h"
#include <libmilk/multilanguage.h>
#include <libmilk/log.h>
#include <libmilk/codes.h>
//#include "testing.h"
#include <jsfriendapi.h>
#include <fstream>
#include <cassert>
#include <sys/stat.h>
#include <utime.h>
#include <math.h>
//script_jsx

namespace lyramilk{namespace script{namespace js
{
	script_jsx::script_jsx()
	{
	}

	script_jsx::~script_jsx()
	{
	}

	lyramilk::data::string static jsx2js(lyramilk::data::string jsxcache)
	{
		lyramilk::data::stringstream src(jsxcache);
		lyramilk::data::stringstream ofs;
		ofs << "function onrequest(request,response){\n";
		lyramilk::data::string line;

		bool incode = false;
		int lineno =0;
		while(getline(src,line)){
			++lineno;
			std::size_t pos_begin = 0;
			if(incode){
				std::size_t pos_flag1 = line.find("%>");
				if(pos_flag1 == line.npos){
					ofs << line << "//#" << lineno << '\n';
					continue;
				}
				ofs << line.substr(0,pos_flag1);
				pos_begin = pos_flag1 + 2;
			}
			while(true){
				std::size_t pos_flag1 = line.find("<%",pos_begin);
				if(pos_flag1 == line.npos){
					ofs << "response.write(unescape(\"" << lyramilk::data::codes::instance()->encode("js",line.substr(pos_begin)) << "\"));";
					incode = false;
					break;
				}
				ofs << "response.write(unescape(\"" << lyramilk::data::codes::instance()->encode("js",line.substr(pos_begin,pos_flag1 - pos_begin)) << "\"));";

				std::size_t pos_flag2 = line.find("%>",pos_flag1 + 2);
				if(pos_flag2 == line.npos){
					ofs << line.substr(pos_flag1 + 2);
					incode = true;
					break;
				}

				lyramilk::data::string tmp = line.substr(pos_flag1 + 2,pos_flag2 - pos_flag1 - 2);
				if(!tmp.empty()){
					if(tmp[0] == '='){
						ofs << "response.write(" << tmp.substr(1) << ")" << ";";
					}else{
						ofs << tmp << ";";
					}
				}
				pos_begin = pos_flag2 + 2;
				incode = false;
			}
			ofs << "//#" << lineno << '\n';
		}
		ofs << "return true;\n}\n";
		return ofs.str();
	}

	bool script_jsx::load_string(lyramilk::data::string scriptstring)
	{
		JS_SetRuntimeThread(rt);
		init();
		JSContext* selectedcx = (JSContext *)JS_GetRuntimePrivate(rt);
		JS::RootedObject global(selectedcx,JS_GetGlobalObject(selectedcx));

		scriptstring = jsx2js(scriptstring);

		JSScript* script = JS_CompileScript(selectedcx,global,scriptstring.c_str(),scriptstring.size(),NULL,1);
		if(script){
			return !!JS_ExecuteScript(selectedcx,global,script,NULL);
		}
		return false;
	}

		struct bytecode
		{
			time_t tm;
			std::string code;
		};

	bool script_jsx::load_file(lyramilk::data::string scriptfile)
	{
		if(scriptfilename.empty()){
			scriptfilename = scriptfile;
		}

		JS_SetRuntimeThread(rt);
		init();
		JSContext* selectedcx = (JSContext *)JS_GetRuntimePrivate(rt);
		JS::RootedObject global(selectedcx,JS_GetGlobalObject(selectedcx));

		static std::map<lyramilk::data::string,bytecode> bytecodemap;
		static lyramilk::threading::mutex_rw bytecodelock;
		JSScript* script = nullptr;

		struct stat st = {0};
		if(0 !=::stat(scriptfile.c_str(),&st)){
			return false;
		}
		{
			//尝试读取
			lyramilk::threading::mutex_sync _(bytecodelock.r());
			std::map<lyramilk::data::string,bytecode>::const_iterator it = bytecodemap.find(scriptfile);
			if(it!=bytecodemap.end()){
				const bytecode& c = it->second;
				if(st.st_mtime == c.tm){
					script = JS_DecodeScript(selectedcx,(const void*)c.code.c_str(),c.code.size(),NULL,NULL);
				}
			}
		}
		if(script){
			return !!JS_ExecuteScript(selectedcx,global,script,nullptr);
		}else{
			JS::CompileOptions options(selectedcx);

			lyramilk::data::string jsxcache;
			{
				std::ifstream ifs;
				ifs.open(scriptfile.c_str());

				char buff[4096];
				while(ifs){
					ifs.read(buff,sizeof(buff));
					jsxcache.append(buff,ifs.gcount());
				}
				ifs.close();
			}

			lyramilk::data::string jscache = jsx2js(jsxcache);

			options.setFileAndLine(scriptfile.c_str(),0);
			script = JS::Compile(selectedcx,global,options,jscache.c_str(),jscache.size());
			if(!script)return false;
			if(JS_ExecuteScript(selectedcx,global,script,nullptr)){
				uint32_t len = 0;
				void* p = JS_EncodeScript(selectedcx,script,&len);
				if(p && len){
					bytecode c;
					c.tm = st.st_mtime;
					c.code.assign((const char*)p,len);
					{
						lyramilk::threading::mutex_sync _(bytecodelock.w());
						bytecodemap[scriptfile] = c;
					}
				}
				return true;
			}
			return false;
		}
	}

	lyramilk::data::string script_jsx::name()
	{
		return "jsx";
	}

	static lyramilk::script::engine* __ctr()
	{
		return new script_jsx();
	}

	static void __dtr(lyramilk::script::engine* eng)
	{
		delete (script_jsx*)eng;
	}

	static bool ___init()
	{
		lyramilk::script::engine::define("jsx",__ctr,__dtr);
		return true;
	}

#ifdef __GNUC__

	static __attribute__ ((constructor)) void __init()
	{
		___init();
	}
#else
	bool r = __init();
#endif
}}}
