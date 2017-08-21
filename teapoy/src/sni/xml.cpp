#include <libmilk/var.h>
#include <libmilk/log.h>
#include <libmilk/multilanguage.h>
#include <libmilk/netaio.h>
#include <libmilk/scriptengine.h>
#include "script.h"
#include <tinyxml.h>

namespace lyramilk{ namespace teapoy{
	static lyramilk::log::logss log(lyramilk::klog,"teapoy.native");


	bool var_to_xml(const lyramilk::data::var::map& m,TiXmlElement* x)
	{
		lyramilk::data::var::map::const_iterator it = m.begin();
		for(;it!=m.end();++it){
			if(it->first == "xml.tag" || it->first == "xml.body") continue;
			x->SetAttribute(lyramilk::data::str(it->first),lyramilk::data::str(it->second.str()));
		}

		lyramilk::data::var::map::const_iterator it_body = m.find("xml.body");
		if(it_body == m.end()) return true;
		if(it_body->second.type() != lyramilk::data::var::var::t_array) throw lyramilk::exception(D("%s应该是%s,但它是%s","xml.body","t_array",it_body->second.type_name().c_str()));

		const lyramilk::data::var::array& ar = it_body->second;
		for(lyramilk::data::var::array::const_iterator it = ar.begin();it!=ar.end();++it){
			if(it->type() == lyramilk::data::var::t_map){
				const lyramilk::data::var::map& childm = *it;
				lyramilk::data::var::map::const_iterator it_tag = childm.find("xml.tag");
				if(it_tag == childm.end()) return false;
				if(it_tag->second.type() != lyramilk::data::var::var::t_str) throw lyramilk::exception(D("%s应该是%s,但它是%s","xml.tag","t_str",it_tag->second.type_name().c_str()));

				TiXmlElement* p = new TiXmlElement(lyramilk::data::str(it_tag->second.str()));
				x->LinkEndChild(p);
				if(!var_to_xml(childm,p)) return false;
			}else if(it->type_like(lyramilk::data::var::t_str)){
				x->LinkEndChild(new TiXmlText(lyramilk::data::str(it->str())));
			}else{
				throw lyramilk::exception(D("xml属性不支持%s类型",it->type_name().c_str()));
			}
		}
		return true;
	}

	bool xml_to_var(const TiXmlNode* x,lyramilk::data::var::map& m)
	{
		for(const TiXmlAttribute* ax = x->ToElement()->FirstAttribute();ax;ax = ax->Next()){
			m[lyramilk::data::str(ax->NameTStr())] = lyramilk::data::str(ax->ValueStr());
		}

		const TiXmlNode* cx = x->FirstChild();
		if(cx == nullptr) return true;
		lyramilk::data::var& tmpv = m["xml.body"];
		tmpv.type(lyramilk::data::var::t_array);
		lyramilk::data::var::array& ar = tmpv;
		for(;cx;cx = cx->NextSibling()){
			int t = cx->Type();
			if(t == TiXmlNode::TINYXML_ELEMENT){
				ar.push_back(lyramilk::data::var::map());
				lyramilk::data::var::map& m = ar.back();
				m["xml.tag"] = lyramilk::data::str(cx->ValueStr());
				xml_to_var(cx->ToElement(),m);
			}else if(t == TiXmlNode::TINYXML_TEXT){
				ar.push_back(lyramilk::data::str(cx->ValueStr()));
			}
		}
		return true;
	}


	lyramilk::data::var xml_stringify(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_map);
		bool has_header = false;
		if(args.size() > 1){
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_bool);
			has_header = args[1];
		
		}
		const lyramilk::data::var::map& m = args[0];

		TiXmlDocument xml_doc;
		if(has_header){
			xml_doc.LinkEndChild(new TiXmlDeclaration("1.0","UTF-8","yes"));
		}

		lyramilk::data::var::map::const_iterator it_tag = m.find("xml.tag");
		if(it_tag == m.end()) return lyramilk::data::var::nil;
		if(it_tag->second.type() != lyramilk::data::var::var::t_str) throw lyramilk::exception(D("%s应该是%s,但它是%s","xml.tag","t_str",it_tag->second.type_name().c_str()));

		TiXmlElement* p = new TiXmlElement(lyramilk::data::str(it_tag->second.str()));
		xml_doc.LinkEndChild(p);
		if(!var_to_xml(m,p)) return lyramilk::data::var::nil;


		std::string str;
		str << xml_doc;
		return lyramilk::data::str(str);
	}

	lyramilk::data::var xml_parse(const lyramilk::data::var::array& args,const lyramilk::data::var::map& env)
	{
		MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
		lyramilk::data::string str = args[0].str();

		lyramilk::data::stringstream ss(str);
		TiXmlDocument xml_doc;
		ss >> xml_doc;


		TiXmlNode* root = xml_doc.RootElement();
		if(root == nullptr) throw lyramilk::exception(D("解析%s失败","xml"));
		lyramilk::data::var::map vret;
		vret["xml.tag"] = lyramilk::data::str(root->ValueStr());

		if(!xml_to_var(root,vret)) return lyramilk::data::var::nil;
		return vret;
	}

	static int define(lyramilk::script::engine* p)
	{
		p->define("xml_stringify",xml_stringify);
		p->define("xml_parse",xml_parse);
		return 2;
	}


	static __attribute__ ((constructor)) void __init()
	{
		lyramilk::teapoy::script2native::instance()->regist("xml",define);
	}

}}