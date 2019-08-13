//#define __STDC_LIMIT_MACROS
#include "config.h"
#include "js_extend.h"
#include <jsapi.h>
#include <jsfriendapi.h>
#ifdef Z_HAVE_TINYXML2
	#include <tinyxml2.h>
#endif
#include <math.h>

#include <sys/stat.h>
#include <fstream>
#include <libmilk/codes.h>
#include <libmilk/log.h>
#include <libmilk/dict.h>

namespace lyramilk{ namespace teapoy{ namespace sni{
	static JSClass normalClass = { "normal", 0,
			JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
            JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub};

#ifdef Z_HAVE_TINYXML2
	static JSBool js_xml_stringify(JSContext *cx, unsigned argc, js::Value *vp);
	static JSBool js_xml_parse(JSContext *cx, unsigned argc, js::Value *vp);

	static JSFunctionSpec js_xml_static_funs[] = {
		JS_FN("stringify",	js_xml_stringify,	1, 0),
		JS_FN("parse",		js_xml_parse,		1, 0),
		JS_FS_END
	};

	static void js2xml(JSContext* cx,JSObject *jo,tinyxml2::XMLDocument* doc,tinyxml2::XMLElement* x)
	{
		JSObject *iter = JS_NewPropertyIterator(cx,jo);
		if(iter){
			jsid jid;
			while(JS_NextProperty(cx,iter,&jid)){
				jsval jk,jv;
				JS_IdToValue(cx,jid,&jk);
				if(!jk.isString()) break;
				JSString* jstr = jk.toString();
				if(jstr == nullptr) break;
				char* skey = JS_EncodeString(cx,jstr);
				JS_LookupPropertyById(cx,jo,jid,&jv);
				if(strncmp(skey,"xml.tag",7) == 0) continue;

				if(jv.isNullOrUndefined()){
				}else if(jv.isInt32()){
					int v = jv.toInt32();

					char svalue[512];
					snprintf(svalue,sizeof(svalue),"%d",v);

					x->SetAttribute(skey,svalue);
				}else if(jv.isDouble()){
					double v = jv.toDouble();

					char svalue[512];
					snprintf(svalue,sizeof(svalue),"%f",v);

					x->SetAttribute(skey,svalue);
					return;
				}else if(jv.isNumber()){
					double v = jv.toNumber();

					char svalue[512];
					snprintf(svalue,sizeof(svalue),"%f",v);

					x->SetAttribute(skey,svalue);
					return;
				}else if(jv.isBoolean()){
					bool v = jv.toBoolean();
					if(v){
						x->SetAttribute(skey,"true");
					}else{
						x->SetAttribute(skey,"false");
					}
				}else if(jv.isString()){
					JSString* jstr = jv.toString();
					if(jstr){
						char * svalue = JS_EncodeString(cx,jstr);
						x->SetAttribute(skey,svalue);
						JS_free(cx,svalue);
					}
				}else if(jv.isObject()){
					JSObject *jo = jv.toObjectOrNull();
					if(strncmp("xml.body",skey,8) == 0 && JS_IsArrayObject(cx,jo)){
						uint32_t len = 0;
						if(JS_GetArrayLength(cx,jo,&len)){
							for(uint32_t i=0;i<len;++i){
								jsval jv;
								JS_LookupElement(cx,jo,i,&jv);

								if(jv.isNullOrUndefined()){
								}else if(jv.isInt32()){
									int v = jv.toInt32();

									char svalue[512];
									snprintf(svalue,sizeof(svalue),"%d",v);

									x->LinkEndChild(doc->NewText(svalue));
								}else if(jv.isDouble()){
									double v = jv.toDouble();

									char svalue[512];
									snprintf(svalue,sizeof(svalue),"%f",v);

									x->LinkEndChild(doc->NewText(svalue));
								}else if(jv.isNumber()){
									double v = jv.toNumber();

									char svalue[512];
									snprintf(svalue,sizeof(svalue),"%f",v);

									x->LinkEndChild(doc->NewText(svalue));
								}else if(jv.isBoolean()){
									bool v = jv.toBoolean();
									if(v){
										x->LinkEndChild(doc->NewText("true"));
									}else{
										x->LinkEndChild(doc->NewText("false"));
									}
								}else if(jv.isString()){
									JSString* jstr = jv.toString();
									if(jstr){
										char * svalue = JS_EncodeString(cx,jstr);
										x->LinkEndChild(doc->NewText(svalue));
										JS_free(cx,svalue);
									}
								}else if(jv.isObject()){
									JSObject *sub_jo = jv.toObjectOrNull();
									jsval sub_label;
									JS_GetProperty(cx,sub_jo,"xml.tag",&sub_label);
									if(sub_label.isString()){
										JSString* jstr = sub_label.toString();
										if(jstr){
											char * sub_tagname = JS_EncodeString(cx,jstr);
											tinyxml2::XMLElement* subx = doc->NewElement(sub_tagname);
											JS_free(cx,sub_tagname);

											x->LinkEndChild(subx);
											js2xml(cx,sub_jo,doc,subx);
										}
									}
								}
							}
						}
					}
				}
				JS_free(cx,skey);
			}
		}
	}

