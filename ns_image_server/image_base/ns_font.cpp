#include "ns_font.h"


using namespace std;

FT_Library ns_font_server::ft_lib;
unsigned int ns_font_server::lib_instances = 0;

ns_font_server font_server;

FT_Library &ns_get_ft_library(){
	return ns_font_server::ft_lib;
}
#ifdef _WIN32
//fix old freetype reference
extern "C" { FILE __iob_func[3] = { *stdin,*stdout,*stderr }; }
#endif
ns_font & ns_font_server::default_font(){
	if (lib_instances == 0){
		if (FT_Init_FreeType( &ft_lib))
			throw ns_ex("ns_font:: Could not initialize freetype library.");
		#ifdef _WIN32 
		//include strstr just to make sure it gets linked, so that freetype.lib finds it
		strstr("foo", "foobar");
		#endif
		lib_instances++;
	}

	if (fonts.size() == 0){
		pair<map<string,ns_font>::iterator,bool> p = fonts.insert(pair<string,ns_font>("ariel",ns_font()));
		p.first->second.load_font(NS_DEFAULT_FONT);
		p.first->second.set_height(16);
		default_font_ = &p.first->second;
	}
	return *default_font_;
}


ns_font_server::~ns_font_server(){
	fonts.clear();
	if (lib_instances == 1){
		FT_Done_FreeType(ft_lib);
		lib_instances--;
	}
}
