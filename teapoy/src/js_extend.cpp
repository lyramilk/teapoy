//#define __STDC_LIMIT_MACROS
#include "js_extend.h"
#include <jsapi.h>
#include <jsfriendapi.h>
#include <tinyxml2.h>
#include <math.h>

namespace lyramilk{ namespace teapoy{ namespace sni{

	static JSBool js_xml_stringify(JSContext *cx, unsigned argc, js::Value *vp);
	static JSBool js_xml_parse(JSContext *cx, unsigned argc, js::Value *vp);


	static JSClass normalClass = { "normal", 0,
			JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
            JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub};

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
				return JS_FALSE;
			}
			JS_free(cx,xmlstring);


			tinyxml2::XMLNode* root = xml_doc.RootElement();
			if(root == nullptr){
				return JS_FALSE;
			}
			JSObject *jo = JS_NewObject(cx,&normalClass,nullptr,JS_GetGlobalObject(cx));
			xml2js(cx,doc,root,jo);

			vp->setObjectOrNull(jo);
		}
		return JS_TRUE;
	}

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
		JSObject* jo = JS_DefineObject(selectedcx,global,"XML",&normalClass,NULL,0);

		JS_DefineFunctions(selectedcx,jo,js_xml_static_funs);
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
}}}