	void static xml2js(JSContext* cx,tinyxml2::XMLDocument* doc,const tinyxml2::XMLNode* x,JSObject *jo)
	{
		for(const tinyxml2::XMLAttribute* ax = x->ToElement()->FirstAttribute();ax;ax = ax->Next()){
			JSString* jsstr = JS_NewStringCopyZ(cx,ax->Value());
			jsval jv;
			jv.setString(jsstr);
			JS_SetProperty(cx,jo,ax->Name(),&jv);
		}

		
		std::vector<jsval> jvs;

		const tinyxml2::XMLNode* node = x->FirstChild();
		for(;node;node = node->NextSibling()){
			const tinyxml2::XMLElement* ele = node->ToElement();
			if(ele){
				JSObject *sub_jo = JS_NewObject(cx,&normalClass,nullptr,JS_GetGlobalObject(cx));

				jsval sub_jv;
				sub_jv.setObjectOrNull(sub_jo);
				jsval sub_jv_label;
				sub_jv_label.setString(JS_NewStringCopyZ(cx,node->Value()));

				JS_SetProperty(cx,sub_jo,"xml.tag",&sub_jv_label);
				xml2js(cx,doc,ele,sub_jo);
				jvs.push_back(sub_jv);
				continue;
			}
			
			const tinyxml2::XMLText* txt = node->ToText();
			if(txt){
				JSString* jsstr = JS_NewStringCopyZ(cx,node->Value());
				jsval sub_jv;
				sub_jv.setString(jsstr);
				jvs.push_back(sub_jv);
			}
		}


		if(!jvs.empty()){
			jsval jk;
			JSObject* jko = JS_NewArrayObject(cx,jvs.size(),jvs.data());
			jk.setObjectOrNull(jko);
			JS_SetProperty(cx,jo,"xml.body",&jk);
		}
	}

	JSBool js_xml_stringify(JSContext *cx, unsigned argc, js::Value *vp)
	{
		tinyxml2::XMLDocument xml_doc;
		tinyxml2::XMLDocument *doc = &xml_doc;

		JSObject *sub_jo = vp[2].toObjectOrNull();
		jsval sub_label;
		JS_GetProperty(cx,sub_jo,"xml.tag",&sub_label);
		if(sub_label.isString()){
			JSString* jstr = sub_label.toString();
			if(jstr){
				char * sub_tagname = JS_EncodeString(cx,jstr);
				tinyxml2::XMLElement* subx = doc->NewElement(sub_tagname);
				JS_free(cx,sub_tagname);

				doc->LinkEndChild(subx);
				js2xml(cx,sub_jo,doc,subx);
			}
		}
		tinyxml2::XMLPrinter printer(NULL,true);
		xml_doc.Print(&printer);

		int sz = printer.CStrSize();
		if(sz > 0){
			vp->setString(JS_NewStringCopyN(cx,printer.CStr(),sz - 1));
		}
		return JS_TRUE;
	}

