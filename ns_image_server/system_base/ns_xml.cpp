#include "ns_ex.h"
#include "ns_xml.h"
#ifndef NS_NO_TINYXML
#include "tinyxml.h"
#endif
#include <stdlib.h>
#include <iostream>

using namespace std;
/*
struct ns_xml_simple_object{
	std::string name;
	std::map<std::string,std::string> properties;
};
*/
#ifndef NS_NO_TINYXML
void convert_to_simple_objects( TiXmlNode* pParent,  std::vector<ns_xml_simple_object> & objs, ns_xml_simple_object * obj, std::string * tag_value, unsigned int level = 0 )
{
	//for (unsigned int i = 0; i < level; i++)
	//	cerr << "\t";
	std::string tmp;
	switch ( pParent->Type())
	{
	case TiXmlNode::DOCUMENT:
		//printf( "Document\n" );
			for (TiXmlNode* pChild = pParent->FirstChild(); pChild != 0; pChild = pChild->NextSibling()) 
				convert_to_simple_objects( pChild,  objs,0,0,level+1 );
		break;
		
	case TiXmlNode::ELEMENT:
		//printf( "Element [%s]\n", pParent->Value() );
		switch(level){
			case 1:{
				//ignore lifespan machine wrapper tag
				if (pParent->Value()==std::string("lifespan_machine")){
					for (TiXmlNode* pChild = pParent->FirstChild(); pChild != 0; pChild = pChild->NextSibling()) 
						convert_to_simple_objects(pChild,objs,0,0,1);
					break;
				}

				std::vector<ns_xml_simple_object>::size_type s = objs.size();
				objs.resize(s+1);
				objs[s].name = pParent->Value();
				for (TiXmlNode* pChild = pParent->FirstChild(); pChild != 0; pChild = pChild->NextSibling()) 
					convert_to_simple_objects(pChild,objs,&objs[s],0,level+1);
				break;
			}
			case 2:{
				if (obj == 0)
					throw ns_ex("An error was identified in the supplied XML file: An empty object was passed to the parser at level ") << level << " in tag " << pParent->Value() << ")";
				
				for (TiXmlNode* pChild = pParent->FirstChild(); pChild != 0; pChild = pChild->NextSibling()){
					convert_to_simple_objects(pChild,objs,obj,&obj->tags[pParent->Value()],level+1);
				}
				break;
				   }
			default: 
				throw ns_ex("An error was identified in the supplied XML file: a tag was found, ") << pParent->Value() << " that was too deeply nested (" << level << ")";
		}

		//obj.name = pParent->Value();
		break;

	case TiXmlNode::COMMENT:
		//printf( "Comment: [%s]\n", pParent->Value());
		break;

	case TiXmlNode::UNKNOWN:
		//printf( "Unknown\n" );
		break;

	case TiXmlNode::TEXT:
		//printf( "Text: [%s]\n",  pParent->Value() );
		switch(level){
			case 2:
				obj->value = pParent->Value();
				break;
			case 3:
				if (tag_value == 0)
					throw ns_ex("An error was identified in the supplied XML file: An empty object was passed to the parser at level ") << level << " in tag " << pParent->Value() << ")";
				*tag_value = pParent->Value();
				break;
			default:
				throw ns_ex("An error was identified in the supplied XML file: A TEXT field was provided, ") << pParent->Value() << ", on level " << level;
		}

	case TiXmlNode::DECLARATION:
		//printf( "Declaration\n" );
		break;
	default:
		break;
	}
	//printf( "\n" );
}


void convert_to_objects( TiXmlNode* pParent,  ns_xml_object & current_object)
{
	//for (unsigned int i = 0; i < level; i++)
	//	cerr << "\t";
	std::string tmp;
	switch ( pParent->Type())
	{
	case TiXmlNode::DOCUMENT:
		//printf( "Document\n" );
			for (TiXmlNode* pChild = pParent->FirstChild(); pChild != 0; pChild = pChild->NextSibling()) 
				convert_to_objects( pChild,  current_object);
		break;

	case TiXmlNode::ELEMENT:{
		//printf( "Element [%s]\n", pParent->Value() );
			std::vector<ns_xml_simple_object>::size_type s = current_object.children.size();
			current_object.children.resize(s+1);
			current_object.children[s].name = pParent->Value();
			TiXmlElement * element(pParent->ToElement());

			TiXmlAttribute* pAttrib=element->FirstAttribute();
			while (pAttrib){
				current_object.children[s].tags[pAttrib->Name()] = pAttrib->Value();
				pAttrib=pAttrib->Next();
			}

			for (TiXmlNode* pChild = pParent->FirstChild(); pChild != 0; pChild = pChild->NextSibling()) 
				convert_to_objects(pChild,current_object.children[s]);

			break;
	}

	case TiXmlNode::TEXT:
		//printf( "Text: [%s]\n",  pParent->Value() );
		current_object.value = pParent->Value();
		break;
		
	case TiXmlNode::DECLARATION:
	case TiXmlNode::COMMENT:
	case TiXmlNode::UNKNOWN:
	default:
		break;
	}
}
void ns_xml_object::to_string(std::string & o){
	if (value.size() != 0 && children.size() > 0)
		throw ns_ex("ns_xml_object::to_string()::Encountered an xml object with a value and children");
	o+="<";
	o+=name;
	for (ns_tag_list::iterator p = tags.begin(); p != tags.end(); p++){
		o+= " ";
		o+= p->first;
		o+= "=\"";
		o+=p->second;
		o+="\"";
	}

	
	if (tags.find("d") != tags.end() && tags["d"] == "M1422.57,-4505.36C1422.57,-4505.36 1289.2,-4626.38 1289.2,-4626.38"){
		int a;
		a=0;
	}
	if (children.size() == 0 && value.size() == 0)
		o+= "/>";
	else o+= ">";
	o+= value;
	if (children.size() != 0)
		o+="\n";
	for (unsigned int i = 0; i < children.size(); i++)
		children[i].to_string(o);

	if (value.size() != 0|| children.size() != 0){
		o += "</";
		o+= name;
		o +=">";
		o+="\n";
	}
}

