#include <libmilk/var.h>
#include <libmilk/log.h>
#include <libmilk/dict.h>
#include <libmilk/netaio.h>
#include <libmilk/scriptengine.h>
#include <libmilk/xml.h>
#include "script.h"

namespace lyramilk{ namespace teapoy{
	static lyramilk::log::logss log(lyramilk::klog,"teapoy.native");

	lyramilk::data::var xml_stringify(const lyramilk::data::array& args,const lyramilk::data::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_map);
		bool has_header = false;
		if(args.size() > 1){
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_bool);
			has_header = args[1];
		}
		const lyramilk::data::map& m = args[0];

		const static lyramilk::data::string xmldecoear = "<?xml version=\"1.0\" standalone=\"yes\"?>";

		lyramilk::data::string xmlstr;
		if(!lyramilk::data::xml::stringify(m,&xmlstr)){
			throw lyramilk::exception(D("生成%s失败","xml"));
		}

		if(has_header){
			return xmldecoear + xmlstr;
		}
		return xmlstr;
	}

	lyramilk::data::var xml_parse(const lyramilk::data::array& args,const lyramilk::data::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
		lyramilk::data::string str = args[0].str();

		lyramilk::data::map m;
		if(!lyramilk::data::xml::parse(str,&m)){
			throw lyramilk::exception(D("解析%s失败","xml"));
		}
		return m;
	}

	static int define(lyramilk::script::engine* p)
	{
		p->define("xml_stringify",xml_stringify);
		p->define("xml_parse",xml_parse);
		return 2;
	}


	static __attribute__ ((constructor)) void __init()
	{
		lyramilk::teapoy::script2native::instance()->regist("xml",define);
	}

}}