#include "webdav.h"
#include "webservice.h"
#include <libmilk/var.h>
#include <libmilk/codes.h>
#include <libmilk/xml.h>

#include <errno.h>
#include <string.h>
#include <utime.h>


namespace lyramilk{ namespace webdav{

	//	file
	file::file()
	{
	}

	file::~file()
	{
	}

	lyramilk::data::string file::etag()
	{
		char buff_etag[100];
		long long unsigned filemtime = lastmodified();
		long long unsigned filesize = length();
		sprintf(buff_etag,"%llx-%llx",filemtime,filesize);
		return buff_etag;
	}

	int file::prop_patch(const lyramilk::data::string& xmlstr)
	{
		lyramilk::data::map x;
		lyramilk::data::xml::parse(xmlstr,&x);


		lyramilk::data::map o;
		lyramilk::data::xml::format_ns(x,&o);


		if(o["xml.tag"] == "propertyupdate" && o["xml.body"].type() == lyramilk::data::var::t_array){
			lyramilk::data::array& ar = o["xml.body"];

			lyramilk::data::array::iterator it = ar.begin();
			for(;it!=ar.end();++it){
				if(it->type() != lyramilk::data::var::t_map) continue;
				lyramilk::data::map& m = *it;
				if(m["xml.tag"] == "set" && m["xml.body"].type() == lyramilk::data::var::t_array){
					lyramilk::data::array& ar = m["xml.body"];
					lyramilk::data::array::iterator it = ar.begin();
					for(;it!=ar.end();++it){
						if(it->type() != lyramilk::data::var::t_map) continue;
						lyramilk::data::map& m = *it;
						if(m["xml.tag"] == "prop" && m["xml.body"].type() == lyramilk::data::var::t_array){
							lyramilk::data::array& ar = m["xml.body"];
							lyramilk::data::array::iterator it = ar.begin();
							for(;it!=ar.end();++it){
								lyramilk::data::map& m = *it;
								if(m["xml.tag"] == "getlastmodified" && m["xml.body"].type() == lyramilk::data::var::t_array){
									lyramilk::data::array& ar = m["xml.body"];
									if(ar.size() == 1 && ar[0].type_like(lyramilk::data::var::t_str)){
										lyramilk::data::string v = ar[0].str();
										struct tm tm;
										strptime(v.c_str(),"%a, %d %b %Y %H:%M:%S %Z", &tm);
										time_t modtime = timegm(&tm);
										int code = lastmodified(modtime);
										if(code != 200) return code;
									}

								}
							}
						}
					}
				}
			}
		}

		return 200;
	}

	//	filelist
	void filelist::append(lyramilk::ptr<lyramilk::webdav::file>& f)
	{
		files.push_back(f);
	}

	void filelist::insert(std::size_t idx,lyramilk::ptr<lyramilk::webdav::file>& f)
	{
		files[idx] = f;
	}

	std::size_t filelist::count()
	{
		return files.size();
	}

	lyramilk::ptr<lyramilk::webdav::file> filelist::at(std::size_t idx)
	{
		return files[idx];
	}

	void filelist::remove(std::size_t idx)
	{
		files.erase(files.begin() + idx);
	}

	void filelist::clear()
	{
		files.clear();
	}

