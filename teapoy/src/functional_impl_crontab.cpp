#include "functional_impl_crontab.h"

namespace lyramilk{ namespace teapoy {
	#define FUNCTIONAL_TYPE	"crontab"

	bool functional_impl_crontab_instance::init(const lyramilk::data::map& cm)
	{
		TODO();
	}

	lyramilk::data::var functional_impl_crontab_instance::exec(const lyramilk::data::array& ar)
	{
		TODO();

	}

	functional_impl_crontab_instance::functional_impl_crontab_instance()
	{
		TODO();
	}

	functional_impl_crontab_instance::~functional_impl_crontab_instance()
	{
		TODO();
	}

	// functional_impl_crontab
	functional_impl_crontab::functional_impl_crontab()
	{}
	functional_impl_crontab::~functional_impl_crontab()
	{}

	bool functional_impl_crontab::init(const lyramilk::data::map& m)
	{
		info = m;
		return true;
	}

	functional::ptr functional_impl_crontab::get_instance()
	{
		std::list<functional_impl_crontab_instance*>::iterator it = es.begin();
		for(;it!=es.end();++it){
			if((*it)->try_lock()){
				return functional::ptr(*it);
			}
		}
		functional_impl_crontab_instance* tmp = underflow();
		if(tmp){
			functional_impl_crontab_instance* pe = nullptr;
			{
				lyramilk::threading::mutex_sync _(l);
				es.push_back(tmp);
				pe = es.back();
				pe->lock();
			}
			return functional::ptr(pe);

		}
		return functional::ptr(nullptr);
	}

	lyramilk::data::string functional_impl_crontab::name()
	{
		return FUNCTIONAL_TYPE;
	}


	functional_impl_crontab_instance* functional_impl_crontab::underflow()
	{
		functional_impl_crontab_instance* ins = new functional_impl_crontab_instance;
		ins->init(info);
		return ins;
	}


	//
	static functional_multi* ctr(void* args)
	{
		return new functional_impl_crontab;
	}

	static void dtr(functional_multi* p)
	{
		delete (functional_impl_crontab*)p;
	}

	static __attribute__ ((constructor)) void __init()
	{
		functional_type_master::instance()->define(FUNCTIONAL_TYPE,ctr,dtr);
	}
}}
