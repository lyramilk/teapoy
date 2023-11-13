#ifndef _lyramilk_teapoy_util_h_
#define _lyramilk_teapoy_util_h_

#include <libmilk/var.h>

namespace lyramilk{ namespace teapoy{
	/// 不支持相对路径,返回
	bool mkdir_p(const lyramilk::data::string& path);
	/// 不支持相对路径
	lyramilk::data::strings path_toarray(const lyramilk::data::string& filename);
	/// 不支持相对路径
	lyramilk::data::string path_simplify(const lyramilk::data::string& filename);
}}


#endif
