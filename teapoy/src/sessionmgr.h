#ifndef _lyramilk_teapoy_sessionmgr_h_
#define _lyramilk_teapoy_sessionmgr_h_

#include "config.h"
#include "webservice.h"
#include <libmilk/factory.h>
#include <libmilk/thread.h>

namespace lyramilk{ namespace teapoy {


	class httpsessionptr
	{
		httpsession* p;
	  public:

		httpsessionptr()
		{
			p = nullptr;
		}

		httpsessionptr(httpsession* p)
		{
			this->p = p;
			if(p){
				p->add_ref();
			}
		}

		httpsessionptr(const httpsessionptr& o)
		{
			this->p = const_cast<httpsessionptr&>(o).p;
			if(p){
				p->add_ref();
			}
		}

		httpsessionptr& operator = (httpsession* p)
		{
			this->p = p;
			if(p){
				p->add_ref();
			}
			return *this;
		}

		httpsessionptr& operator = (const httpsessionptr& o)
		{
			this->p = const_cast<httpsessionptr&>(o).p;
			if(p){
				p->add_ref();
			}
			return *this;
		}

	  	virtual ~httpsessionptr()
		{
			if(p){
				p->relese();
			}
		}

		httpsession* operator->()
		{
			return p;
		}

		operator httpsession*()
		{
			return p;
		}
	};

	class httpsession_manager
	{
	  public:
		httpsession_manager();
	  	virtual ~httpsession_manager();

		virtual bool init(const lyramilk::data::map& info) = 0;

		virtual httpsessionptr get_session(const lyramilk::data::string& sessionid) = 0;
		virtual void destory_session(httpsession* ses) = 0;
	  public:
		virtual httpsessionptr create_session()
		{
			return get_session("");
		}
	};

	class httpsession_manager_factory : public lyramilk::util::factory<httpsession_manager>
	{
	  public:
		httpsession_manager_factory();
	  	virtual ~httpsession_manager_factory();

		static httpsession_manager_factory* instance();
	};


}}

#endif
