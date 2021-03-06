#include "url_dispatcher.h"
#include "webservice.h"
#include "script.h"
#include <libmilk/log.h>
#include <libmilk/dict.h>
#include <sys/stat.h>
#include <cassert>

namespace lyramilk{ namespace teapoy {

	///	url_selector
	url_selector::url_selector()
	{
	}

	url_selector::~url_selector()
	{
	}


	///	url_selector_loger
	url_selector_loger::url_selector_loger(lyramilk::data::string prefix,httpadapter* adapter)
	{
		td.mark();
		this->prefix = prefix;
		this->adapter = adapter;
		this->iscancel = false;
	}

	url_selector_loger::~url_selector_loger()
	{
		if(iscancel) return;
		//lyramilk::klog(lyramilk::log::trace,prefix) << D("%s:%u-->%s 耗时%.3f(毫秒)",req->dest().c_str(),req->dest_port(),req->url().c_str(),double(td.diff()) / 1000000) << std::endl;
		lyramilk::netio::netaddress addr = adapter->channel->dest();
		int code = adapter->response->code;
		if(code == 0) code = 404;

		lyramilk::data::string method = adapter->request->get(":method");
		lyramilk::klog(lyramilk::log::trace,prefix) << D("%u %s:%u %s %s %s 耗时%.3f(毫秒)",code,addr.ip_str().c_str(),addr.port(),method.c_str(),adapter->request->url().c_str(),adapter->request->get("User-Agent").c_str(),double(td.diff()) / 1000000) << std::endl;
	}

	void url_selector_loger::cancel()
	{
		iscancel = true;
	}

	///	url_regex_selector
	url_regex_selector::url_regex_selector()
	{
		regex_handler = nullptr;
	}

	url_regex_selector::~url_regex_selector()
	{
	}

	bool url_regex_selector::init(lyramilk::data::map& m)
	{
		lyramilk::data::string emptystr;
		lyramilk::data::map emptymap;
		lyramilk::data::array emptyarray;

		allowmethod = m["method"].conv(emptystr);
		lyramilk::data::string type = m["type"].conv(emptystr);
		lyramilk::data::string pattern = m["pattern"].conv(emptystr);
		lyramilk::data::string module = m["module"].conv(emptystr);
		lyramilk::data::map& auth = m["auth"].conv(emptymap);
		lyramilk::data::strings defpages;
		{
			const lyramilk::data::array& v = m["index"].conv(emptyarray);

			lyramilk::data::array::const_iterator it = v.begin();
			for(;it!=v.end();++it){
				if(it->type_like(lyramilk::data::var::t_str)){
					defpages.push_back(it->str());
				}else{
					lyramilk::klog(lyramilk::log::warning,"teapoy.url_regex_selector.init") << D("定义url映射警告：默认页面%s类型错误,源信息：%s",it->type(),it->str().c_str()) << std::endl;
				}
			}
		}
		{
			const lyramilk::data::array& v = m["assert"].conv(emptyarray);

			lyramilk::data::array::const_iterator it = v.begin();
			for(;it!=v.end();++it){
				if(it->type_like(lyramilk::data::var::t_str)){
					int  erroffset = 0;
					const char *error = "";
					pcre* rgx = pcre_compile(it->str().c_str(),0,&error,&erroffset,nullptr);
					if(rgx){
						regex_assert.push_back(rgx);
					}
				}else{
					lyramilk::klog(lyramilk::log::warning,"teapoy.url_regex_selector.init") << D("定义url映射警告：默认页面%s类型错误,源信息：%s",it->type(),it->str().c_str()) << std::endl;
				}
			}
		}

		

		if(!init_selector(defpages,pattern,module)){
			lyramilk::klog(lyramilk::log::warning,"teapoy.url_regex_selector.init") << D("定义url映射失败：%s类型未定义",type.c_str()) << std::endl;
			return false;
		}
		if(!auth.empty()){
			init_auth(auth["type"],auth["module"]);
		}
		return true;
	}

	bool url_regex_selector::init_auth(const lyramilk::data::string& enginetype,const lyramilk::data::string& authscript)
	{
		this->authtype = enginetype;
		this->authscript = authscript;
		return true;
	}

