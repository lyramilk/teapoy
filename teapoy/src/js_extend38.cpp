//#define __STDC_LIMIT_MACROS
#include "config.h"
#include "js_extend.h"
#include <jsapi.h>
#include <jsfriendapi.h>
#ifdef TINYXML2_FOUND
	#include <tinyxml2.h>
#endif
#include <math.h>

#include <sys/stat.h>
#include <fstream>

#include <libmilk/codes.h>
#include <libmilk/log.h>
#include <libmilk/dict.h>

namespace lyramilk{ namespace teapoy{ namespace sni{
	static JSClass normalClass = { "normal", JSCLASS_HAS_PRIVATE,
			nullptr, nullptr, nullptr, nullptr,
			nullptr, nullptr, nullptr};

#ifdef TINYXML2_FOUND
	static bool js_xml_stringify(JSContext *cx, unsigned argc, JS::Value *vp);
	static bool js_xml_parse(JSContext *cx, unsigned argc, JS::Value *vp);




	static JSFunctionSpec js_xml_static_funs[] = {
		JS_FN("stringify",	js_xml_stringify,	1, 0),
		JS_FN("parse",		js_xml_parse,		1, 0),
		JS_FS_END
	};






		bool j2s(JSContext* cx,JS::Value jv,lyramilk::data::string* retv)
		{
			if(jv.isString()){
				JSString* jstr = jv.toString();
				if(jstr == nullptr){
					*retv = "";
					return false;
				}
				std::size_t len = JS_GetStringEncodingLength(cx,jstr);

				retv->resize(len);
				JS_EncodeStringToBuffer(cx,jstr,(char*)retv->data(),len);
				return true;
			}else if(jv.isSymbol()){
				JS::RootedSymbol sym(cx,jv.toSymbol());

				JSString* jstr = JS::GetSymbolDescription(sym);
				if(jstr == nullptr){
					*retv = "";
					return false;
				}
				std::size_t len = JS_GetStringEncodingLength(cx,jstr);

				retv->resize(len);
				JS_EncodeStringToBuffer(cx,jstr,(char*)retv->data(),len);
				return true;
			}
			retv->clear();
			return false;
		}



		void j2v(JSContext* cx,JS::HandleValue jv,lyramilk::data::var* retv)
		{
			JS::RootedValue value(cx,jv);
			
		}






	static void js2xml(JSContext* cx,JSObject *jo,tinyxml2::XMLDocument* doc,tinyxml2::XMLElement* x)
	{
		JS::RootedObject jsobj(cx,jo);

		JS::AutoIdArray ida(cx,JS_Enumerate(cx,jsobj));

		size_t c = ida.length();
		for(size_t i=0;i<c;++i){
			JS::RootedId idkey(cx,ida[i]);
			JS::RootedValue jskey(cx);
			JS::RootedValue jsval(cx);

			JS_IdToValue(cx,idkey,&jskey);
			JS_GetPropertyById(cx,jsobj,idkey,&jsval);
			lyramilk::data::string skey;
			j2s(cx,jskey,&skey);
			//j2v(cx,jsval,&m[key]);
			//x->SetAttribute(skey,svalue);
			if(skey.compare(0,7,"xml.tag") == 0) continue;

			JSType valuetype = JS_TypeOfValue(cx,jsval);
			switch(valuetype){
			  case JSTYPE_VOID:
				continue;
			  case JSTYPE_OBJECT:
				{
					JSObject *jso = jsval.toObjectOrNull();
					JS::RootedObject jo(cx,jso);
					if(JS_IsArrayObject(cx,jo) && skey.compare(0,8,"xml.body") == 0){
						uint32_t len = 0;
						if(!JS_GetArrayLength(cx,jo,&len)){
							continue;
						}
						for(uint32_t i=0;i<len;++i){
							JS::RootedValue jv(cx);
							JS_GetElement(cx,jo,i,&jv);


							JSType subvaluetype = JS_TypeOfValue(cx,jv);
							switch(subvaluetype){
							  case JSTYPE_STRING:
								{
									lyramilk::data::string tstr;
									j2s(cx,jv,&tstr);
									x->LinkEndChild(doc->NewText(tstr.c_str()));
								}
								break;
							  case JSTYPE_NUMBER:
								{
									if(jv.isInt32()){
										int v = jv.toInt32();
										char svalue[512];
										snprintf(svalue,sizeof(svalue),"%d",v);

										x->LinkEndChild(doc->NewText(svalue));
										continue;
									}else if(jv.isDouble()){
										double v = jv.toDouble();

										char svalue[512];
										snprintf(svalue,sizeof(svalue),"%f",v);

										x->LinkEndChild(doc->NewText(svalue));
										continue;
									}
								}
								break;
							  case JSTYPE_BOOLEAN:
								if(jv.toBoolean()){
									x->LinkEndChild(doc->NewText("true"));
								}else{
									x->LinkEndChild(doc->NewText("false"));
								}
								break;
							  default:
								break;
							}
						}
						continue;
					}
				}
				continue;
			  case JSTYPE_STRING:
				{
					lyramilk::data::string tstr;
					j2s(cx,jsval,&tstr);
					x->SetAttribute(skey.c_str(),tstr.c_str());
				}
				continue;
			  case JSTYPE_NUMBER:
				{
					if(jsval.isInt32()){
						int v = jsval.toInt32();
						char svalue[512];
						snprintf(svalue,sizeof(svalue),"%d",v);

						x->SetAttribute(skey.c_str(),svalue);
						continue;
					}else if(jsval.isDouble()){
						double v = jsval.toDouble();

						char svalue[512];
						snprintf(svalue,sizeof(svalue),"%f",v);

						x->SetAttribute(skey.c_str(),svalue);
						continue;
					}
				}
				continue;
			  case JSTYPE_BOOLEAN:
				if(jsval.toBoolean()){
					x->SetAttribute(skey.c_str(),"true");
				}else{
					x->SetAttribute(skey.c_str(),"false");
				}
				continue;
			  case JSTYPE_SYMBOL:
				{
					lyramilk::data::string tstr;
					j2s(cx,jsval,&tstr);
					x->SetAttribute(skey.c_str(),tstr.c_str());
			    }
				continue;
			  default:
				continue;
			}
		}
	}

