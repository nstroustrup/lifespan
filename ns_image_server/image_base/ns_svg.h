#ifndef NS_SVG
#define NS_SVG
#include "ns_vector.h"
#include <vector>
#include <string>
#include <ostream>

struct ns_svg_header_spec{
	ns_svg_header_spec():width(8),height(10),percent(false),view_box(10,10),pos(0,0),stand_alone(true){}
	float width;
	float height;
	bool percent;
	std::string description;
	ns_vector_2d view_box;
	ns_vector_2d pos;
	bool stand_alone;
};

class ns_svg{
public:
	ns_svg():header_added_(false){}
	void draw_line(const ns_vector_2d & a, const ns_vector_2d & b, const ns_color_8 & color, const float thickness=1, const float opacity=1);
	void draw_poly_line(const std::vector<ns_vector_2d> & points, const ns_color_8 & color, const float thickness=1, const float opacity=1);

	void open_hyperlink(const std::string & link, const std::string & target="_blank");
	void close_hyperlink();

	void draw_rectangle(const ns_vector_2d & a, const ns_vector_2d & b, const ns_color_8 & line_color, const ns_color_8 & fill_color, const float opacity=1,const bool draw_line=true, const bool draw_fill=true, const float thickness=1, const float & corner_rounding=0);
	void draw_polygon(const std::vector<ns_vector_2d> & points, const ns_color_8 & line_color, const ns_color_8 & fill_color, const float opacity=1, const bool draw_line=true, const bool draw_fill=true, const float thickness=1);
	void draw_text(const std::string & text, const ns_vector_2d & position, const ns_color_8 & color, const float size, const float rotate=0);
	void clear();
	void compile(std::ostream & out, const ns_svg_header_spec & spec=ns_svg_header_spec());
	void compile(std::string & out, const ns_svg_header_spec & spec=ns_svg_header_spec());
	void draw_complex(const std::string & str);
	void start_group();
	void end_group();
	void specifiy_header(const ns_svg_header_spec & spec);
	bool header_added() const{
		return header_added_;
	}
private:
	void create_header(const ns_svg_header_spec & spec, std::string & out);
	void add_footer();
	inline void output_hex_val(const ns_8_bit & v);
	inline void output_color(const ns_color_8 & color, const bool & draw=true);
	void output_points(const std::vector<ns_vector_2d> & points);
	bool header_added_;

	std::string output;
};


#endif
