#include "stringutil.h"
#include <algorithm>

namespace lyramilk{ namespace teapoy{
	strings split(lyramilk::data::string data,lyramilk::data::string sep)
	{
		strings lines;
		std::size_t posb = 0;
		do{
			std::size_t poscrlf = data.find(sep,posb);
			if(poscrlf == data.npos){
				lyramilk::data::string newline = data.substr(posb);
				posb = poscrlf;
				lines.push_back(newline);
			}else{
				lyramilk::data::string newline = data.substr(posb,poscrlf - posb);
				posb = poscrlf + sep.size();
				lines.push_back(newline);
			}
		}while(posb != data.npos);
		return lines;
	}

	strings pathof(lyramilk::data::string path)
	{
		strings ret;
		strings v = split(path,"/");
		strings::iterator it = v.begin();
		if(it==v.end()) return ret;
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
		return data.substr(pos1,pos2-pos1);
	}

	lyramilk::data::string lowercase(lyramilk::data::string src)
	{
		std::transform(src.begin(), src.end(), src.begin(), tolower);
		return src;
	}
}}
