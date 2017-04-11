#include "stringutil.h"
#include <algorithm>

namespace lyramilk{ namespace teapoy{
	lyramilk::data::strings split(lyramilk::data::string data,lyramilk::data::string sep)
	{
		lyramilk::data::strings lines;
		lines.reserve(10);
		std::size_t posb = 0;
		do{
			std::size_t poscrlf = data.find(sep,posb);
			if(poscrlf == data.npos){
				lines.push_back(data.substr(posb));
				posb = poscrlf;
			}else{
				lines.push_back(data.substr(posb,poscrlf - posb));
				posb = poscrlf + sep.size();
			}
		}while(posb != data.npos);
		return lines;
	}

	lyramilk::data::strings pathof(lyramilk::data::string path)
	{
		lyramilk::data::strings ret;
		lyramilk::data::strings v = split(path,"/");
		lyramilk::data::strings::iterator it = v.begin();
		if(it==v.end()) return ret;
		ret.reserve(10);
		ret.push_back(*it);
		for(++it;it!=v.end();++it){
			if(*it == ".") continue;
			if(*it == ".." && !ret.empty()){
				ret.pop_back();
				continue;
			}
			ret.push_back(*it);
		}
		return ret;
	}

	lyramilk::data::string trim(lyramilk::data::string data,lyramilk::data::string pattern)
	{
		if (data.empty()) return data;
		std::size_t pos1 = data.find_first_not_of(pattern);
		std::size_t pos2 = data.find_last_not_of(pattern);
		if(pos2 == data.npos){
			return "";
		}
		pos2++;
		std::size_t des = pos2-pos1;
		if(des == data.size()) return data;
		return data.substr(pos1,des);
	}

	lyramilk::data::string lowercase(lyramilk::data::string src)
	{
		std::transform(src.begin(), src.end(), src.begin(), tolower);
		return src;
	}
}}