	void static xml2js(JSContext* cx,tinyxml2::XMLDocument* doc,const tinyxml2::XMLNode* x,JSObject *jo)
	{
		JS::RootedObject jsobj(cx,jo);
		for(const tinyxml2::XMLAttribute* ax = x->ToElement()->FirstAttribute();ax;ax = ax->Next()){
			JSString* jsstr = JS_NewStringCopyZ(cx,ax->Value());
			JS::RootedValue jsval(cx);
			jsval.setString(jsstr);
			JS_SetProperty(cx,jsobj,ax->Name(),jsval);
		}

		
		std::vector<JS::Value> jvs;

		const tinyxml2::XMLNode* node = x->FirstChild();
		for(;node;node = node->NextSibling()){
			const tinyxml2::XMLElement* ele = node->ToElement();
			if(ele){
				JS::RootedObject sub_jo(cx,JS_NewObject(cx,&normalClass));

				JS::Value sub_jv;
				sub_jv.setObjectOrNull(sub_jo);
				JS::RootedValue sub_jv_label(cx);
				sub_jv_label.setString(JS_NewStringCopyZ(cx,node->Value()));

				JS_SetProperty(cx,sub_jo,"xml.tag",sub_jv_label);
				xml2js(cx,doc,ele,sub_jo);
				jvs.push_back(sub_jv);
				continue;
			}
			
			const tinyxml2::XMLText* txt = node->ToText();
			if(txt){
				JSString* jsstr = JS_NewStringCopyZ(cx,node->Value());
				JS::Value sub_jv;
				sub_jv.setString(jsstr);
				jvs.push_back(sub_jv);
			}
		}



		if(!jvs.empty()){
			JS::HandleValueArray jav = JS::HandleValueArray::fromMarkedLocation(jvs.size(),jvs.data());
			JSObject* jko = JS_NewArrayObject(cx,jav);

			JS::RootedValue jsval(cx);
			jsval.setObjectOrNull(jko);
			JS_SetProperty(cx,jsobj,"xml.body",jsval);
		}

	}

	static bool js_xml_stringify(JSContext *cx, unsigned argc, JS::Value *vp)
	{
		tinyxml2::XMLDocument xml_doc;
		tinyxml2::XMLDocument *doc = &xml_doc;

		JS::RootedObject sub_jo(cx,vp[2].toObjectOrNull());
		JS::RootedValue sub_label(cx);
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
		return true;
	}

