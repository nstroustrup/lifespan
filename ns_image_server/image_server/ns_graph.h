#ifndef NS_GRAPH
#define NS_GRAPH

#include <fstream>
#include "ns_image.h"
#include "ns_tiff.h"
#include "ns_jpeg.h"
#include <vector>
#include "ns_font.h"
#include "ns_svg.h"
#include <math.h>
#include <float.h>

///Stores display parameters of a ns_graph_object such as color, width, opacity, etc.
struct ns_graph_color_set{
	ns_graph_color_set():width(1),opacity(1.0){}
	ns_graph_color_set(const bool _draw, const ns_color_8 & _color, const unsigned char _width, const unsigned char _edge_width, const float _opacity):
							draw(_draw),color(_color),width(_width),edge_width(_edge_width),opacity(_opacity),point_shape(ns_square){}
	typedef enum {ns_square,ns_vertical_line} ns_point_shape;
	bool draw;
	ns_color_8 color;
	ns_color_8 edge_color;
	unsigned char width;
	unsigned char edge_width;
	ns_point_shape point_shape;
	float opacity;
};

///Each graph object has subcomponents (outline, fill area, associated text, etc) whose properties
///can be independantly specified.  ns_graph_properties specifies the display parameters of each aspect
///of a ns_graph_object
struct ns_graph_properties{
	ns_graph_properties():line		(false,ns_color_8(0,0,0),3,1,1),
						  point		(true,ns_color_8(0,0,0),3,1,1),
						  area_fill	(false,ns_color_8(255,255,255),0,0,1.0),
						  text		(true,ns_color_8(0,0,0),0,0,1),
						  text_size(14),
						  text_decimal_places((unsigned int)-1),
						  line_hold_order(ns_first),
						  draw_vertical_lines(ns_no_line),
						  draw_negatives(true),draw_tick_marks(true){}
	ns_graph_color_set line;
	ns_graph_color_set point;
	ns_graph_color_set area_fill;
	ns_graph_color_set text;

	bool draw_negatives;
	bool draw_tick_marks;

	unsigned int text_decimal_places;
	int text_size;
	typedef enum {ns_zeroth, ns_zeroth_centered,ns_first} ns_hold_order;
	typedef enum {ns_no_line,ns_outline,ns_full_line} ns_vertical_line_type;
	ns_hold_order line_hold_order; //the way lines are drawn
	ns_vertical_line_type draw_vertical_lines;

};

///Data to be displayed on a graph.
///Dependant variables represent a common data series that is used as the x-axis for multiple independant variables.
///Independant variables represent positions on the y axis at which data occurs.  if data is provided in the x std::vector,
///the object is treated as scatter plot (x,y) pairs.  If x vales are not provided, the graph's global dependant variable
///is used.
///Vertical lines are drawn as a vertical line positioned at x=x[0].
///Stars are drawn as an asterisk at position (x[0],y[0])
struct ns_graph_object{
	void clear() { y.clear(); x.clear(); data_label.clear(); x_label.clear(); y_label.clear(); hyperlinks.clear();}
	typedef enum{ ns_graph_none, ns_graph_independant_variable, ns_graph_dependant_variable, ns_graph_vertical_line, ns_graph_star,ns_graph_horizontal_line} ns_graph_object_type;

	ns_graph_object(const ns_graph_object_type & _type):type(_type){}
	ns_graph_object_type type;
	ns_graph_properties properties;
	std::vector<double> y;
	std::vector<double> x;
	//when outputted to svg, data points
	//can be hyperlinked to outside sources.
	std::vector<std::string> hyperlinks;
	std::string data_label;
	std::string x_label,y_label;
};

/*
* ns_graph_axis specifies the boundaries and scaling of a graph
* boundaries[0-1]: max and min of x axis
* boundaries[2-3]: max and min of y axis
* tick_intervals[0-1]: major and minor tick size of x axis
* tick_intervals[2-3]: major and minor tick size of y axis
*/
struct ns_graph_axes{
	typedef enum{ns_at_zero,ns_at_min_value,ns_at_max_value} ns_axes_position;
	ns_graph_axes(){
		_boundaries[0] = 0;
		_boundaries[1] = 0;
		_boundaries[2] = 0;
		_boundaries[3] = 0;
		_tick_intervals[0] = 0;
		_tick_intervals[1] = 0;
		_tick_intervals[2] = 0;
		_tick_intervals[3] = 0;
		_boundary_specified[0] = false;
		_boundary_specified[1] = false;
		_boundary_specified[2] = false;
		_boundary_specified[3] = false;
		_tick_interval_specified[0] = false;
		_tick_interval_specified[1] = false;
		_tick_interval_specified[2] = false;
		_tick_interval_specified[3] = false;
		_axis_offset[0] = 0;
		_axis_offset[1] = 0;
		axis_position[0] = axis_position[1] = ns_at_min_value;
	}
	ns_axes_position axis_position[2];
	inline double & operator[] (const unsigned int & i){return boundary(i);}

	inline double & boundary(const unsigned int & i){
		_boundary_specified[i] = true;
		return _boundaries[i];
	}
	inline const double & boundary(const unsigned int & i) const{return _boundaries[i];}

