#ifndef NS_FONT_H
#define NS_FONT_H
#include "ns_thread.h"

#include "ft2build.h"
#include FT_FREETYPE_H
#include FT_CACHE_H

#include <string>
#include "ns_image.h"
struct ns_font_output_dimension{
	ns_font_output_dimension(const long w_,const long h_):w(w_),h(h_), offset_x(0), offset_y(0) {}
	ns_font_output_dimension():w(0), h(0),offset_x(0),offset_y(0){}
	long w, h;

	//some rotations introduce an offset < 0
	long offset_x, offset_y;
};

struct ns_glyph_cach_entry {
	ns_glyph_cach_entry(char c_, unsigned int size_, float angle_) :c(c_), size(size_), angle(angle_) {}
	char c;
	unsigned int size;
	float angle;
};
bool operator<(const ns_glyph_cach_entry& a, const ns_glyph_cach_entry& b);

FT_Library & ns_get_ft_library();
///ns_font is a wrapper for the FreeType font renderer, allowing text rendering on ns_image_whole objects.
class ns_font{
public:
	ns_font():face_loaded(false),current_face_height(0), face(0){}

	~ns_font();
	///Loads the specified font (ie Arial, Helvetica, etc) from disk
	void load_font(const std::string& font_file);

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

	ns_font_output_dimension get_render_size(const std::string& text, float angle = 0, bool lock=true) {
		ns_image_standard im;
		im.prepare_to_recieve_image(ns_image_properties(1,1,3,-1));
		return draw_color(0,0,angle,ns_color_8(0,0,0),text,im,false,lock);
	}

	//renders the specified text at (x,y) in the output image.
	//If the output image is grayscale, text is rendered at the
	//grayscale equivalent of the specified color
	template<class color_t>
	ns_font_output_dimension draw(const int  x, const int y, const color_t& color, const std::string& text, ns_image_standard& im) {
		return draw(x, y, 0, color, text, im);
	}

	template<class color_t>
	ns_font_output_dimension draw(const int  x, const int y, const float angle,const color_t & color, const std::string & text, ns_image_standard & im){
		if (text.size() == 0)
			return ns_font_output_dimension(0,0);
		if (im.properties().components == 3)
			return draw_color(x,y,angle,color,text,im);
		else return draw_grayscale(x,y,angle,((int)(color.x)+ (int)(color.y) + (int)(color.z))/3,text,im);
	}

	///renders the specified text at (x,y) in the output image.
	///segfaults if the specified image does not have the RGB colorspace
	ns_font_output_dimension draw_color(const int x, const int y, const ns_color_8& color, const std::string& text, ns_image_standard& im, const bool draw = true, const bool lock = true) {
	  return draw_color(x, y, 0, color, text, im, draw,lock);
	}
	ns_font_output_dimension draw_color(const int x, const int y, const float angle,const ns_color_8 & color, const std::string & text, ns_image_standard & im, const bool draw=true,const bool lock=true){
		
		if (im.properties().components != 3)
			throw ns_ex("ns_font::Drawing color on a B&W image");
		int	previous_glyph_index=0;
		FT_Vector d;
		d.x = d.y = 0;
		const int c(im.properties().components);
		ns_vector_3<float> color_v(color.x,color.y,color.z);
		ns_font_output_dimension dim(0,0);
		ns_acquire_lock_for_scope render_locker(render_lock,__FILE__,__LINE__,false);
		if (lock)
		  render_locker.get( __FILE__, __LINE__);

		//get the offset so that nothing runs less than x and y
		ns_font_output_dimension offset;
		if (draw) {
		  offset = get_render_size(text, angle,false);
			d.x = -offset.offset_x;
			d.y = -offset.offset_y;
			if (angle == 0)
				offset.h= 0;
		}
		FT_Glyph rglyph;
		for (unsigned int i = 0; i < text.size(); i++) {
			if (text[i] >= 127 || text[i] <= 31)  //ignore invalid characters
				continue;
			FT_Vector f = render_glyph(text[i],angle,previous_glyph_index, rglyph);
			d.x += f.x;
			d.y += f.y;


			FT_BitmapGlyph glyph = (FT_BitmapGlyph)rglyph;
			if (!draw) { //recognize any parts after rotation that stick to the left or below x and y.
						  //when we actually draw, we swill shift over by this amount.
				if (dim.offset_y > glyph->top  + d.y)
					dim.offset_y = glyph->top + d.y;
				if (dim.offset_x > glyph->left+ d.x)
					dim.offset_x = glyph->left+ d.x;
			}


			if (draw &&
				(
				(x + d.x + glyph->left) < 0 || (y + d.y - glyph->top < 0) ||
				(x + d.x + glyph->left + glyph->bitmap.width) > (int)im.properties().width ||
				//((long)y + d.y - glyph->top + glyph->bitmap.rows > (int)im.properties().height)
					(offset.h + y < glyph->top - d.y) ||
					(offset.h + y - d.y > im.properties().height)

					))
				continue;

			//update overall dimensions
			FT_BBox bounding_box;

			FT_Glyph_Get_CBox(rglyph, FT_GLYPH_BBOX_UNSCALED, &bounding_box);
			if (dim.h < ((bounding_box.yMax - bounding_box.yMin) >> 6) + d.y)
				dim.h = ((bounding_box.yMax - bounding_box.yMin) >> 6) + d.y;
			if (dim.w < ((bounding_box.xMax - bounding_box.xMin) >> 6) + d.x)
				dim.w = ((bounding_box.xMax - bounding_box.xMin) >> 6) + d.x;

			if (draw){
		//		previous_glyph_index = text[i];
				for (unsigned int yi = 0; yi < glyph->bitmap.rows; yi++)
					for (unsigned int xi = 0; xi < glyph->bitmap.width; xi++){

						int cur_y = offset.h - (glyph->top- (int)(y + yi) + d.y) ;  //offset.offset_y - glyph->top + y + yi - d.y
						float v = glyph->bitmap.buffer
											[(yi)*((glyph->bitmap.width+glyph->bitmap.pitch)/2) +
												+ xi];
						v/=255.0;
						ns_vector_3<float> cur(im[cur_y][3*(x+d.x+xi + glyph->left )+0],
												im[cur_y][3*(x+d.x+xi + glyph->left )+1],
												im[cur_y][3*(x+d.x+xi + glyph->left )+2]);
						cur = cur*(1.0-v) + color_v*v;

						im[cur_y][3*(x+d.x+xi + glyph->left)+ 0] = (ns_8_bit)cur.x;
						im[cur_y][3*(x+d.x+xi + glyph->left)+ 1] = (ns_8_bit)cur.y;
						im[cur_y][3*(x+d.x+xi + glyph->left)+ 2] = (ns_8_bit)cur.z;


					}
			}
			/* increment pen position */
			d.x += rglyph->advance.x >> 16;
			d.y += rglyph->advance.y >> 16;
		}
		if (lock)
		render_locker.release();
		if (!draw) {	//enlarge the final size to allow for the offset shift
			dim.w -= dim.offset_x;
			dim.h -= dim.offset_y;
		}
		return dim;
	}