	lyramilk::data::string filelist::to_xml()
	{
		lyramilk::data::map xr;
		xr["xml.tag"] = "multistatus";
		xr["xmlns"] = "DAV:";
		xr["xml.body"].type(lyramilk::data::var::t_array);
		lyramilk::data::array& xb = xr["xml.body"];

		std::vector<lyramilk::ptr<lyramilk::webdav::file> >::iterator it = files.begin();
		for(;it!=files.end();++it){
			lyramilk::ptr<lyramilk::webdav::file>& p = *it;
			lyramilk::data::map file;
			file["xml.tag"] = "response";
			file["xml.body"].type(lyramilk::data::var::t_array);
			lyramilk::data::array& fb = file["xml.body"];
			fb.resize(2);

			fb[0].type(lyramilk::data::var::t_map);
			{
				lyramilk::data::map& href = fb[0];
				href["xml.tag"] = "href";
				href["xml.body"] = p->href();//coder->encode(url + "/" + filename);
			}

			fb[1].type(lyramilk::data::var::t_map);

			lyramilk::data::map& propstat = fb[1];
			propstat["xml.tag"] = "propstat";
			propstat["xml.body"].type(lyramilk::data::var::t_array);
			lyramilk::data::array& propstatbody = propstat["xml.body"];


			if(p->code() == 200){
				lyramilk::data::map xstatus;
				xstatus["xml.body"] = "HTTP/1.1 200 OK";
				xstatus["xml.tag"] = "status";
				propstatbody.push_back(xstatus);

				lyramilk::data::map prop;
				prop["xml.tag"] = "prop";
				prop["xml.body"].type(lyramilk::data::var::t_array);
				lyramilk::data::array& propbody = prop["xml.body"];

				{
					lyramilk::data::map displayname;
					displayname["xml.tag"] = "displayname";
					displayname["xml.body"] = p->displayname();
					propbody.push_back(displayname);
				}



				/*{
					lyramilk::data::map tmp;
					tmp["xml.tag"] = "creationdate";
					propbody.push_back(tmp);
				}*/

				/*{
					lyramilk::data::map tmp;
					tmp["xml.tag"] = "R:bigbox";
					propbody.push_back(tmp);
				}*/

				{
					lyramilk::data::map tmp;
					tmp["xml.tag"] = "getcontentlength";
					tmp["xml.body"] = lyramilk::data::str(p->length());
					propbody.push_back(tmp);
				}



				/*{
					lyramilk::data::map tmp;
					tmp["xml.tag"] = "getcontenttype";
					propbody.push_back(tmp);
				}*/



				{
					lyramilk::data::map tmp;
					tmp["xml.tag"] = "getetag";
					tmp["xml.body"] = lyramilk::data::str(p->etag());
					propbody.push_back(tmp);
				}



				{
					lyramilk::data::map tmp;
					tmp["xml.tag"] = "owner";
					tmp["xml.body"] = lyramilk::data::str(p->owner());
					propbody.push_back(tmp);
				}


				{
					lyramilk::data::map tmp;
					tmp["xml.tag"] = "getlastmodified";

					time_t modtime = p->lastmodified();
					tm t;
					gmtime_r(&modtime,&t);
					char gmttimebuff[256];
					strftime(gmttimebuff, sizeof(gmttimebuff), "%a, %d %b %Y %H:%M:%S %Z", &t);
					tmp["xml.body"] = gmttimebuff;
					propbody.push_back(tmp);
				}


				{
					lyramilk::data::map resourcetype;
					resourcetype["xml.tag"] = "resourcetype";
					resourcetype["xml.body"].type(lyramilk::data::var::t_array);
					lyramilk::data::array& resourcetypebody = resourcetype["xml.body"];

					if(p->isdir()){
						lyramilk::data::map resourcetypeitem;
						resourcetypeitem["xml.tag"] = "collection";
						resourcetypebody.push_back(resourcetypeitem);
					}

					propbody.push_back(resourcetype);
				}

				{
					lyramilk::data::map tmp;
					tmp["xml.tag"] = "privilege";
					tmp["xml.body"].type(lyramilk::data::var::t_array);
					lyramilk::data::array& supported_privilege_set_bodys = tmp["xml.body"];

					{
						lyramilk::data::map tmp;
						tmp["xml.tag"] = "all";
						tmp["xml.body"].type(lyramilk::data::var::t_array);
						supported_privilege_set_bodys.push_back(tmp);
					}

					propbody.push_back(tmp);
				}


				{
					lyramilk::data::map tmp;
					tmp["xml.tag"] = "supportedlock";
					propbody.push_back(tmp);
				}

				propstatbody.push_back(prop);
			}else{
				lyramilk::data::string httpcodedesc = "HTTP/1.1 ";
				httpcodedesc += lyramilk::teapoy::get_error_code_desc(p->code());
				lyramilk::data::map xstatus;
				xstatus["xml.body"] = httpcodedesc;
				xstatus["xml.tag"] = "status";
				propstatbody.push_back(xstatus);
			
			}


			xb.push_back(file);
		}
		lyramilk::data::string xmlstr = "<?xml version=\"1.0\" encoding=\"utf-8\" ?>" + lyramilk::data::xml::stringify(xr);
		return xmlstr;
	}



	//	fileemulator
	fileemulator::fileemulator(const lyramilk::data::string& url,const lyramilk::data::string& real,const lyramilk::data::string& filename)
	{
		this->url = url;
		this->real = real;
		this->filename = filename;

		if(0 != ::stat(real.c_str(),&st)){
			if(errno == ENOENT){
				retcode = 404;
			}else if(errno == EACCES){
				retcode = 403;
			}else{
				retcode = 500;
			}
		}else{
			retcode = 200;
		}
	}

	fileemulator::~fileemulator()
	{
	}

	int fileemulator::code()
	{
		return retcode;
	}

	bool fileemulator::isdir()
	{
		if(code() != 200) return false;
		return (st.st_mode&S_IFDIR) != 0;
	}


	lyramilk::data::string fileemulator::href()
	{
		if(code() != 200) return "";
		return url;
	}

	lyramilk::data::string fileemulator::owner()
	{
		if(code() != 200) return "";
		return "anonymous";
	}

	lyramilk::data::uint64 fileemulator::length()
	{
		if(code() != 200) return 0;
		return st.st_size;
	}

	int fileemulator::lastmodified(lyramilk::data::uint64 g)
	{
		if(code() != 200) return code();
		struct utimbuf tbuf;
		tbuf.actime = st.st_atime;
		tbuf.modtime = g;

		int r = utime(real.c_str(), &tbuf);
		if(r == 0) return 200;
		if(errno == ENOENT){
			return 404;
		}else if(errno == EACCES){
			return 403;
		}else{
			return 500;
		}
	}

	lyramilk::data::uint64 fileemulator::lastmodified()
	{
		if(code() != 200) return 0;
		return st.st_mtime;
	}

	lyramilk::data::string fileemulator::displayname()
	{
		if(code() != 200) return "";
		return filename;
	}

	bool fileemulator::lockstatus()
	{
		return false;
	}

	bool fileemulator::trylock()
	{
		return false;
	}

	bool fileemulator::unlock()
	{
		return false;
	}


}}