void ns_xml_object_reader::to_string(std::string & s){
	for (unsigned int i = 0; i < objects.children.size(); i++)
		objects.children[i].to_string(s);
}

void ns_xml_object_reader::from_string(const std::string & s){
	TiXmlDocument doc("in");
	doc.LoadFile("");
	doc.Parse(s.c_str());
	if (doc.Error())
		throw ns_ex("ns_xml_object_reader::Failed to parse document:") << doc.Value() << " : " << doc.ErrorDesc();

	convert_to_objects(&doc,objects);
}


void ns_xml_object_reader::from_filename(const std::string & filename){
	TiXmlDocument doc(filename.c_str());
	
	if (!doc.LoadFile())
		throw ns_ex("ns_xml_object_reader::Failed to parse document:") << doc.Value() << " : " << doc.ErrorDesc();

	convert_to_objects(&doc,objects);
}


void ns_xml_simple_object_reader::from_string(const std::string & s){
	TiXmlDocument doc("in");
	doc.LoadFile("");
	doc.Parse(s.c_str());
	if (doc.Error())
		throw ns_ex("ns_xml_object_reader::Failed to parse document:") << doc.Value() << " : " << doc.ErrorDesc();

	convert_to_simple_objects(&doc,objects,0,0,0);
}

#endif