	ns_font_output_dimension text_size(const std::string & text){
		ns_image_standard im;
		return draw_grayscale<ns_8_bit>(0,0,0,0,text,im,false);
	}

	///renders the specified text at (x,y) in the output image.
	///Incorrect results occur if the image is not in the grayscale colorspace
	template<class ns_component>
	ns_font_output_dimension draw_grayscale(const int x, const int y,const ns_component& val, const std::string& text, ns_image_standard& im, const bool draw = true) {
		return draw_grayscale(x, y, 0, val, text, im, draw);
	}
	template<class ns_component>
	ns_font_output_dimension draw_grayscale(const int x, const int y, const float angle,const ns_component & val, const std::string & text, ns_image_standard & im,const bool draw=true){
		ns_acquire_lock_for_scope lock(render_lock,__FILE__,__LINE__);
		int	previous_glyph_index=0;
		const int c(im.properties().components);
		FT_Vector d;
		d.x = 0;
		d.y = 0;
		ns_font_output_dimension dim(0, 0);
		FT_Glyph rglyph;
		for (int i = 0; i < (int)text.size(); i++) {
			if (text[i] >= 127 || text[i] <= 31)  //ignore invalid characters
				continue;
			FT_Vector f = render_glyph(text[i], angle, previous_glyph_index, rglyph);
			d.x += f.x;
			d.y += f.y;
			FT_BitmapGlyph glyph = (FT_BitmapGlyph)rglyph;
			if (draw &&
				(
				(x + d.x + glyph->left) < 0 || (y + d.y - glyph->top < 0) ||
					(x + d.x + glyph->left + glyph->bitmap.width) > (int)im.properties().width ||
					((long)y + d.y - glyph->top + glyph->bitmap.rows > (int)im.properties().height)
					))
				continue;

			//find final area of image
			FT_BBox bounding_box;
			FT_Glyph_Get_CBox(rglyph, FT_GLYPH_BBOX_UNSCALED, &bounding_box);
			if (dim.h < ((bounding_box.yMax - bounding_box.yMin) >> 6) + d.y)
				dim.h = ((bounding_box.yMax - bounding_box.yMin) >> 6) + d.y;
			if (dim.w < ((bounding_box.xMax - bounding_box.xMin) >> 6) + d.x)
				dim.w = ((bounding_box.xMax - bounding_box.xMin) >> 6) + d.x;
				

			if (draw){

				for (unsigned int yi = 0; yi < glyph->bitmap.rows; yi++)
					for (unsigned int xi = 0; xi < glyph->bitmap.width; xi++){
						int cur_y = y+yi- glyph->top;
						float v = glyph->bitmap.buffer
											[(yi)*((glyph->bitmap.width+glyph->bitmap.pitch)/2) +
												+ xi];
						v/=255.0;
						im[cur_y][x+d.x+xi + glyph->left] = (ns_8_bit)(im[cur_y][(x+d.x+xi + glyph->left)]*(float)(1.0-v) + (float)(val)*v);

					}
			}
			/* increment pen position */
			d.x += rglyph->advance.x >> 16;
			d.y += rglyph->advance.y >> 16;
		}
		lock.release();
		//dim.w = d.x;
		return dim;
	}
private:
	unsigned long current_face_height;
	static ns_lock render_lock;
	FT_Vector render_glyph(const char& c, const float &angle, int& previous_index, FT_Glyph & glyph);
	typedef std::map<ns_glyph_cach_entry, std::pair<int, FT_Glyph> > ns_glyph_cache_type;
	ns_glyph_cache_type glyph_cache;
	FT_Face face;
	bool face_loaded;

	friend class ns_font_server;
};

class ns_font_server{
public:
	FTC_Manager* cache_manager;
	ns_font_server() :default_font_lock("dflk"), default_font_(0){}
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
