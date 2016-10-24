#ifndef NS_FONT_H
#define NS_FONT_H
#include "ns_thread.h"

#include "ft2build.h"
#include FT_FREETYPE_H

#include <string>
#include "ns_image.h"
struct ns_font_output_dimension{
	ns_font_output_dimension(const long w_,const long h_):w(w_),h(h_){}
	ns_font_output_dimension(){}
	long w, h;
};

FT_Library & ns_get_ft_library();
///ns_font is a wrapper for the FreeType font renderer, allowing text rendering on ns_image_whole objects.
class ns_font{
public:
	ns_font():face_loaded(false),current_face_height(0){}

	~ns_font(){
		if (face_loaded){
			ns_acquire_lock_for_scope lock(render_lock, __FILE__, __LINE__);
			 FT_Done_Face(face);
			 lock.release();
			 face_loaded = false;
		}
	}
	///Loads the specified font (ie Arial, Helvetica, etc) from disk
	void load_font(const std::string &font_file){
		ns_acquire_lock_for_scope lock(render_lock, __FILE__, __LINE__);

		if (face_loaded){
			 FT_Done_Face(face);
			face_loaded =false;
		}

		int error = FT_New_Face(ns_get_ft_library(), font_file.c_str(), 0, &face );

		if ( error == FT_Err_Unknown_File_Format )
			throw ns_ex("ns_font::Unknown font file format in file ") << font_file;
		else if (error)
			throw ns_ex("ns_font::Error opening font file ") << font_file;
		face_loaded = true;
		lock.release();
	}

	///Specifies the font size of characters to be rendered
	void set_height(const int h){
		if (current_face_height == h)
			return;
		ns_acquire_lock_for_scope lock(render_lock, __FILE__, __LINE__);
		int error = FT_Set_Char_Size(
			face,
			0,64*h,  //width, height
			0,0); //resolution horizontal, vertical.  default 72
		current_face_height = h;
		lock.release();
    }

	ns_font_output_dimension get_render_size(const std::string & text){
		ns_image_standard im;
		im.prepare_to_recieve_image(ns_image_properties(1,1,3,-1));
		return draw_color(0,0,ns_color_8(0,0,0),text,im,false);
	}

	//renders the specified text at (x,y) in the output image.
	//If the output image is grayscale, text is rendered at the
	//grayscale equivalent of the specified color
	template<class color_t>
	ns_font_output_dimension draw(const int  x, const int y, const color_t & color, const std::string & text, ns_image_standard & im){
		if (text.size() == 0)
			return ns_font_output_dimension(0,0);
		if (im.properties().components == 3)
			return draw_color(x,y,color,text,im);
		else return draw_grayscale(x,y,((int)(color.x)+ (int)(color.y) + (int)(color.z))/3,text,im);
	}

	///renders the specified text at (x,y) in the output image.
	///segfaults if the specified image does not have the RGB colorspace
	ns_font_output_dimension draw_color(const int x, const int y, const ns_color_8 & color, const std::string & text, ns_image_standard & im, const bool draw=true){
		
	

		if (im.properties().components != 3)
			throw ns_ex("ns_font::Drawing color on a B&W image");
		int	previous_glyph_index=0;
		int _x = 0;
		const int c(im.properties().components);
		ns_vector_3<float> color_v(color.x,color.y,color.z);
		ns_font_output_dimension dim(0,0);
		ns_acquire_lock_for_scope lock(render_lock, __FILE__, __LINE__);
		for (unsigned int i = 0; i < text.size(); i++) {
			if (text[i] >= 127 || text[i] <= 31)  //ignore invalid characters
				continue;
			_x += render_glyph(text[i],previous_glyph_index);
			if (draw &&
				((x + _x + face->glyph->bitmap_left) < 0 || 
				(x+_x+face->glyph->bitmap.width+ face->glyph->bitmap_left) > (int)im.properties().width ||
				y - face->glyph->bitmap_top + face->glyph->bitmap.rows > (int)im.properties().height ||
				(int)y - (int)face->glyph->bitmap_top < 0))
				continue;

			if (dim.h < face->glyph->bitmap.rows)
				dim.h = face->glyph->bitmap.rows;

			if (draw){
		//		previous_glyph_index = text[i];
				for (int yi = 0; yi < face->glyph->bitmap.rows; yi++)
					for (int xi = 0; xi < face->glyph->bitmap.width; xi++){

						int cur_y = (int)(y+yi)- face->glyph->bitmap_top;
						float v = face->glyph->bitmap.buffer
											[(yi)*((face->glyph->bitmap.width+face->glyph->bitmap.pitch)/2) +
												+ xi];
						v/=255.0;
						ns_vector_3<float> cur(im[cur_y][3*(x+_x+xi + face->glyph->bitmap_left)+0],
												im[cur_y][3*(x+_x+xi + face->glyph->bitmap_left)+1],
												im[cur_y][3*(x+_x+xi + face->glyph->bitmap_left)+2]);
						cur = cur*(1.0-v) + color_v*v;

						im[cur_y][3*(x+_x+xi + face->glyph->bitmap_left)+ 0] = (ns_8_bit)cur.x;
						im[cur_y][3*(x+_x+xi + face->glyph->bitmap_left)+ 1] = (ns_8_bit)cur.y;
						im[cur_y][3*(x+_x+xi + face->glyph->bitmap_left)+ 2] = (ns_8_bit)cur.z;


					}
			}
			/* increment pen position */
			_x += face->glyph->advance.x >> 6;
		}
		lock.release();
		dim.w = _x;
		return dim;
	}

