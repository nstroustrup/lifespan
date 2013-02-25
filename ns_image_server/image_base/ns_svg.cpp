#include "ns_svg.h"


using namespace std;

void ns_svg::draw_line(const ns_vector_2d & a, const ns_vector_2d & b, const ns_color_8 & color,const float thickness,const float opacity){
	output+="<g stroke=\"";
	output_color(color,true); 
	 output+="\" opacity=\"" + ns_to_string_short(opacity);
	output+="\" ><line x1=\"" + ns_to_string_short(a.x) + "\" y1=\"" + ns_to_string_short(a.y) + "\" " +
								  "x2=\"" + ns_to_string_short(b.x) + "\" y2=\"" + ns_to_string_short(b.y) + "\" " +
								  "stroke-width=\"" + ns_to_string_short(thickness) + "\" /></g>\n";
}

void ns_svg::open_hyperlink(const string & link, const string & target){
	output+="<a xlink:href=\"";
	output+=link;
	output+= "\"";
	if (target.size() != 0){
		output+=" target=\"";
		output+=target;
		output+="\"";
	}
	output+=">";
}
void ns_svg::close_hyperlink(){
	output+="</a>";
}


void ns_svg::draw_poly_line(const vector<ns_vector_2d> & points, const ns_color_8 & color,const float thickness, const float opacity){
	output+="<polyline fill=\"none\" stroke=\"";
	output_color(color,true); 
	 output+="\" opacity=\"" + ns_to_string_short(opacity);
	output+= "\" stroke-width=\"" + ns_to_string_short(thickness) + "\" points=\"";
	unsigned char c = 0;
	output_points(points);
	output +="\" />\n";
}

void ns_svg::draw_rectangle(const ns_vector_2d & a, const ns_vector_2d & b, const ns_color_8 & line_color, const ns_color_8 & fill_color, const float opacity,const bool draw_line, const bool draw_fill, const float thickness, const float & corner_rounding){
	 output+="<rect fill=\"";
	 output_color(fill_color,draw_fill);
	 output+= "\" stroke=\"";
	 output_color(line_color,draw_line);
	 output+="\" stroke-width=\"" + ns_to_string_short(thickness);
	 output+="\" opacity=\"" + ns_to_string_short(opacity) + "\" ";
	 output+="x=\"" + ns_to_string_short(a.x) + "\" ";
	 output+="y=\"" + ns_to_string_short(a.y) + "\" ";
	 output+="width=\"" + ns_to_string_short(b.x-a.x) + "\" ";
	 output+="height=\"" + ns_to_string_short(b.y-a.y) + "\" ";
	 if (corner_rounding != 0){
		 output += "rx=\"" + ns_to_string_short(corner_rounding) + "\" ";
	 }
	 output+=" />\n";
}

void ns_svg::draw_polygon(const vector<ns_vector_2d> & points, const ns_color_8 & line_color, const ns_color_8 & fill_color, const float opacity,const bool draw_line, const bool draw_fill, const float thickness){
	 output+="<polygon fill=\"";
	 output_color(fill_color,draw_fill);
	 output+= "\" stroke=\"";
	 output_color(line_color,draw_line);
	 output+="\" stroke-width=\"" + ns_to_string_short(thickness);
	 output+="\" opacity=\"" + ns_to_string_short(opacity) + "\" points=\""; 
     output_points(points);
	 output+="\"/>\n";
}

void ns_svg::draw_text(const string & text, const ns_vector_2d & position, const ns_color_8 & color, const float size, const float rotate){
	
	if (rotate == 0){
	output+="<text x=\""+ ns_to_string_short(position.x) + "\" y=\"" + ns_to_string_short(position.y) + "\" ";
	output+=" font-family=\"Arial\" font-size=\"" + ns_to_string_short(size/6) + "\" fill=\"";
	output_color(color);
	output+="\" >";
	output+= text;
	output+="</text>\n";
	}
	else{
	output += "<g transform=\"translate(" + ns_to_string_short(position.x) + " " + ns_to_string_short(position.y) + ")\" >\n";

	output += "<g transform=\"rotate(" + ns_to_string_short(rotate) + ")\" >\n";
	output+="<text font-family=\"Arial\" font-size=\"" + ns_to_string_short(size/6) + "\" fill=\"";
	output_color(color);
	output+="\" >";
	output+= text;
	output+="</text>\n";
	output+="</g>\n</g>\n";
	}
}
void ns_svg::clear(){
	output.resize(0);
}
void ns_svg::compile(ostream & out, const ns_svg_header_spec & spec){
	string head;
	if (!header_added_)
		create_header(spec,head);
	out << head;
	add_footer();
	out << output;
}
void ns_svg::compile(string & out, const ns_svg_header_spec & spec){	
	string head;
	if (!header_added_)
		create_header(spec,head);
	out+=head;
	add_footer();
	out+=output;
}

void ns_svg::draw_complex(const string & str){
	output+=str;
}

void ns_svg::output_hex_val(const ns_8_bit & v){
	ns_8_bit low = v%16;
	ns_8_bit high = v/16;
	
	if (high<10) output+=('0'+high);
	else output += ('a'+(high-10));
	if (low<10) output+=('0'+low);
	else output += ('a'+(low-10));
}
 void ns_svg::output_color(const ns_color_8 & color, const bool & draw){
	if (!draw){
		output+="none";
		return;
	}
	output+="#";
	output_hex_val(color.x);
	output_hex_val(color.y);
	output_hex_val(color.z);
}

 void ns_svg::start_group(){
	output+="<g>";
 }
 void ns_svg::end_group(){
	output+="</g>";

 }
void ns_svg::output_points(const vector<ns_vector_2d> & points){
	char c = 0;
	for (unsigned int i = 0; i < points.size(); i++){
		output += ns_to_string_short(points[i].x) + "," + ns_to_string_short(points[i].y) + " ";
		if (c == 8)
			output += "\n\t";
        c++;
	}

}
void ns_svg::specifiy_header(const ns_svg_header_spec & spec){
	string head;
	create_header(spec,head);
	output.insert(0,head);
	header_added_ = true;
}

void ns_svg::create_header(const ns_svg_header_spec & spec, string & out){
	if (spec.stand_alone){
		out += "<?xml version=\"1.0\" standalone=\"yes\"?>";
		out += "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\"\n";
		out += "\t\"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n";
	}
	string a;
	a = "cm";
	if (spec.percent)
		a = "%";
	out += "<svg width=\"" + ns_to_string(spec.width) + a + "\" height=\"" + ns_to_string_short(spec.height) + a + "\" version=\"1.1\"\n\t";
	out +=  "xmlns=\"http://www.w3.org/2000/svg\" ";
	out +=  "xmlns:xlink=\"http://www.w3.org/1999/xlink\" ";
	out += "preserveAspectRatio=\"no\" ";
	if (!(spec.pos==ns_vector_2d(0,0)))
		out+= "x=\"" + ns_to_string_short(spec.pos.x) + "\" y=\"" +ns_to_string_short(spec.pos.y) + "\" ";
	if (!(spec.view_box==ns_vector_2d(0,0))){
		out+="viewBox=\"0 0 "; 
		out+=ns_to_string_short(spec.view_box.x) + " " + ns_to_string_short(spec.view_box.y) + "\"\n";
	}

	out+=">";
	out += "<desc>";
	if (spec.description.size() != 0){
		out+=spec.description;
		out+="\n";
	}
	out+="Generated by ns_svg.h, Nicholas Stroustrup 2008</desc>\n";
}
void ns_svg::add_footer(){
	output+= "</svg>\n";
}