	JSBool js_xml_parse(JSContext *cx, unsigned argc, js::Value *vp)
	{
		tinyxml2::XMLDocument xml_doc;
		tinyxml2::XMLDocument *doc = &xml_doc;
		JSString* jsstr = vp[2].toString();
		if(jsstr){
			char * xmlstring = JS_EncodeString(cx,jsstr);
			if(tinyxml2::XML_SUCCESS != xml_doc.Parse(xmlstring,strlen(xmlstring))){
				JS_free(cx,xmlstring);
				vp->setUndefined();
				return JS_TRUE;
			}
			JS_free(cx,xmlstring);


			tinyxml2::XMLNode* root = xml_doc.RootElement();
			if(root == nullptr){
				vp->setUndefined();
				return JS_TRUE;
			}
			JSObject *jo = JS_NewObject(cx,&normalClass,nullptr,JS_GetGlobalObject(cx));


			jsval sub_jv_label;
			sub_jv_label.setString(JS_NewStringCopyZ(cx,root->Value()));

			JS_SetProperty(cx,jo,"xml.tag",&sub_jv_label);

			xml2js(cx,doc,root,jo);

			vp->setObjectOrNull(jo);
			return JS_TRUE;
		}
		vp->setUndefined();
		return JS_TRUE;
	}
#endif
	js_extend::js_extend()
	{
	}

	js_extend::~js_extend()
	{
	}

	void js_extend::inite()
	{
		JS_SetRuntimeThread(rt);
		JSContext* selectedcx = cx_template;
		JSObject* global = JS_GetGlobalObject(selectedcx);
#ifdef Z_HAVE_TINYXML2
		JSObject* jo = JS_DefineObject(selectedcx,global,"XML",&normalClass,NULL,0);
		JS_DefineFunctions(selectedcx,jo,js_xml_static_funs);
#endif
	}

	static lyramilk::script::engine* __ctr()
	{
		js_extend* p = new js_extend();
		p->inite();
		return p;
	}

	static void __dtr(lyramilk::script::engine* eng)
	{
		delete (js_extend*)eng;
	}


	void js_extend::engine_load(const char* scripttypename)
	{
		lyramilk::script::engine::undef(scripttypename);
		lyramilk::script::engine::define(scripttypename,__ctr,__dtr);
	}





	static lyramilk::script::engine* _jshtml_ctr()
	{
		jshtml* p = new jshtml();
		p->inite();
		return p;
	}

	static void _jshtml_dtr(lyramilk::script::engine* eng)
	{
		delete (jshtml*)eng;
	}


	void inline jsstr2str(const jschar* cstr,size_t len,lyramilk::data::string* strg)
	{
		lyramilk::data::string& str = *strg;
		const jschar* streof = &cstr[len];
		str.reserve(len*3);
		for(;cstr < streof;){
			jschar jwc = *cstr++;
			unsigned wchar_t wc = jwc;
			if(jwc >= 0xd800 && jwc <= 0xdfff && cstr<streof){
				jschar jwc2 = *cstr++;
				wc = (jwc2&0x03ff) + (((jwc&0x03ff) + 0x40) << 10);
			}
			if(wc < 0x80){
				str.push_back((unsigned char)wc);
			}else if(wc < 0x800){
				str.push_back((unsigned char)((wc>>6)&0x1f) | 0xc0);
				str.push_back((unsigned char)((wc>>0)&0x3f) | 0x80);
			}else if(wc < 0x10000){
				str.push_back((unsigned char)((wc>>12)&0xf) | 0xe0);
				str.push_back((unsigned char)((wc>>6)&0x3f) | 0x80);
				str.push_back((unsigned char)((wc>>0)&0x3f) | 0x80);
			}else if(wc < 0x200000){
				str.push_back((unsigned char)((wc>>18)&0x7) | 0xf0);
				str.push_back((unsigned char)((wc>>12)&0x3f) | 0x80);
				str.push_back((unsigned char)((wc>>6)&0x3f) | 0x80);
				str.push_back((unsigned char)((wc>>0)&0x3f) | 0x80);
			}else if(wc < 0x4000000){
				str.push_back((unsigned char)((wc>>24)&0x3) | 0xf8);
				str.push_back((unsigned char)((wc>>18)&0x3f) | 0x80);
				str.push_back((unsigned char)((wc>>12)&0x3f) | 0x80);
				str.push_back((unsigned char)((wc>>6)&0x3f) | 0x80);
				str.push_back((unsigned char)((wc>>0)&0x3f) | 0x80);
			}else if(wc < 0x80000000){
				str.push_back((unsigned char)((wc>>30)&0x1) | 0xfc);
				str.push_back((unsigned char)((wc>>24)&0x3f) | 0xf0);
				str.push_back((unsigned char)((wc>>18)&0x3f) | 0x80);
				str.push_back((unsigned char)((wc>>12)&0x3f) | 0x80);
				str.push_back((unsigned char)((wc>>6)&0x3f) | 0x80);
				str.push_back((unsigned char)((wc>>0)&0x3f) | 0x80);
			}
		}
	}
	bool j2s(JSContext* cx,js::Value jv,lyramilk::data::string* retv)
	{
		if(jv.isString()){
			JSString* jstr = jv.toString();
			if(jstr == nullptr){
				*retv = "";
				return false;
			}
			size_t len = 0;
			const jschar* cstr = JS_GetStringCharsZAndLength(cx,jstr,&len);
			jsstr2str(cstr,len,retv);
			return true;
		}
		retv->clear();
		return false;
	}


