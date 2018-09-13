#include "url_dispatcher.h"
#include "webservice.h"
#include <libmilk/log.h>
#include <libmilk/multilanguage.h>
#include <sys/stat.h>

namespace lyramilk{ namespace teapoy {

	///	url_selector
	url_selector::url_selector()
	{
	}

	url_selector::~url_selector()
	{
	}

	///	url_regex_selector
	url_regex_selector::url_regex_selector()
	{
		regex_handler = nullptr;
	}

	url_regex_selector::~url_regex_selector()
	{
	}

	bool url_regex_selector::init(const lyramilk::data::strings& default_pages,const lyramilk::data::string& regex_str,const lyramilk::data::string& path_pattern)
	{
		this->default_pages = default_pages;
		assert(regex_handler == nullptr);
		if(regex_handler == nullptr){
			int  erroffset = 0;
			const char *error = "";
			regex_handler = pcre_compile(regex_str.c_str(),0,&error,&erroffset,nullptr);
		}
		if(regex_handler == nullptr){
			lyramilk::klog(lyramilk::log::warning,"teapoy.url_regex_selector.init") << D("编译正则表达式%s失败",regex_str.c_str()) << std::endl;
			return false;
		}

		this->regex_str = regex_str;
		url_to_path_rule.clear();
		lyramilk::data::string ret;
		int sz = path_pattern.size();
		for(int i=0;i<sz;++i){
			const char &c = path_pattern[i];
			if(c == '$'){
				if(path_pattern[i + 1] == '{'){
					std::size_t d = path_pattern.find('}',i+1);
					if(d != path_pattern.npos){
						lyramilk::data::string su = path_pattern.substr(i+2,d - i - 2);
						int qi = atoi(su.c_str());
						url_to_path_rule.push_back(ret);
						url_to_path_rule.push_back(qi);
						ret.clear();
						i = d;
						continue;
					}
				}
			}
			ret.push_back(c);
		}
		url_to_path_rule.push_back(ret);
		this->path_pattern = path_pattern;

		return true;
	}

	bool url_regex_selector::test(httprequest* request,lyramilk::data::string *real)
	{
		if(!regex_handler) return false;

		lyramilk::data::string url = request->url();
		int ov[256] = {0};
		int rc = pcre_exec(regex_handler,nullptr,url.c_str(),url.size(),0,0,ov,256);
		if(rc > 0){
			real->clear();
			for(lyramilk::data::var::array::const_iterator it = url_to_path_rule.begin();it!=url_to_path_rule.end();++it){
				if(it->type() == lyramilk::data::var::t_str){
					real->append(it->str());
				}else if(it->type_like(lyramilk::data::var::t_int)){
					int qi = *it;
					int bof = ov[(qi<<1)];
					int eof = ov[(qi<<1)|1];
					if(eof - bof > 0 && bof < (int)url.size()){
						real->append(url.substr(bof,eof-bof));
					}
				}
			}
			std::size_t pos_args = real->find("?");
			if(pos_args!=real->npos){
				*real = real->substr(0,pos_args);
			}
			return true;
		}
		return false;
	}

	bool url_regex_selector::hittest(httprequest* request,httpresponse* response,httpadapter* adapter)
	{
		lyramilk::data::string realfile;
		if(!test(request,&realfile)){
			return false;
		}
		if(!path_pattern.empty()){
			struct stat st = {0};
			if(0 !=::stat(realfile.c_str(),&st)){
#ifdef _DEBUG
			lyramilk::klog(lyramilk::log::warning,"teapoy.url_regex_selector.hittest") << D("虽然url(%s)与模式匹配，但本地文件%s没有找到",request->url().c_str(),realfile.c_str()) << std::endl;
#endif
				return false;
			}
			if(st.st_mode&S_IFDIR){
				lyramilk::data::string rawdir;
				realfile.push_back('/');
				std::size_t pos_end = realfile.find_last_not_of("/");
				rawdir = realfile.substr(0,pos_end + 1);
				rawdir.push_back('/');
				lyramilk::data::strings::const_iterator it = default_pages.begin();

				for(;it!=default_pages.end();++it){
					realfile = rawdir + *it;
					if(0 == ::stat(realfile.c_str(),&st) && !(st.st_mode&S_IFDIR)){
						break;
					}
				}
				if (it == default_pages.end()){
					return false;
				}
			}
		}
#ifdef _DEBUG
		lyramilk::klog(lyramilk::log::debug,"teapoy.url_regex_selector.hittest") << D("url(%s)与模式匹配，本地文件%s找到",request->url().c_str(),realfile.c_str()) << std::endl;
#endif
		return call(request,response,adapter,realfile);

	}

	/// url_selector_factory
	url_selector_factory* url_selector_factory::instance()
	{
		static url_selector_factory _mm;
		return &_mm;
	}

	///	url_dispatcher
	url_dispatcher::url_dispatcher()
	{

	}

	url_dispatcher::~url_dispatcher()
	{
	}

	bool url_dispatcher::call(httprequest* request,httpresponse* response,httpadapter* adapter)
	{
		if(request == nullptr || response == nullptr) return false;
		std::list<lyramilk::ptr<url_selector> >::iterator it = selectors.begin();
		for(;it!=selectors.end();++it){
			lyramilk::ptr<url_selector>& ptr = *it;
			if(ptr->hittest(request,response,adapter)){
				return true;
			}
		}
		return false;
	}

	bool url_dispatcher::add(lyramilk::ptr<url_selector> selector)
	{
		selectors.push_back(selector);
		return true;
	}
}}
