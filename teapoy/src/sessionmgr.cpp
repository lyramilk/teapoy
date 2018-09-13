#include "sessionmgr.h"
#include <libmilk/thread.h>
#include <libmilk/md5.h>
#include <unistd.h>

namespace lyramilk{ namespace teapoy {
	//	httpsession_manager
	httpsession_manager::httpsession_manager()
	{}

	httpsession_manager::~httpsession_manager()
	{}


	//	httpsession_manager_factory
	httpsession_manager_factory::httpsession_manager_factory()
	{}

	httpsession_manager_factory::~httpsession_manager_factory()
	{}

	httpsession_manager_factory* httpsession_manager_factory::instance()
	{
		static httpsession_manager_factory _mm;
		return &_mm;
	}



	const unsigned int time_alive_seconds = 1200;




	class httpsession_default:public httpsession
	{
	  public:
		lyramilk::data::stringdict smap;
		lyramilk::data::string sid;
		time_t tm_life;
		lyramilk::threading::mutex_spin lock;

		httpsession_default()
		{
		}

		virtual ~httpsession_default()
		{
		}
		virtual bool init(const lyramilk::data::var::map& info)
		{
			return true;
		}

		virtual bool set(const lyramilk::data::string& key,const lyramilk::data::string& value)
		{
			lyramilk::threading::mutex_sync _(lock);

			tm_life = time(nullptr) + time_alive_seconds;
			if(value.empty()){
				smap.erase(key);
			}else{
				smap[key] = value;
			}
		}

		virtual lyramilk::data::string get(const lyramilk::data::string& key)
		{
			lyramilk::threading::mutex_sync _(lock);

			lyramilk::data::stringdict::const_iterator it = smap.find(key);
			if(it == smap.end()){
				return "";
			}

			return it->second;
		}

		lyramilk::data::string get_session_id()
		{
			return sid;
		}

	};

	//	httpsession_manager_default
	class httpsession_manager_default:public httpsession_manager , public lyramilk::threading::threads
	{
		std::map<lyramilk::data::string,httpsession_default*> mgr;
		lyramilk::threading::mutex_rw lock;

	  public:
		static httpsession_manager* ctr(void*)
		{
			return new httpsession_manager_default;
		}


		static void dtr(httpsession_manager* p)
		{
			delete (httpsession_manager_default*)p;
		}

		httpsession_manager_default()
		{
		}

	  	virtual ~httpsession_manager_default()
		{
		}

		virtual bool init(const lyramilk::data::var::map& info)
		{
			active(1);
			return true;
		}

		virtual httpsessionptr get_session(const lyramilk::data::string& sessionid)
		{
			time_t tm_life = time(nullptr) + time_alive_seconds;
			{
				lyramilk::threading::mutex_sync _(lock.r());
				std::map<lyramilk::data::string,httpsession_default*>::iterator it = mgr.find(sessionid);
				if(it != mgr.end()){
					it->second->tm_life = tm_life;
					return it->second;
				}

			}
			{
				httpsession_default* ins = new httpsession_default;
				lyramilk::cryptology::md5 mkey;
				mkey << (void*)ins;

				while(true){
					ins->sid = mkey.str16();
					ins->tm_life = 0;
					{
						lyramilk::threading::mutex_sync _(lock.w());
						std::pair<std::map<lyramilk::data::string,httpsession_default*>::iterator,bool> r = mgr.insert(std::pair<lyramilk::data::string,httpsession_default*>(ins->sid,ins));
						if(r.second){
							return r.first->second;
						}
					}
					mkey << "lyramilk.com";
				}
			}
			return nullptr;
		}

		virtual void destory_session(httpsessionptr& ses)
		{
			delete (httpsession_default*)(httpsession*)ses;
		}

		virtual int svc()
		{
			while(true){
#ifdef _DEBUG
				sleep(1);
#else
				sleep(10);
#endif
				time_t tm_now = time(nullptr);
				lyramilk::data::strings sids;
				{
					lyramilk::threading::mutex_sync _(lock.r());
					std::map<lyramilk::data::string,httpsession_default*>::const_iterator it = mgr.begin();
					for(;it!=mgr.end();++it){
						if(it->second->tm_life < tm_now){
							sids.push_back(it->first);
						}
					}
				}

				{
					lyramilk::data::strings::const_iterator sid_it = sids.begin();
					for(;sid_it!=sids.end();++sid_it){
						lyramilk::threading::mutex_sync _(lock.w());
						std::map<lyramilk::data::string,httpsession_default*>::iterator it = mgr.find(*sid_it);
						if(it != mgr.end()){
							if(!it->second->in_using()){
								mgr.erase(it);
COUT << "清理会话<" << it->first << ">" << std::endl;
							}
						}
					}
				}
			}
			return 0;
		}
	};




	static __attribute__ ((constructor)) void __init()
	{
		httpsession_manager_factory::instance()->define("default",httpsession_manager_default::ctr,httpsession_manager_default::dtr);
	}
}}