	static void script_js_error_report(JSContext *cx, const char *message, JSErrorReport *report)
	{
		js::Value jve;

		//jshtml* p = JS_GetContextPrivate(selectedcx);

		if(JS_IsExceptionPending(cx) && JS_GetPendingException(cx,&jve)){
			JSObject *jo = jve.toObjectOrNull();
			if(jo){
				js::Value __prop;
				if(JS_GetProperty(cx,jo,"stack",&__prop)){

					std::vector<int>* prlines = (std::vector<int>*)JS_GetContextPrivate(cx);
					int lineno = report->lineno;
					if(prlines && lineno < prlines->size() + 1){
						lineno = prlines->at(lineno - 1);
					}

					lyramilk::data::string retv;
					j2s(cx,__prop,&retv);
					lyramilk::klog(lyramilk::log::warning,"lyramilk.script.jshtml") << lyramilk::kdict("%s(%d:%d)%s",(report->filename ? report->filename : "<no name>"),lineno,report->column,message) << "\n" << retv << std::endl;
					return;
				}
			}
		}

		lyramilk::klog(lyramilk::log::warning,"lyramilk.script.jshtml") << lyramilk::kdict("%s(%d:%d)%s",(report->filename ? report->filename : "<no name>"),report->lineno,report->column,message) << std::endl;
	}



	jshtml::jshtml()
	{
	}

	jshtml::~jshtml()
	{
	}

	void jshtml::engine_load(const char* scripttypename)
	{
		lyramilk::script::engine::define(scripttypename,_jshtml_ctr,_jshtml_dtr);
	}


	bool jshtml::load_string(const lyramilk::data::string& cscriptstring)
	{
		JS_SetRuntimeThread(rt);
		init();
		JSContext* selectedcx = (JSContext *)JS_GetRuntimePrivate(rt);
		JSObject* global = JS_GetGlobalObject(selectedcx);

		std::vector<int> tmprlines;
		lyramilk::data::string scriptstring = code_convert(cscriptstring,&tmprlines);
		JSScript* script = JS_CompileScript(selectedcx,global,scriptstring.c_str(),scriptstring.size(),nullptr,1);
		if(script){
			return !!JS_ExecuteScript(selectedcx,global,script,nullptr);
		}
		return false;
	}


		struct bytecode
		{
			time_t tm;
			std::string code;
			std::vector<int> rlines;
		};



