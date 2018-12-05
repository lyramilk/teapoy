#include "util.h"
#include "stringutil.h"
#include <sys/stat.h>
#include <errno.h>

namespace lyramilk{ namespace teapoy{
	bool mkdir_p(const lyramilk::data::string& path)
	{
		struct stat st = {0};
		if(0 ==::stat(path.c_str(),&st)){
			return (st.st_mode&S_IFDIR) != 0;
		}
		if(errno != ENOENT){
			return false;
		}


		int create_dirs = 0;
		lyramilk::data::string result;
		lyramilk::data::strings dest = path_toarray(path);
		lyramilk::data::strings::const_iterator it = dest.begin();
		for(;it!=dest.end();++it){
			result.push_back('/');
			result.append(*it);

			//S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH
			//S_IRUSR | S_IWUSR
			if(0 != ::mkdir(result.c_str(),S_IRUSR | S_IWUSR | S_IXUSR)){
				if(errno == EEXIST) continue;
				return false;
			}
			++create_dirs;
		}
		return create_dirs > 0;
	}


	lyramilk::data::strings path_toarray(const lyramilk::data::string& filename)
	{
		if(filename[0] != '/'){
			return lyramilk::data::strings();
		}
		lyramilk::data::strings tmp = lyramilk::teapoy::split(filename,"/");
		lyramilk::data::strings dest;
		if(tmp.empty()) return dest;
		dest.reserve(tmp.size());
		lyramilk::data::strings::const_iterator it = tmp.begin();
		for(;it!=tmp.end();++it){
			if(it->empty()) continue;
			if(it->at(0) == '.'){
				std::size_t s = it->size();
				if(s == 2 && it->at(1) == '.'){
					dest.pop_back();
				}else if(s == 1){
					continue;
				}
			}
			dest.push_back(*it);
		}
		return dest;
	}


	lyramilk::data::string path_simplify(const lyramilk::data::string& filename)
	{
		if(filename.find("/.") == filename.npos){
			return filename;
		}

		lyramilk::data::string result;
		result.reserve(256);
		lyramilk::data::strings dest = path_toarray(filename);
		lyramilk::data::strings::const_iterator it = dest.begin();
		for(;it!=dest.end();++it){
			result.push_back('/');
			result.append(*it);
		}
		return result;
	}

}}
