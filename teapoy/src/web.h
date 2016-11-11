#ifndef _lyramilk_teapoy_web_h_
#define _lyramilk_teapoy_web_h_

#include "http.h"
#include <libmilk/netaio.h>
#include <libmilk/factory.hpp>
#include <map>

#define SERVER_VER "teapoy/3.1.0"

namespace lyramilk{ namespace teapoy { namespace web {
	class methodinvoker;

	class urlfilter
	{
	  public:
		virtual bool convert(lyramilk::data::string& url) const = 0;
	};

	class processer_selector
	{
	  public:
		virtual bool invoke(methodinvoker* invoker,lyramilk::teapoy::http::request* req,std::ostream& os,bool* ret) = 0;
	};

	class parametersfilter
	{
	  public:
		virtual bool convert(lyramilk::data::var::map& args) const = 0;
	};

	class methodinvoker
	{
		urlfilter* puf;
		parametersfilter* paf;
		processer_selector* pcc;
	  public:
		methodinvoker();
		virtual ~methodinvoker();
		virtual bool call(lyramilk::teapoy::http::request* req,std::ostream& os) = 0;
		virtual void set_url_filter(urlfilter* uf);
		virtual void set_parameters_filter(parametersfilter* af);
		virtual void set_processer(processer_selector* cs);

		virtual bool convert_url(lyramilk::data::string& url) const;
		virtual bool convert_parameters(lyramilk::data::var::map& args) const;
		virtual bool process(lyramilk::teapoy::http::request* req,std::ostream& os,bool* ret);
	};

	class methodinvokers:public lyramilk::util::multiton_factory<methodinvoker>
	{
	  public:
		static methodinvokers *instance();

	};

	class aiohttpsession:public lyramilk::netio::aiosession2
	{
		lyramilk::teapoy::http::request req;
	  public:
		lyramilk::data::string root;
		aiohttpsession();
		virtual ~aiohttpsession();

		virtual bool onrequest(const char* cache,int size,std::ostream& os);
	};
}}}

#endif