	ns_font_output_dimension text_size(const std::string & text){
		ns_image_standard im;
		return draw_grayscale<ns_8_bit>(0,0,0,text,im,false);
	}

	///renders the specified text at (x,y) in the output image.
	///Incorrect results occur if the image is not in the grayscale colorspace
	template<class ns_component>
	ns_font_output_dimension draw_grayscale(const int x, const int y, const ns_component & val, const std::string & text, ns_image_standard & im,const bool draw=true){
		ns_acquire_lock_for_scope lock(render_lock,__FILE__,__LINE__);
		int	previous_glyph_index=0;
		int _x = 0;
		const int c(im.properties().components);
		ns_font_output_dimension dim;

		for (int i = 0; i < (int)text.size(); i++) {
			if (text[i] >= 127 || text[i] <= 31)  //ignore invalid characters
				continue;
			_x += render_glyph(text[i],previous_glyph_index);
			if (draw && (
				((x+_x+ face->glyph->bitmap_left) < 0 ||
				(x+_x+face->glyph->bitmap.width+ face->glyph->bitmap_left) > (int)im.properties().width ||
				(y - face->glyph->bitmap_top + face->glyph->bitmap.rows) > (int)im.properties().height ||
				(y - face->glyph->bitmap_top) < 0)))
				continue;
			if (dim.h < face->glyph->bitmap.rows)
				dim.h = face->glyph->bitmap.rows;


			if (draw){

				for (int yi = 0; yi < face->glyph->bitmap.rows; yi++)
					for (int xi = 0; xi < face->glyph->bitmap.width; xi++){
						int cur_y = y+yi- face->glyph->bitmap_top;
						float v = face->glyph->bitmap.buffer
											[(yi)*((face->glyph->bitmap.width+face->glyph->bitmap.pitch)/2) +
												+ xi];
						v/=255.0;
		//				cerr << "(" << cur_y <<"," << x+_x+xi + face->glyph->bitmap_left << ")";
						im[cur_y][x+_x+xi + face->glyph->bitmap_left] = (ns_8_bit)(im[cur_y][(x+_x+xi + face->glyph->bitmap_left)]*(float)(1.0-v) + (float)(val)*v);

					}
			}
			/* increment pen position */
			_x += face->glyph->advance.x >> 6;
		}
		lock.release();
		dim.w = _x;
		return dim;
	}
private:
	unsigned long current_face_height;
	static ns_lock render_lock;
	///launch the freetype render engine for specified current character.  Kerning occurs based on the specified index
	///of the last glyph to be rendered
	inline FT_Pos render_glyph(const char & c, int & previous_index){

		int error;
		int glyph_index = FT_Get_Char_Index( face, c );
		FT_Vector delta;
		delta.x = 0;

		if (FT_HAS_KERNING( face ) && previous_index && glyph_index) {
			error = FT_Get_Kerning( face, previous_index, glyph_index, ft_kerning_default, &delta );
			if (error != 0)
				throw ns_ex("ns_font::Error getting Kerning");
		}
		error = 0;

		/* load glyph image into the slot (erase previous one) */
		error = FT_Load_Glyph( face, glyph_index, FT_LOAD_DEFAULT );
		if ( error ) throw ns_ex("ns_font::Error Loading Glyph");

		error = FT_Render_Glyph( face->glyph, FT_RENDER_MODE_NORMAL );
		if ( error )  throw ns_ex("ns_font::Error Rendering Glyph");
		previous_index = glyph_index;
		return delta.x >> 6;
}

	FT_Face face;
	bool face_loaded;
};

class ns_font_server{
public:
	ns_font_server() :default_font_lock("dflk") {}
	ns_lock default_font_lock;
	ns_font & get_default_font();
	~ns_font_server();
private:
	std::map<std::string,ns_font> fonts;
	ns_font * default_font_;
	static FT_Library ft_lib;
	static unsigned int lib_instances;
	friend FT_Library & ns_get_ft_library();
};

extern ns_font_server font_server;
#endif