	bool jshtml::load_file(const lyramilk::data::string& scriptfile)
	{
		if(!scriptfilename.empty()){
			return false;
		}

		JS_SetRuntimeThread(rt);
		init();
		JSContext* selectedcx = (JSContext *)JS_GetRuntimePrivate(rt);
		JS_SetErrorReporter(selectedcx, script_js_error_report);


		JSObject* global = JS_GetGlobalObject(selectedcx);

		/*
		JSScript* script = nullptr;
		script = JS_CompileUTF8File(selectedcx,global,scriptfile.c_str());
		if(!script)return false;
		return !!JS_ExecuteScript(selectedcx,global,script,nullptr);
		*/
		static std::map<lyramilk::data::string,bytecode> bytecodemap;
		static lyramilk::threading::mutex_rw bytecodelock;
		JSScript* script = nullptr;

		struct stat st = {0};
		if(0 !=::stat(scriptfile.c_str(),&st)){
			return false;
		}
		{
			//尝试读取
			lyramilk::threading::mutex_sync _(bytecodelock.r());
			std::map<lyramilk::data::string,bytecode>::const_iterator it = bytecodemap.find(scriptfile);
			if(it!=bytecodemap.end()){
				const bytecode& c = it->second;
				if(st.st_mtime == c.tm){
					JS_SetContextPrivate(selectedcx,(void*)&c.rlines);
					script = JS_DecodeScript(selectedcx,(const void*)c.code.c_str(),c.code.size(),nullptr,nullptr);
				}
			}
		}
		if(script){
			return !!JS_ExecuteScript(selectedcx,global,script,nullptr);
		}else{
			JS::CompileOptions options(selectedcx);
			//options.setSourcePolicy(JS::CompileOptions::NO_SOURCE);

			options.filename = scriptfile.c_str();
			JS::RootedObject g(selectedcx,global);
			//script = JS::Compile(selectedcx,g,options,scriptfile.c_str());


			lyramilk::data::string scriptstring;

			std::ifstream ifs(scriptfile.c_str(),std::ifstream::binary|std::ifstream::in);
			while(ifs){
				char buff[4096];
				ifs.read(buff,sizeof(buff));
				scriptstring.append(buff,ifs.gcount());
			}
			ifs.close();
			bytecode c;

			scriptstring = code_convert(scriptstring,&c.rlines);
			script = JS::Compile(selectedcx,g,options,scriptstring.c_str(),scriptstring.size());

			if(!script)return false;


			JS_SetContextPrivate(selectedcx,&c.rlines);

			if(JS_ExecuteScript(selectedcx,global,script,nullptr)){
				uint32_t len = 0;
				void* p = JS_EncodeScript(selectedcx,script,&len);
				if(p && len){
					c.tm = st.st_mtime;
					c.code.assign((const char*)p,len);
					{
						lyramilk::threading::mutex_sync _(bytecodelock.w());
						bytecodemap[scriptfile] = c;
					}
				}
				JS_SetContextPrivate(selectedcx,&bytecodemap[scriptfile].rlines);

				return true;
			}
			return false;
		}
	}



	/*******************************************************************************/

	enum codestatus{
		cs_normal,
		cs_apos,
		cs_apos_t,
		cs_quot,
		cs_quot_t,
		cs_comment_preview,
		cs_line_comment,
		cs_block_comment,
		cs_block_comment_closing,
	};

	std::size_t code_find(const lyramilk::data::string& code,std::size_t skip,const lyramilk::data::string& pattern,codestatus* pcs)
	{
		if(code.size() <= skip) return code.npos;
		unsigned char PLACEHOLDER = 0;
		while(pattern.find(PLACEHOLDER) != pattern.npos && PLACEHOLDER < 0xff) {
			PLACEHOLDER++;
		}

		codestatus& cs = *pcs;

		lyramilk::data::string result;
		lyramilk::data::string::const_iterator it = code.begin() + skip;

		for(;it!=code.end();++it){
			const char& c = *it;
			switch(cs){
			  case cs_normal:
				if(c == '\''){
					cs = cs_apos;
					result.push_back(c);
				}else if(c == '"'){
					cs = cs_quot;
					result.push_back(c);
				}else if(c == '/'){
					cs = cs_comment_preview;
					result.push_back(c);
				}else{
					result.push_back(c);
					std::size_t r = result.find(pattern);
					if(r != result.npos){
						return r + skip;
					}
				}
			  	break;
			  case cs_apos:
				if(c ==  '\\'){
					cs = cs_apos_t;
					result.push_back(c);
				}else if(c ==  '\''){
					cs = cs_normal;
					result.push_back(c);
				}else{
					result.push_back(PLACEHOLDER);
				}
			  	break;
			  case cs_apos_t:
				result.push_back(PLACEHOLDER);
				cs = cs_apos;
			  	break;
			  case cs_quot:
				if(c ==  '\\'){
					cs = cs_quot_t;
					result.push_back(PLACEHOLDER);
				}else if(c ==  '\"'){
					cs = cs_normal;
					result.push_back(c);
				}else{
					result.push_back(PLACEHOLDER);
				}
			  	break;
			  case cs_quot_t:
				result.push_back(PLACEHOLDER);
				cs = cs_quot;
			  	break;
			  case cs_comment_preview:
				if(c ==  '*'){
					cs = cs_block_comment;
					result.push_back(c);
				}else if(c == '/'){
					cs = cs_line_comment;
					it = code.end() - 1;
				}else{
					result.push_back('/');
					result.push_back(c);
					cs = cs_normal;
				}
				break;
			  case cs_block_comment_closing:
				if(c ==  '/'){
					result.push_back('*');
					result.push_back('/');
					cs = cs_normal;
					break;
				}else{
					cs = cs_block_comment;
					result.push_back(PLACEHOLDER);
					//重新进入 cs_block_comment 状态
				}
			  case cs_block_comment:
				if(c ==  '*'){
					cs = cs_block_comment_closing;
				}else{
					result.push_back(PLACEHOLDER);
				}
			  	break;
			  case cs_line_comment:
				break;
			}
		}
		if(cs == cs_line_comment){
			cs = cs_normal;
		}

		return result.npos;
	}

