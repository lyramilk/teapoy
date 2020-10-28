#include "cvmap_dispatcher.h"
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <libmilk/log.h>
#include <libmilk/dict.h>
#include <libmilk/json.h>
#include <libmilk/testing.h>
#include <libmilk/netio.h>


namespace lyramilk{ namespace teapoy {
	// cvmap_selector
	cvmap_selector::cvmap_selector()
	{
	}

	cvmap_selector::~cvmap_selector()
	{
	}

	// cvmap_script_selector
	cvmap_script_selector::cvmap_script_selector()
	{
		es = nullptr;
	}

	cvmap_script_selector::~cvmap_script_selector()
	{
	}

	bool cvmap_script_selector::init(const lyramilk::data::string& enginetype,const lyramilk::data::string& path_pattern)
	{
		es = engine_pool::instance()->get(enginetype);
		if(es == nullptr) return false;
		this->enginetype = enginetype;


		cells.clear();
		cells.reserve(5);

		lyramilk::data::string str;
		int sz = path_pattern.size();
		for(int i=0;i < sz;++i){
			const char &c = path_pattern[i];
			if(c == '$' && i + 3 < sz && path_pattern[i + 1] == '{' && path_pattern[i + 3] == '}'){
				if(path_pattern[i + 2] == 'c'){
					if(!str.empty()){
						cvpatterncell cell;
						cell.t = cvpatterncell::cvc_str;
						cell.s = str;
						str.clear();
						cells.push_back(cell);
					}
					cvpatterncell cell;
					cell.t = cvpatterncell::cvc_c;
					cells.push_back(cell);
					i += 3;
					continue;
				} else if(path_pattern[i + 2] == 'v'){
					if(!str.empty()){
						cvpatterncell cell;
						cell.t = cvpatterncell::cvc_str;
						cell.s = str;
						str.clear();
						cells.push_back(cell);
					}
					cvpatterncell cell;
					cell.t = cvpatterncell::cvc_v;
					cells.push_back(cell);
					i += 3;
					continue;
				}
			}
			str.push_back(c);
		}
		if(!str.empty()){
			cvpatterncell cell;
			cell.t = cvpatterncell::cvc_str;
			cell.s = str;
			str.clear();
			cells.push_back(cell);
		}
		return true;
	}

	dispatcher_check_status cvmap_script_selector::test(const lyramilk::data::string& c,lyramilk::data::int64 v,lyramilk::data::string *real) const
	{
		real->clear();
		std::vector<cvpatterncell>::const_iterator it = cells.begin();
		for(;it!=cells.end();++it){
			if(it->t == cvpatterncell::cvc_str){
				real->append(it->s);
			}else if(it->t == cvpatterncell::cvc_c){
				real->append(c);
			}else if(it->t == cvpatterncell::cvc_v){
				real->append(lyramilk::data::str(v));
			}
		}
		return cs_ok;
	}

	dispatcher_check_status cvmap_script_selector::hittest(const lyramilk::data::string& c,lyramilk::data::int64 v,const lyramilk::data::map& request,lyramilk::data::map* response)
	{
		lyramilk::data::string realfile;
		dispatcher_check_status cs = test(c,v,&realfile);
		if(cs != cs_ok){
			return cs;
		}

		struct stat st = {0};
		if(0 !=::stat(realfile.c_str(),&st)){
			if(errno == ENOENT){
				return cs_pass;
			}else if(errno == EACCES){
				return cs_error;
			}else if(errno == ENAMETOOLONG){
				return cs_error;
			}
			return cs_error;
		}

		if(st.st_mode&S_IFDIR){
			return cs_error;
		}

		return call(realfile,request,response);
	}

	dispatcher_check_status cvmap_script_selector::call(const lyramilk::data::string& scriptpath,const lyramilk::data::map& request,lyramilk::data::map* response)
	{
		lyramilk::script::engines::ptr eng = es->get();
		if(eng == nullptr){
			lyramilk::klog(lyramilk::log::warning,"cvmap_script_selector." "call") << D("加载[%s]类型文件%s失败",enginetype.c_str(),scriptpath.c_str()) << std::endl;
			return cs_error;
		}
		if(!eng->load_file(scriptpath)){
			eng = nullptr;
			lyramilk::klog(lyramilk::log::warning,"cvmap_script_selector." "call") << D("加载[%s]类型文件%s失败",enginetype.c_str(),scriptpath.c_str()) << std::endl;
			return cs_error;
		}

		lyramilk::data::array ar;
		ar.push_back(request);
		lyramilk::data::var vret;
		if(eng->call("onrequest",ar,&vret)){
			if(vret.type_like(lyramilk::data::var::t_map)){
				*response = vret;
				return cs_ok;
			}
			return cs_ok;
		
		}
		lyramilk::klog(lyramilk::log::warning,"cvmap_script_selector." "call") << D("运行[%s]类型文件%s失败",enginetype.c_str(),scriptpath.c_str()) << std::endl;
		return cs_error;
	}


	class cvmap_selector_loger
	{
		lyramilk::data::string c;
		lyramilk::data::int64 v;
		lyramilk::data::map* response;
		lyramilk::debug::nsecdiff td;
		lyramilk::netio::netaddress addr;
	  public:
		cvmap_selector_loger(const lyramilk::netio::netaddress& source,const lyramilk::data::string& c,lyramilk::data::int64 v,lyramilk::data::map* response)
		{
			td.mark();
			this->c = c;
			this->v = v;
			this->response = response;
			this->addr = source;
		}

		~cvmap_selector_loger()
		{
			lyramilk::data::map& mrep = *response;

			lyramilk::data::int64 e = mrep["e"].conv(0);
			lyramilk::data::string m = mrep["m"];
			lyramilk::klog(lyramilk::log::trace,"cvmap_selector_loger") << D("%lld %s %s:%u c=%s (v=%lld) 耗时%.3f(毫秒)",e,m.c_str(),addr.ip_str().c_str(),addr.port(),this->c.c_str(),v,double(td.diff()) / 1000000) << std::endl;
		}
	};




	// cvmap_dispatcher
	cvmap_dispatcher::cvmap_dispatcher()
	{
	}

	cvmap_dispatcher::~cvmap_dispatcher()
	{
	}


	dispatcher_check_status cvmap_dispatcher::call(const lyramilk::netio::netaddress& source,const lyramilk::data::string& c,lyramilk::data::int64 v,const lyramilk::data::map& request,lyramilk::data::map* response)
	{
		cvmap_selector_loger _(source,c,v,response);

		std::list<lyramilk::ptr<cvmap_selector> >::iterator it = selectors.begin();
		for(;it!=selectors.end();++it){
			lyramilk::ptr<cvmap_selector>& ptr = *it;
			dispatcher_check_status cs = ptr->hittest(c,v,request,response);
			if(cs != cs_pass){
				return cs;
			}
		}

		lyramilk::klog(lyramilk::log::debug,"cvmap_dispatcher." "call") << D("加载[c=%s,v=%lld]失败",c.c_str(),v) << std::endl;
		return cs_pass;
	}


	bool cvmap_dispatcher::add(lyramilk::ptr<cvmap_selector> selector)
	{
		selectors.push_back(selector);
		return true;
	}


}}