	bool url_regex_selector::init_selector(const lyramilk::data::strings& default_pages,const lyramilk::data::string& regex_str,const lyramilk::data::string& path_pattern)
	{
		this->default_pages = default_pages;
		assert(regex_handler == nullptr);
		int  erroffset = 0;
		const char *error = "";
		if(regex_handler == nullptr){
			regex_handler = pcre_compile(regex_str.c_str(),0,&error,&erroffset,nullptr);
		}
		if(regex_handler == nullptr){
			lyramilk::klog(lyramilk::log::warning,"teapoy.url_regex_selector.init") << D("编译正则表达式%s失败",regex_str.c_str()) << std::endl;
			return false;
		}

		error = "";
		//regex_handler_study = pcre_study(regex_handler,PCRE_EXTRA_STUDY_DATA|PCRE_EXTRA_MATCH_LIMIT|PCRE_EXTRA_MATCH_LIMIT_RECURSION|PCRE_EXTRA_CALLOUT_DATA|PCRE_EXTRA_TABLES,&err);
		regex_handler_study = pcre_study(regex_handler,0,&error);

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
						bool isgroupindex = true;
						for(lyramilk::data::string::iterator numberit = su.begin();numberit!=su.end();++numberit){
							if(!isdigit(*numberit)){
								isgroupindex = false;
							}
						}

						if(isgroupindex){
							{
								url_regex_part q;
								q.type = url_regex_part::t_static;
								q.str = ret;
								url_to_path_rule.push_back(q);
							}
							{
								url_regex_part q;
								q.type = url_regex_part::t_index;
								q.index = atoi(su.c_str());
								url_to_path_rule.push_back(q);
							}
						}else{
							{
								url_regex_part q;
								q.type = url_regex_part::t_static;
								q.str = ret;
								url_to_path_rule.push_back(q);
							}
							/**
							{
								url_regex_part q;
								q.type = url_regex_part::t_group;
								q.str = su;
								url_to_path_rule.push_back(q);
							}
							/*/
							{
								int idx = pcre_get_stringnumber(regex_handler,su.c_str());
								url_regex_part q;
								q.type = url_regex_part::t_index;
								q.index = idx;
								url_to_path_rule.push_back(q);
							}
							/**/
						}
						i = d;
						ret.clear();
						continue;
					}
				}
			}
			ret.push_back(c);
		}

		{
			url_regex_part q;
			q.type = url_regex_part::t_static;
			q.str = ret;
			url_to_path_rule.push_back(q);
		}

		this->path_pattern = path_pattern;
		return true;
	}

	bool url_regex_selector::url_to_localtion(const lyramilk::data::string& url,lyramilk::data::string *real)
	{
		if(!regex_handler) return false;

		const static int ov_size = 2048;

		for(std::vector<pcre*>::const_iterator it = regex_assert.begin();it!=regex_assert.end();++it){
			int ov[ov_size] = {0};
			int rc = pcre_exec((const pcre*)*it,nullptr,url.c_str(),url.size(),0,0,ov,ov_size);
			if(rc < 1) return false;
		}

		int ov[ov_size] = {0};
		int rc = pcre_exec(regex_handler,regex_handler_study,url.c_str(),url.size(),0,0,ov,ov_size);
		if(rc > 0){
			real->clear();
			for(url_parts_type::const_iterator it = url_to_path_rule.begin();it!=url_to_path_rule.end();++it){
				if(it->type == url_regex_part::t_static){
					real->append(it->str);
				}else if(it->type == url_regex_part::t_index){
					int qi = it->index;
					int bof = ov[(qi<<1)];
					int eof = ov[(qi<<1)|1];
					if(eof - bof > 0 && bof < (int)url.size()){
						real->append(url.substr(bof,eof-bof));
					}
				}else if(it->type == url_regex_part::t_group){
					const char* substr = nullptr;
					int r = pcre_get_named_substring(regex_handler,url.c_str(),ov,rc,it->str.c_str(),&substr);
					if(r > 0 || substr != nullptr){
						std::cout.write(substr,r) << std::endl;
						real->append(substr);
						pcre_free_substring(substr);
						TODO();

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

	dispatcher_check_status url_regex_selector::hittest(httprequest* request,httpresponse* response,httpadapter* adapter)
	{
		dispatcher_check_status cs = cs_pass;

		if(!allowmethod.empty() && allowmethod.find("*") == allowmethod.npos){
			if(allowmethod.find(request->get(":method")) == allowmethod.npos){
				return cs_pass;
			}		
		}

		lyramilk::data::string realfile;
		if(!url_to_localtion(request->url(),&realfile)){
			return cs_pass;
		}

		if(!path_pattern.empty()){
			struct stat st = {0};
			if(0 !=::stat(realfile.c_str(),&st)){
#ifdef _DEBUG
				lyramilk::klog(lyramilk::log::debug,"teapoy.url_regex_selector.hittest") << D("虽然url(%s)与模式(%s)匹配，但本地文件%s没有找到",request->url().c_str(),path_pattern.c_str(),realfile.c_str()) << std::endl;
#endif
				return cs_pass;
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
					return cs_pass;
				}
			}
		}
#ifdef _DEBUG
		lyramilk::klog(lyramilk::log::debug,"teapoy.url_regex_selector.hittest") << D("url(%s)与模式(%s)匹配，本地文件%s找到",request->url().c_str(),path_pattern.c_str(),realfile.c_str()) << std::endl;
#endif
		cs = check_auth(request,response,adapter,realfile);
		if(cs == cs_pass){
			return call(request,response,adapter,realfile);
		}
		return cs;
	}


	dispatcher_check_status url_regex_selector::check_auth(httprequest* request,httpresponse* response,httpadapter* adapter,const lyramilk::data::string& real)
	{
		if(authtype.empty()) return cs_pass; 
		{
			lyramilk::script::engines* es = engine_pool::instance()->get(authtype);
			if(!es) return cs_error;

			lyramilk::script::engines::ptr eng = es->get();
			if(!eng) return cs_error;


			if(!eng->load_file(authscript)){
				lyramilk::klog(lyramilk::log::warning,"teapoy.web.processer.auth") << D("加载文件%s失败",authscript.c_str()) << std::endl;

				adapter->response->code = 500;
				return cs_error;
			}


			lyramilk::data::stringstream ss;
			lyramilk::data::array ar;
			{
				response->set("Content-Type","application/json;charset=utf8");

				lyramilk::data::array jsin_param;
				jsin_param.push_back(session_response_datawrapper(adapter,ss));

				ar.push_back(eng->createobject("HttpRequest",jsin_param));
				ar.push_back(eng->createobject("HttpResponse",jsin_param));
				if(request->header_extend.empty()){
					ar.push_back(lyramilk::data::var::nil);
				}else{
					ar.push_back(request->header_extend);
				}
			}
			lyramilk::data::var vret;
			if(!eng->call("auth",ar,&vret)){
				adapter->response->code = 501;
			}
			if(vret.type() == lyramilk::data::var::t_invalid){
				adapter->response->code = 500;
				return cs_error;
			}
			if(vret.type_like(lyramilk::data::var::t_bool)){
				if((bool)vret){
					return cs_pass;
				}else{
					return cs_ok;
				}
			}
		}
		//if(ret)*ret = true;
		adapter->response->code = 500;
		return cs_error;
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

	dispatcher_check_status url_dispatcher::call(httprequest* request,httpresponse* response,httpadapter* adapter)
	{
		url_selector_loger _("teapoy.web.dispatcher",adapter);

		if(request == nullptr || response == nullptr) return cs_error;
		std::list<lyramilk::ptr<url_selector> >::iterator it = selectors.begin();
		for(;it!=selectors.end();++it){
			lyramilk::ptr<url_selector>& ptr = *it;
			dispatcher_check_status cs = ptr->hittest(request,response,adapter);
			if(cs != cs_pass){
				_.cancel();
				return cs;
			}
		}
		return cs_pass;
	}

	bool url_dispatcher::add(lyramilk::ptr<url_selector> selector)
	{
		selectors.push_back(selector);
		return true;
	}
}}