	lyramilk::data::string html_2_str(const lyramilk::data::string& code)
	{
		lyramilk::data::string result;

		result.push_back('"');
		lyramilk::data::string::const_iterator it = code.begin();
		for(;it!=code.end();++it){
			const char& c = *it;
			if(c == '\\'){
				result.push_back('\\');
				result.push_back(c);
			}else if(c == '\"'){
				result.push_back('\\');
				result.push_back(c);
			}else if(c == '\n'){
				result.push_back('\\');
				result.push_back('n');
			}else{
				result.push_back(c);
			}
		}

		result.push_back('"');
		return result;
	}
	#define  LINEEOF "\n"

	lyramilk::data::string jshtml::code_convert(const lyramilk::data::string& code,std::vector<int>* prlines)
	{
		std::vector<int>& rlines = *prlines;
		rlines.clear();
		lyramilk::data::stringstream src(code);
		lyramilk::data::stringstream ofs;
		lyramilk::data::string line;


		enum {
			s_0,
			s_init,
			s_onrequest_html,
			s_onrequest_code,
		}s = s_0;

		int lineno =0;
		codestatus cs = cs_normal;
		while(getline(src,line)){
			std::size_t skip = 0;
			++lineno;
			if(s == s_0){
				if(line.compare(0,3,"<%@") != 0){
					ofs << "function onrequest(request,response){" << "//#" << lineno << LINEEOF;
					rlines.push_back(lineno);
					s = s_onrequest_html;
				}else{
					s = s_init;
					skip = 3;
				}
			}

			while(skip < line.size()){
				if(s == s_init){
					std::size_t pos = code_find(line,skip,"%>",&cs);
					if(pos == line.npos){
						ofs << line.substr(skip) << "//#" << lineno << LINEEOF;
						rlines.push_back(lineno);
						skip = line.size();
						break;
					}else{
						ofs << line.substr(skip,pos - skip) << "//#" << lineno << LINEEOF;
						rlines.push_back(lineno);
						skip = pos + 2;
						ofs << "function onrequest(request,response){" << "//#" << lineno << LINEEOF;
						rlines.push_back(lineno);
						s = s_onrequest_html;
					}
				}


				if(s == s_onrequest_html){
					std::size_t pos = line.find("<%",skip);
					if(pos == line.npos){
						ofs << "response.write(" << html_2_str(line.substr(skip)) << " + \"\\n\");	//#" << lineno << LINEEOF;
						rlines.push_back(lineno);
						skip = line.size();
						break;
					}
					ofs << "response.write(" << html_2_str(line.substr(skip,pos - skip)) << ");	//#" << lineno << LINEEOF;
					rlines.push_back(lineno);
					skip = pos + 2;
					s = s_onrequest_code;
				}

				if(s == s_onrequest_code){
					std::size_t pos = code_find(line,skip,"%>",&cs);
					if(pos == line.npos){
						ofs << line.substr(skip) << "//#" << lineno << LINEEOF;
						rlines.push_back(lineno);
						skip = line.size();
						break;
					}

					if(skip < line.size() && line.at(skip) == '='){
						ofs << "response.write(" << line.substr(skip + 1,pos - skip - 1) << ");	//#" << lineno << LINEEOF;
						rlines.push_back(lineno);
						skip = pos + 2;
						s = s_onrequest_html;
						continue;
					}

					ofs << line.substr(skip,pos - skip) << "//#" << lineno << LINEEOF;
					rlines.push_back(lineno);
					skip = pos + 2;

					if(skip == line.size()){
						ofs << "response.write(\"\\n\");	//#" << lineno << LINEEOF;
						rlines.push_back(lineno);
					}
					s = s_onrequest_html;
				}
			}
		}
		ofs << "return true;\n}\n";
		return ofs.str();
	}
	/*******************************************************************************/

}}}
