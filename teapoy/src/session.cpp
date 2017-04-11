#include <libmilk/var.h>
#include "session.h"
#include <stdlib.h>
#include <libmilk/thread.h>
#include <pthread.h>

namespace lyramilk{ namespace teapoy { namespace web {

	class memory_session:public sessions
	{
	  private:
		struct timedmap
		{
			lyramilk::data::var::map m;
			time_t tm;
			timedmap()
			{
				tm = time(0);
			}
		};
		time_t st;
		std::map<lyramilk::data::string,timedmap> k;

		static lyramilk::data::string makeid()
		{
			srand(time(0));
			union{
				int i[4];
				char c[1];
			}u;
			u.i[0] = rand();
			u.i[1] = rand();
			u.i[2] = rand();
			u.i[3] = rand();
			int size = sizeof(u);
			lyramilk::data::string str;
			str.reserve(size);
			for(int i=0;i<size;++i){
				unsigned char c = u.c[i];
				unsigned char q = c % 36;
				if(q <10){
					str.push_back(q + '0');
				}else{
					str.push_back(q - 10 + 'A');
				}
			}
			return str;
		}

		void gc()
		{
			lyramilk::threading::mutex_sync _(lock);
			time_t now = time(0);
			time_t q = now - 600;
			for(std::map<lyramilk::data::string,timedmap>::iterator it = k.begin();it!=k.end();++it){
				if(it->second.tm < q){
					k.erase(it);
				}
			}
		}

	  private:
		virtual lyramilk::data::var& get(const lyramilk::data::string& sid,const lyramilk::data::string& key)
		{
			lyramilk::threading::mutex_sync _(lock);
			timedmap& t = k[sid];
			t.tm = time(0);
			return t.m[key];
		}
		virtual void set(const lyramilk::data::string& sid,const lyramilk::data::string& key,const lyramilk::data::var& value)
		{
			lyramilk::threading::mutex_sync _(lock);
			timedmap& t = k[sid];
			t.tm = time(0);
			t.m[key] = value;
		}

		lyramilk::data::string newid()
		{
			std::map<lyramilk::data::string,lyramilk::data::var::map>::iterator it;
			while(true){
				lyramilk::data::string id = makeid();
				if(k.find(id) == k.end()){
					timedmap& t = k[id];
					t.tm = time(0);
					return id;
				}
			}
		}
		pthread_t thread;
		bool ok;
		lyramilk::threading::mutex_os lock;

		static int thread_task(memory_session* p)
		{
			while(p->ok){
				try{
					p->gc();
				}catch(...){
				}
				sleep(10);
			}
			return 0;
		}

	  public:
		memory_session()
		{
			st = time(0);
			ok = true;
			pthread_create(&thread,NULL,(void* (*)(void*))thread_task,this);
		}
		virtual ~memory_session()
		{
			ok = false;
			pthread_join(thread,nullptr);
		}
	};



	sessions* sessions::defaultinstance()
	{
		static memory_session _mm;
		return &_mm;
	}


	sessions_factory* sessions_factory::instance()
	{
		static sessions_factory _mm;
		return &_mm;
	}

}}}
