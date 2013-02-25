#ifndef NS_XMP_ENCODER
#define NS_XMP_ENCODER
#include <string>
#include <iostream>
#include "ns_ex.h"


struct ns_xmp_tiff_data{
	int orientation;
	float xresolution,yresolution;
	int resolution_unit;
};
class ns_long_string_xmp_encoder{
public:
	static void write(const std::string & s, std::string & xmp, const ns_xmp_tiff_data * t=0){
		std::string escaped_output = "<![CDATA[";
		escaped_output += "!ns_image_server:";
		escaped_output+=s;
		escaped_output += "]]>";

		if (escaped_output.size() >= 64*1024)
		//	throw ns_ex(
				std::cerr << "Warning: The specified TIFF description field is very long (" << ((unsigned long)s.size())/1024 << "KB).  \
						 This is fine according to the Adobe XMP standard, but Adobe Photoshop chokes on XMP dc:description \
						 fields longer than 2^16-1 characters. \n";
		xmp.resize(0);
		xmp+="<?xpacket begin=\"\" id=\"W5M0MpCehiHzreSzNTczkc9d\"?>";
		xmp+="<x:xmpmeta xmlns:x=\"adobe:ns:meta/\" x:xmptk=\"XMP Core 4.1.1-Exiv2\">";
		xmp+="<rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\"";
		xmp+="         xmlns:dc=\"http://purl.org/dc/elements/1.1/\"";
		xmp+=">";
		xmp+="  <rdf:Description rdf:about=\"\">";   
		xmp+="    <dc:description>";
		xmp+="    <rdf:Alt>";
		xmp+="     <rdf:li xml:lang=\"x-default\">";
		xmp+="";
		xmp+=escaped_output;
		xmp+="</rdf:li>";
		xmp+="    </rdf:Alt>";
		xmp+="    </dc:description>";
		xmp+="  </rdf:Description>";
		xmp+="</rdf:RDF>";
		xmp+="</x:xmpmeta>";
		xmp+="<?xpacket end=\"w\"?>";
		if (t != 0){
			xmp+="<rdf:RDF xmlns:rdf='http://www.w3.org/1999/02/22-rdf-syntax-ns#'>\n"
				 "<rdf:Description about='' xmlns:tiff='http://ns.adobe.com/tiff/1.0'\n"
			
			"tiff:Orientation='";
			xmp += ns_to_string(t->orientation) + "'\n";
			xmp+="tiff:XResolution='";
			xmp += ns_to_string(t->xresolution)+ "'\n";
			xmp+="tiff:YResolution='";
			xmp += ns_to_string(t->yresolution)+ "'\n";
			xmp+="tiff:ResolutionUnit='";
			xmp += ns_to_string(t->resolution_unit) + "'\n";
			xmp+= "</rdf:Description>";
		}
	}
	static void read(const std::string & xmp, std::string & s, ns_xmp_tiff_data * t=0){
	//	char * dsc;
		long count(0);
		{
			std::string::size_type a = xmp.find("<dc:description>");
			if (a == std::string::npos){
				s.clear();
				return;
			}
			std::string::size_type a1 = xmp.find("!ns_image_server:",a);
			if (a1 == std::string::npos){
				s.clear();
				return;
			}
			std::string::size_type b = xmp.find("]]>",a1+17);
			if (b == std::string::npos)
				b = xmp.find("<",a1+17);

			if (b == std::string::npos){
				s.clear();
				return;
			}
			std::string tmp = xmp.substr(a1+17,b-a1-17);
			s.resize(0);
			s.reserve(tmp.size());
			unsigned long state(0);
			for (unsigned int i = 0; i < tmp.size(); i++){
				
				if (state == 0){
						if (tmp[i] != '&') s.push_back(tmp[i]);
						else state++;
				}
				else if (state == 1){
					switch(tmp[i]){
						case 'l': s.push_back('<'); state++; break;
						case 'g': s.push_back('>'); state++; break;
						case 'a': s.push_back('&'); state++; break;
						case '#': state++; break; //photoshop converts whitespace, which we can ignore
						default: s.push_back('&');
								 s.push_back(tmp[i]);
								 state = 0;
					}
				}
				else if (tmp[i] ==';')
					state = 0;
			}
		}
		if (t != 0){
			std::string::size_type o,o1;
			std::string tmp;
			o= xmp.find("tiff:Orientation='");
			o1 = xmp.find_first_of("'",o+18);
			tmp = xmp.substr(o+18,o1-o-18+1);
			t->orientation = atol(tmp.c_str());
			o = xmp.find("tiff:XResolution='");
			o1 = xmp.find_first_of("'",o+18);
			tmp = xmp.substr(o+18,o1-o-18+1);
			t->xresolution = atof(tmp.c_str());
			o = xmp.find("tiff:YResolution='");
			o1 = xmp.find_first_of("'",o+18);
			tmp = xmp.substr(o+18,o1-o-18+1);
			t->yresolution = atof(tmp.c_str());
			o = xmp.find("tiff:ResolutionUnit='");
			o1 = xmp.find_first_of("'",o+21);
			tmp = xmp.substr(o+21,o1-o-21+1);
			t->resolution_unit = atol(tmp.c_str());
		}
	}
};

#endif
