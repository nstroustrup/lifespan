#include "ns_vector.h"
std::string ns_color_to_hex_string(const ns_color_8 & c){
	throw ns_ex("ns_color_to_hex_string()::Not Implemented");
}

ns_color_8 ns_hex_string_to_color(const std::string & c){
	if (c.empty())
		return ns_color_8(0,0,0);
	if (c.size() != 6)
		throw ns_ex("ns_hex_string_to_color::Invalid color!");
	char a[3][3];
	a[0][0] = c[0];
	a[0][1] = c[1];
	a[0][2] = 0;
	a[1][0] = c[2];
	a[1][1] = c[3];
	a[1][2] = 0;
	a[2][0] = c[4];
	a[2][1] = c[5];
	a[2][2] = 0;

	ns_color_8 col(strtol(a[0],0,16),strtol(a[1],0,16),strtol(a[2],0,16));
	return col;
}
