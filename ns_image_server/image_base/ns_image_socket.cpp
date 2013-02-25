#include "ns_image_socket.h"

using namespace std;

unsigned int ns_image_properties_length_as_char(){
	ns_image_properties p;
	return (sizeof(p.height)+sizeof(p.width)+sizeof(p.components) + sizeof(p.resolution))/sizeof(char);
}
unsigned int ns_image_properties_to_char(const ns_image_properties & p, char *& buf){
	buf = new char[ns_image_properties_length_as_char()+p.description.size()+1];

	const char * h(reinterpret_cast<const char * >(&p.height));
	const char * w(reinterpret_cast<const char * >(&p.width));
	const char * c(reinterpret_cast<const char * >(&p.components));
	const char * f(reinterpret_cast<const char * >(&p.resolution));
	unsigned int i;
	for (i = 0; i < sizeof(p.height)/sizeof(char); i++)
		buf[i] = h[i];	
	for (unsigned int j = 0 ; j < sizeof(p.width)/sizeof(char); j++){
		buf[i] = w[j];
		i++;
	}
	for (unsigned int j = 0 ; j < sizeof(p.components)/sizeof(char); j++){
		buf[i] = c[j];
		i++;
	}
	for (unsigned int j = 0 ; j < sizeof(p.resolution)/sizeof(char); j++){
		buf[i] = f[j];
		i++;
	}
	for (unsigned int j = 0; j < p.description.size(); j++){
		buf[i] = p.description[j];
		i++;
	}
	buf[i] = 0;
	return ns_image_properties_length_as_char() + p.description.size() + 1;
}

ns_image_properties ns_char_to_image_properties(const char * buf,const int header_length){
	ns_image_properties p;

	char * h = reinterpret_cast<char *>(&p.height);
	char * w = reinterpret_cast<char *>(&p.width);
	char * c = reinterpret_cast<char *>(&p.components);
	char * f = reinterpret_cast<char *>(&p.resolution);
	unsigned int i;
	for (i = 0; i < sizeof(p.height)/sizeof(char); i++)
		h[i] = buf[i];
	for (unsigned int j = 0; j < sizeof(p.width)/sizeof(char); j++){
		w[j] = buf[i];
		i++;
	}
	for (unsigned int j =0; j < sizeof(p.components)/sizeof(char); j++){
		c[j] = buf[i];
		i++;
	}	
	for (unsigned int j =0; j < sizeof(p.resolution)/sizeof(char); j++){
		f[j] = buf[i];
		i++;
	}
	for (;buf[i] != 0; i++){
		if (i >= header_length)
			throw ns_ex("ns_char_to_image_properties()::Improperly sized header!");
		p.description.push_back(buf[i]);
	}
	return p;

}
