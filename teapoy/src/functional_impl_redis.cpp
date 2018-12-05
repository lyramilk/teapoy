#include "functional_impl_redis.h"
#include "redis.h"
#include <unistd.h>

namespace lyramilk{ namespace teapoy {

	//	functional_impl_redis_instance
	#define FUNCTIONAL_TYPE	"redis"

	bool functional_impl_redis_instance::init(const lyramilk::data::map& cm)
	{
		lyramilk::data::map m = cm;
		redis::redis_client* s = new redis::redis_client;

		if(!s->open(m["host"].str(),m["port"].conv(-1))){
			delete s;
			return false;
		}

		if(m["password"].type() != lyramilk::data::var::t_invalid && !s->auth(m["password"].str())){
			delete s;
			return false;
		}

		if(c){
			delete c;
		}
		c = s;	
		return true;
	}

	lyramilk::data::var functional_impl_redis_instance::exec(const lyramilk::data::array& ar)
	{
		if(!c) return lyramilk::data::var::nil;
		if(ar.empty() || !ar[0].type_like(lyramilk::data::var::t_str)){
			return lyramilk::data::var::nil;
		}
		lyramilk::data::string cmd = ar[0];
		lyramilk::data::var result;
		if(c->exec(ar,result)){
			return result;
		}
		return lyramilk::data::var::nil;
	}

	functional_impl_redis_instance::functional_impl_redis_instance()
	{
		c = nullptr;
	}

	functional_impl_redis_instance::~functional_impl_redis_instance()
	{
		if(c)delete c;
	}


	// functional_impl_redis
	functional_impl_redis::functional_impl_redis()
	{}
	functional_impl_redis::~functional_impl_redis()
	{}

	bool functional_impl_redis::init(const lyramilk::data::map& m)
	{
		info = m;
		return true;
	}

	functional::ptr functional_impl_redis::get_instance()
	{
		std::list<functional_impl_redis_instance*>::iterator it = es.begin();
		for(;it!=es.end();++it){
			if((*it)->try_lock()){
				return functional::ptr(*it);
			}
		}
		functional_impl_redis_instance* tmp = underflow();
		if(tmp){
			functional_impl_redis_instance* pe = nullptr;
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

	lyramilk::data::string functional_impl_redis::name()
	{
		return FUNCTIONAL_TYPE;
	}


	functional_impl_redis_instance* functional_impl_redis::underflow()
	{
		functional_impl_redis_instance* ins = new functional_impl_redis_instance;
		ins->init(info);
		return ins;
	}


	//

	static functional_multi* ctr(void* args)
	{
		return new functional_impl_redis;
	}

	static void dtr(functional_multi* p)
	{
		delete (functional_impl_redis*)p;
	}

	static __attribute__ ((constructor)) void __init()
	{
		functional_type_master::instance()->define(FUNCTIONAL_TYPE,ctr,dtr);
	}

}}
