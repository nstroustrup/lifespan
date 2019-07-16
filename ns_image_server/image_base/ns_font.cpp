#include "ns_font.h"

using namespace std;
ns_lock ns_font::render_lock("ft_l");
FT_Library ns_font_server::ft_lib;
unsigned int ns_font_server::lib_instances = 0;

ns_font_server font_server;

FT_Library &ns_get_ft_library(){
	return ns_font_server::ft_lib;
}
FT_Vector ns_font::render_glyph(const char& c, int& previous_index, FT_Glyph & glyph) {
	int error;
	FT_Vector delta;
	delta.x = delta.y = 0;

	/* load glyph image into the slot (erase previous one) */
	ns_glyph_cache_type::iterator p = glyph_cache.find(ns_glyph_cach_entry(c, current_face_height));
	if (p == glyph_cache.end()) {

		int glyph_index = FT_Get_Char_Index(face, c);
		p = glyph_cache.insert(ns_glyph_cache_type::value_type(ns_glyph_cach_entry(c, current_face_height), ns_glyph_cache_type::mapped_type(glyph_index, FT_Glyph()))).first;
		error = FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
		if (error != 0) throw ns_ex("ns_font::Error loading glyph");
		error = FT_Get_Glyph(face->glyph, &p->second.second);
		if (error != 0) throw ns_ex("ns_font::Error copying glyph");
		error = FT_Glyph_To_Bitmap(&p->second.second, FT_RENDER_MODE_NORMAL, 0, false);
		if (error != 0) throw ns_ex("ns_font::Error rendering glyph");
	}

	if (FT_HAS_KERNING(face) && previous_index && p->second.first) {
		error = FT_Get_Kerning(face, previous_index, p->second.first, ft_kerning_default, &delta);
		if (error != 0)
			throw ns_ex("ns_font::Error getting Kerning");
	}
	glyph = p->second.second;

	previous_index = p->second.first;
	delta.x = delta.x >> 6;
	delta.y = delta.y >> 6;
	return delta;
}
bool operator<(const ns_glyph_cach_entry& a, const ns_glyph_cach_entry& b) {
	if (a.c == b.c) return a.size < b.size;
	return a.c < b.c;
}

ns_font::~ns_font() {
		if (face_loaded) {
			ns_acquire_lock_for_scope lock(render_lock, __FILE__, __LINE__);
			FT_Done_Face(face);
			lock.release();
			face_loaded = false;
		}
	for (ns_glyph_cache_type::iterator p = glyph_cache.begin(); p != glyph_cache.end(); ++p) {
		FT_Done_Glyph(p->second.second);
	}
	glyph_cache.clear();
}

void ns_font::load_font(const std::string& font_file) {
		ns_acquire_lock_for_scope lock(render_lock, __FILE__, __LINE__);

		if (face_loaded) {
			FT_Done_Face(face);
			face_loaded = false;
		}

		int error = FT_New_Face(ns_get_ft_library(), font_file.c_str(), 0, &face);

		if (error == FT_Err_Unknown_File_Format)
			throw ns_ex("ns_font::Unknown font file format in file ") << font_file;
		else if (error)
			throw ns_ex("ns_font::Error opening font file ") << font_file;
		face_loaded = true;
		lock.release();
}
#ifdef _WIN32
//fix old freetype reference
extern "C" { FILE __iob_func[3] = { *stdin,*stdout,*stderr }; }
#endif
ns_font & ns_font_server::get_default_font(){
	if (lib_instances == 0){
		if (FT_Init_FreeType( &ft_lib))
			throw ns_ex("ns_font:: Could not initialize freetype library (init).");
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