	static bool js_xml_parse(JSContext *cx, unsigned argc, JS::Value *vp)
	{
		tinyxml2::XMLDocument xml_doc;
		tinyxml2::XMLDocument *doc = &xml_doc;
		JSString* jsstr = vp[2].toString();
		if(jsstr){
			char * xmlstring = JS_EncodeString(cx,jsstr);
			if(tinyxml2::XML_SUCCESS != xml_doc.Parse(xmlstring,strlen(xmlstring))){
				JS_free(cx,xmlstring);
				vp->setUndefined();
				return true;
			}
			JS_free(cx,xmlstring);


			tinyxml2::XMLNode* root = xml_doc.RootElement();
			if(root == nullptr){
				vp->setUndefined();
				return true;
			}

			JS::RootedObject jo(cx,JS_NewObject(cx,&normalClass));

			JS::RootedValue sub_jv_label(cx);
			sub_jv_label.setString(JS_NewStringCopyZ(cx,root->Value()));

			JS_SetProperty(cx,jo,"xml.tag",sub_jv_label);

			xml2js(cx,doc,root,jo);

			vp->setObjectOrNull(jo);
			return true;
		}
		vp->setUndefined();
		return true;
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
#ifdef TINYXML2_FOUND
		JSObject* jo = JS_DefineObject(cx_template,global_template,"XML",&normalClass,0);
		JS::RootedObject js_object(cx_template,jo);
		JS_DefineFunctions(cx_template,js_object,js_xml_static_funs);
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

	static void script_js_error_report(JSContext *cx, const char *message, JSErrorReport *report)
	{
		JS::RootedValue jerror_value(cx);

		if(JS_IsExceptionPending(cx) && JS_GetPendingException(cx,&jerror_value)){
			JSObject *jerror_pobject = jerror_value.toObjectOrNull();
			if(jerror_pobject){
				std::vector<int>* prlines = (std::vector<int>*)JS_GetContextPrivate(cx);
				int lineno = report->lineno;
				if(prlines && lineno < prlines->size() + 1){
					lineno = prlines->at(lineno - 1);
				}

				JS::RootedObject jerror_object(cx,jerror_pobject);
				JS::RootedValue jsstack(cx);
				if(JS_GetProperty(cx,jerror_object,"stack",&jsstack)){
					lyramilk::data::var retv;
					j2v(cx,jsstack,&retv);
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
		init();
		/*
		JSContext* selectedcx = (JSContext *)JS_GetRuntimePrivate(rt);
		JSObject* global = JS_GetGlobalObject(selectedcx);

		std::vector<int> tmprlines;
		lyramilk::data::string scriptstring = code_convert(cscriptstring,&tmprlines);
		JSScript* script = JS_CompileScript(selectedcx,global,scriptstring.c_str(),scriptstring.size(),nullptr,1);
		if(script){
			return !!JS_ExecuteScript(selectedcx,global,script,nullptr);
		}
		*/
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
		static std::map<lyramilk::data::string,bytecode> bytecodemap;
		static lyramilk::threading::mutex_rw bytecodelock;

		if(!scriptfilename.empty()){
			return false;
		}

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
					JS_SetContextPrivate(cx_current,(void*)&c.rlines);
					script = JS_DecodeScript(cx_current,(const void*)c.code.c_str(),c.code.size());
				}
			}
		}
		if(script){
			return JS_ExecuteScript(cx_current,global_current,JS::RootedScript(cx_current,script));
		}else{
			JS::CompileOptions options(cx_current);
			//options.setSourcePolicy(JS::CompileOptions::NO_SOURCE);

			bytecode c;
			lyramilk::data::string scriptstring = code_convert(scriptstring,&c.rlines);


			JS::RootedScript jss(cx_current,script);
			if(!JS::Compile(cx_current,global_current,options,scriptstring.c_str(),scriptstring.size(),&jss)){
				return false;
			}
			if(JS_ExecuteScript(cx_current,global_current,jss)){
				uint32_t len = 0;
				void* p = JS_EncodeScript(cx_current,jss,&len);
				if(p && len){
					bytecode c;
					c.tm = st.st_mtime;
					c.code.assign((const char*)p,len);
					{
						lyramilk::threading::mutex_sync _(bytecodelock.w());
						bytecodemap[scriptfile] = c;
					}
				}
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
