#include "env.h"
#include <libmilk/thread.h>

namespace lyramilk{ namespace teapoy
{
	static lyramilk::data::map __g_cfg;
	static lyramilk::threading::mutex_rw __g_cfg_lock;

	const lyramilk::data::var& env::get(const lyramilk::data::string& cfgname)
	{
		lyramilk::threading::mutex_sync _(__g_cfg_lock.r());
		lyramilk::data::map::iterator it = __g_cfg.find(cfgname);
		if(it != __g_cfg.end()) return it->second;
		return lyramilk::data::var::nil;
	}

	void env::set(const lyramilk::data::string& cfgname,const lyramilk::data::var& cfgvalue)
	{
		lyramilk::threading::mutex_sync _(__g_cfg_lock.w());
		if(cfgvalue.type() != lyramilk::data::var::t_invalid){
			__g_cfg[cfgname] = cfgvalue;
		}else{
			__g_cfg.erase(cfgname);
		}
	}

	static bool __g_b_is_debug = true;
	bool env::is_debug()
	{
		return __g_b_is_debug;
	}

	void env::set_debug(bool b)
	{
		__g_b_is_debug = b;
	}
}}