	inline double & tick(const unsigned int & i){
		_tick_interval_specified[i] = true;
		return _tick_intervals[i];
	}
	const inline double & tick(const unsigned int & i) const{return _tick_intervals[i];}

	const inline bool & boundary_specified(const unsigned int & i) const{return _boundary_specified[i];}
	const inline bool & tick_interval_specified(const unsigned int & i) const{return _tick_interval_specified[i];}
	const inline double & axis_offset(const unsigned int & i) const {return _axis_offset[i];}

	inline bool & boundary_specified(const unsigned int & i) {return _boundary_specified[i];}
	inline bool & tick_interval_specified(const unsigned int & i) {return _tick_interval_specified[i];}
	inline double & axis_offset(const unsigned int & i) {return _axis_offset[i];}

	void check_for_sanity() const;

private:
	double _boundaries[4];
	double _tick_intervals[4];

	bool _boundary_specified[4];
	bool _tick_interval_specified[4];
	double _axis_offset[2];
};

struct ns_graph_specifics{

	ns_graph_specifics():number_of_x_major_ticks(5),
						 number_of_y_major_ticks(5),
						 number_of_x_minor_ticks(40),
						 number_of_y_minor_ticks(40),
						 boundary(20,40),
						 aspect_ratio(1.0){};
	double dx,dy;
	ns_graph_axes axes;

	ns_vector_2i boundary;
	unsigned int x_axis_pos;
	int global_independant_variable_id;

	unsigned int number_of_x_major_ticks,
				 number_of_y_major_ticks,
				 number_of_x_minor_ticks,
				 number_of_y_minor_ticks;

	double		 major_x_tick,
				 minor_x_tick,
				 major_y_tick,
				 minor_y_tick;
	double aspect_ratio;  // width/height
};


///ns_graph takes a collection of ns_graph_objects and plots them on a 2D graph.
///The user should fill the contents std::vector with all desired graph objects, and then
///call draw().
///Frequency distribution graphs of data series can be automatically generated by
///suppling the data series to add_fequency_distribution()
class ns_graph{
public:
	ns_graph(){
		ns_graph_properties defaults;
		title_properties.text_size = defaults.text_size*2;
		set_graph_display_options();
		stored_contents.reserve(10);
	}
	//returns the index of the global independant variable, if found.
	int  check_input_data();

	///generates a graph of all data specified in contents
	ns_graph_specifics draw(ns_image_standard & image);
	void draw(std::string & svg_output);
	void draw(std::ostream & svg_output);
	void draw(ns_svg & svg_output);
	void clear(){
		axes = ns_graph_axes();
		y_axis_properties=
		x_axis_properties=
		area_properties=
		title_properties = ns_graph_properties();
		title.clear();
		stored_contents.clear();
		contents.clear();
	}
	///automatically generates a frequency distribution of the specified data
	ns_graph_axes add_frequency_distribution(const ns_graph_object & in, const bool crop_outliers=true);
	///automatically generates a frequency distribution of the specified data
	///The integral of all frequency distributions add up to the normalization value (1.0 if not specified)
	ns_graph_axes add_frequency_distribution(const std::vector<const ns_graph_object *> & in,const std::vector<double> & normalization=std::vector<double>(), const bool crop_outliers=true);

	void set_graph_display_options(const std::string & title_="",const ns_graph_axes & specified_axes_ = ns_graph_axes(), const double aspect_ratio_ = 1.618){
		title = title_;
		axes = specified_axes_;
		aspect_ratio = aspect_ratio_;
	}

	void concatenate(const ns_graph & graph){}


	void add_and_store(const ns_graph_object & a) {

		stored_contents.push_back(a);
		contents.push_back(&(*stored_contents.rbegin()));
	}	
	void add_reference(ns_graph_object * a) {
		contents.push_back(a);
	}
	ns_graph_properties y_axis_properties,
						x_axis_properties,
						area_properties,
						title_properties;

	std::vector<ns_graph_object *> contents;
	private:
	//references to all objects being plotted
	//if the user wants, graph data can be stored
	std::vector<ns_graph_object> stored_contents;
	void plot_object(const ns_graph_object & y, const ns_graph_object & x,ns_svg & svg, const ns_graph_specifics & spec);
	///plot_component takes two graph objects,
	///"y" specifies the object that contains the dependant variable
	///"x" specifies the object that contains the independant variable
	void plot_object(const ns_graph_object & y, const ns_graph_object & x, ns_image_standard & image, const ns_graph_specifics & spec);

	void calculate_graph_specifics(const unsigned int width, const unsigned int height, ns_graph_specifics & s, const ns_graph_axes & specified_axes);

	bool contains_data(){
		for (unsigned int i = 0; i < contents.size(); i++){
			if (contents[i]->type == ns_graph_object::ns_graph_independant_variable && contents[i]->y.size() != 0 ||
				contents[i]->type == ns_graph_object::ns_graph_dependant_variable && contents[i]->y.size() != 0)
				return true;
		}
		return false;
	}
	
	std::string title;
	ns_graph_axes axes;
	double aspect_ratio;
};

#endif
