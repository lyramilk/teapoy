#include "functional_impl_logfile.h"

namespace lyramilk{ namespace teapoy {
	#define FUNCTIONAL_TYPE	"logfile"

	bool functional_impl_logfile_instance::init(const lyramilk::data::map& cm)
	{
		lyramilk::data::map::const_iterator it = cm.find("filefmt");
		if(it == cm.end()) return false;
		return lf.init(it->second.str(),false);
	}

	lyramilk::data::var functional_impl_logfile_instance::exec(const lyramilk::data::array& ar)
	{
		if(ar.size() < 1) return false;
		lyramilk::data::string cmd = ar[0].str();


		lyramilk::data::string msg;
		for(unsigned int i = 1;i<ar.size();++i){
			msg.append(ar[i].str());
			msg.push_back(' ');
		}


		//输入日志时只用当前时间分页。
		time_t t_now = time(nullptr);
		tm __t;
		tm *t = localtime_r(&t_now,&__t);


		char buff[128];
		int r = ::strftime(buff,sizeof(buff),"%T ",t);



		if(cmd == "trace"){
			lyramilk::data::string cache;
			cache.append(buff,r);
			cache.append("[TRACE] ");
			cache.append(msg);
			cache.append("\n");
			lf.append(cache.c_str(),cache.size());
		}else if(cmd == "error"){
			lyramilk::data::string cache;
			cache.append(buff,r);
			cache.append("[ERROR] ");
			cache.append(msg);
			cache.append("\n");
			lf.append(cache.c_str(),cache.size());
		}
		return true;
	}

	functional_impl_logfile_instance::functional_impl_logfile_instance()
	{
	}

	functional_impl_logfile_instance::~functional_impl_logfile_instance()
	{
	}

	// functional_impl_logfile
	functional_impl_logfile::functional_impl_logfile()
	{
		ins = nullptr;
	}
	functional_impl_logfile::~functional_impl_logfile()
	{
		if(ins) delete ins;
	}

	bool functional_impl_logfile::init(const lyramilk::data::map& m)
	{
		info = m;
		ins = new functional_impl_logfile_instance;
		ins->init(info);
		return true;
	}

	functional::ptr functional_impl_logfile::get_instance()
	{

		return ins;
	}

	lyramilk::data::string functional_impl_logfile::name()
	{
		return FUNCTIONAL_TYPE;
	}

	//
	static functional_multi* ctr(void* args)
	{
		return new functional_impl_logfile;
	}

	static void dtr(functional_multi* p)
	{
		delete (functional_impl_logfile*)p;
	}

	static __attribute__ ((constructor)) void __init()
	{
		functional_type_master::instance()->define(FUNCTIONAL_TYPE,ctr,dtr);
	}
}}