bool ns_get_next_tag(istream & in,string & text_before_next_tag,string & next_tag){
	text_before_next_tag.resize(0);
	next_tag.resize(0);
	unsigned long state(0);
	while(true){
		char c(in.get());
		if (in.fail())
			return false;
		switch(state){
			case 0:
				if (c=='<'){
					state = 1;
					break;
				}
				text_before_next_tag+=c;
				break;
			case 1:{
				if (c=='>')
					return true;
				next_tag+=c;
				break;
			}
		}
	}
}
bool ns_is_only_whitespace(const string & s){
	for (unsigned int i = 0; i < s.size(); i++)
		if (!isspace(s[i]))
			return false;
	return true;
}
//ask whether the specified tag is a closing tag.
bool ns_is_closing_tag(const std::string & tag){
	return !(tag.size() == 0 || tag[0] != '/');
}
//ask whether specified tag is the closing tag for start_tag
bool ns_is_closing_tag(const std::string & tag, const std::string & start_tag){
	if (!ns_is_closing_tag(tag))
		return false;
	if (tag.size() != start_tag.size()+1)
		return false;
	for (unsigned int i = 1; i < tag.size(); i++){
		if (tag[i] != start_tag[i-1])
			return false;
	}
	return true;
}
const std::string & ns_xml_simple_object::tag(const std::string & key) const {
	std::map<std::string,std::string>::const_iterator p = tags.find(key);
	if(p == tags.end())
		throw ns_ex("ns_xml_simple_object::") << name << "::" << key << " key not specified";
	else return p->second;
}
const std::string & ns_xml_object::tag(const std::string & key) const {
	std::map<std::string,std::string>::const_iterator p = tags.find(key);
	if(p == tags.end())
		throw ns_ex("ns_xml_simple_object::") << name << "::" << key << " key not specified";
	else return p->second;
}
void ns_xml_simple_object_reader::from_stream(istream & in){
	string text_before_next_tag,
		   next_tag;
	bool in_lifespan_machine_tag(false);
	const std::string lifespan_machine_tag("lifespan_machine");
	unsigned long state(0);
	ns_xml_simple_object current_object;
	ns_xml_simple_object::ns_tag_list::iterator current_tag(current_object.tags.end());
	while(true){
		if (!ns_get_next_tag(in,text_before_next_tag,next_tag))
			break;
		switch(state){
			case 0:
				if (next_tag.size() < 4 || next_tag.substr(0,4) != "?xml")
					throw ns_ex("Could not find opening xml tag");
				state++;
				break;
			case 1:{
				if (!ns_is_only_whitespace(text_before_next_tag))
					throw ns_ex("Unknown text at root: ") << text_before_next_tag;
				if (next_tag.size() == 0)
					throw ns_ex("Empty tag found at root");
				if (ns_is_closing_tag(next_tag,"xml")){
					state--;
					break;
				}
			
				if (!in_lifespan_machine_tag && next_tag == lifespan_machine_tag){
					in_lifespan_machine_tag = true;
					break;
				}
				if (in_lifespan_machine_tag && ns_is_closing_tag(next_tag,lifespan_machine_tag)){
					in_lifespan_machine_tag = false;
					break;
				}
				if (ns_is_closing_tag(next_tag))
								throw ns_ex("Improper closing tag at root:") << next_tag;	
				 current_object.name = next_tag;
				 state++;
				 break;
			}
			case 2:{
				if (next_tag.size() == 0)
					throw ns_ex("Empty tag found at root");
				if (!ns_is_closing_tag(next_tag,current_object.name)){
					//hoping that this is a nested tag
					if (ns_is_closing_tag(next_tag))
						throw ns_ex("Improper closing tag at:") << current_object.name << "::" << next_tag;	
					if (!ns_is_only_whitespace(text_before_next_tag))
						throw ns_ex("Unknown text in tag: ") << current_object.name << ": " << text_before_next_tag;
					
					//we've found a nested object

					std::pair<ns_xml_simple_object::ns_tag_list::iterator,bool> r(current_object.tags.insert(ns_xml_simple_object::ns_tag_list::value_type(next_tag,"")));
					if (!r.second)
						throw ns_ex("Duplicate tag found:") << current_object.name << "::" << next_tag;
					current_tag = r.first;
					state++;
				}
				else{
					if (current_object.tags.empty()){
						//we've found an object with no tags, just a value.
						current_object.value = text_before_next_tag;
					}
					else{
						if (!ns_is_only_whitespace(text_before_next_tag))
							throw ns_ex("Value in nested object") << "::" << current_object.name << "::" << text_before_next_tag;
					}
					objects.push_back(current_object);
					current_object.clear();
					state--;
				}
				break;
			}
			case 3:{
				if (!ns_is_closing_tag(next_tag,current_tag->first))
					throw ns_ex("Improper nested tag:") << current_object.name << "::" << current_tag->first << "::" << next_tag;
				current_tag->second = text_before_next_tag;
				state--;
				break;
			}
			default:
				throw ns_ex("Entered an unknown parsing state:") << state;
		}
	}
	if (in_lifespan_machine_tag)
		throw ns_ex("Could not find closing tag for <") << lifespan_machine_tag << ">";
	if (state>1){
		switch(state){
		case 2: throw ns_ex("Could not find closing tag for <") << current_object.name << ">";
		case 3: throw ns_ex("Could not find clossing for <") << current_object.name << "><" << current_tag->first << ">";
		default: ns_ex("Hit end of file in a bizzare state!") << state;
		}
	}
}


void ns_xml_simple_writer::add_header(){
	o += "<?xml version=\"1.0\">";
	o += eolc;
}
void ns_xml_simple_writer::start_group(const std::string & name){
	
	for (unsigned int i = 0; i < groups.size(); i++) o+=tabc;
	o+="<";
	o+=name;
	o+=">";
	o+=eolc;
	groups.push_back(name);
}
void ns_xml_simple_writer::end_group(){
	if(groups.size() == 0)
		throw ns_ex("ns_pairs_to_xml::Invalid end of group specification");
	o+="</";
	o+=groups[groups.size()-1];
	o+=">";
	o+=eolc;
	groups.pop_back();
}
void ns_xml_simple_writer::add_tag(const std::string & k, const std::string & v){
	for (unsigned int i = 0; i < groups.size(); i++) o+=tabc;
	o+="<";
	o+=k;
	o+=">";
	o+=v;
	o+="</";
	o+=k;
	o+=">";
	o+=eolc;
}


void ns_xml_simple_writer::add_raw(const std::string & k){
	o += k;
}

void ns_xml_simple_writer::generate_whitespace(const bool & b){
	if (b){eolc = "\n"; tabc="\t";}
	else  {eolc = ""; tabc="";}
}

void ns_xml_simple_writer::add_footer(){
	o += "</xml>";
	//o += eolc;  //trailing whitespace appears to freak out tinyxml
}
const std::string ns_xml_simple_writer::result(){
	if(groups.size() != 0)
		throw ns_ex("ns_pairs_to_xml::Invalid end of group specification");
	return o;
}
