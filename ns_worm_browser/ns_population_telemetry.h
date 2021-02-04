#pragma once
#include "ns_graph.h"
#include "ns_survival_curve.h"
#include "ns_death_time_annotation.h"
#include "ns_vector.h"
#include <random>
#include "ns_gmm.h"




struct ns_outlier_search_data {
	ns_outlier_search_data() {}
	ns_outlier_search_data(const double& w, const double& a) :weak_movement_duration(w), alive_but_not_moving_duration(a),by_hand_annotation_exists(false), is_an_outlier(false){}
	double weak_movement_duration,
		alive_but_not_moving_duration;
	bool by_hand_annotation_exists;
	bool is_an_outlier;
	ns_outlier_search_data(const ns_dying_animal_description_base<const ns_death_time_annotation>& source_data, int number_of_by_hand_specified_worms_at_position) {
		const ns_death_time_annotation* d =
			ns_dying_animal_description_group<const ns_death_time_annotation>::calculate_best_guess_death_annotation(source_data.by_hand.movement_based_death_annotation, source_data.by_hand.death_associated_expansion_start);
		by_hand_annotation_exists = d != 0 && !d->time.fully_unbounded() ||
			source_data.by_hand.last_fast_movement_annotation != 0 && !source_data.by_hand.last_fast_movement_annotation->time.fully_unbounded() ||
			number_of_by_hand_specified_worms_at_position != 1;

		if (source_data.machine.last_fast_movement_annotation == 0 || 
			source_data.machine.movement_based_death_annotation == 0) {
			weak_movement_duration = alive_but_not_moving_duration = -1;
			return;
		}
		else weak_movement_duration = source_data.machine.movement_based_death_annotation->time.best_estimate_event_time_for_possible_partially_unbounded_interval() - 
			source_data.machine.last_fast_movement_annotation->time.best_estimate_event_time_for_possible_partially_unbounded_interval();
		if (source_data.machine.death_associated_expansion_start == 0) {
			alive_but_not_moving_duration = -1;
			return;
		}
		else alive_but_not_moving_duration = source_data.machine.death_associated_expansion_start->time.best_estimate_event_time_for_possible_partially_unbounded_interval() 
			- source_data.machine.movement_based_death_annotation->time.best_estimate_event_time_for_possible_partially_unbounded_interval();
	}
};



struct ns_worm_lookup_info {
	ns_worm_lookup_info() {}
	ns_worm_lookup_info(const ns_64_bit& r, const ns_stationary_path_id& i):region_id(r),id(i) {}
	ns_64_bit region_id;
	ns_stationary_path_id id;
};
struct ns_lookup_index {
	ns_lookup_index(const double mt, const double dt) :movement_time(mt), death_associated_expansion_time(dt) {}
	double movement_time, death_associated_expansion_time;
};
bool operator <(const ns_lookup_index& a, const ns_lookup_index& b);

struct ns_scatter_plot_element {
	ns_scatter_plot_element() :x_specified(true), y_specified(true) {}
	double x_raw, y_raw;
	double x_to_plot, y_to_plot;
	double x_specified, y_specified;
	ns_64_bit region_info_id;
	ns_stationary_path_id stationary_path_id;
	ns_outlier_search_data outlier_data;
};


struct ns_measurement_accessor {
	typedef ns_scatter_plot_element data_t;
	virtual const double operator()(const ns_scatter_plot_element& e, bool& valid_data_point) const = 0;
	virtual ns_measurement_accessor* clone() = 0;
};

struct ns_weak_movement_accessor : public ns_measurement_accessor {
	typedef ns_outlier_search_data data_t;
	const double operator()(const ns_scatter_plot_element& e, bool& valid_data_point) const { valid_data_point = e.outlier_data.weak_movement_duration != -1;  return e.outlier_data.weak_movement_duration; }
	ns_weak_movement_accessor* clone() { return new ns_weak_movement_accessor; }
};
struct ns_alive_but_non_moving_movement_accessor : ns_measurement_accessor {
	typedef ns_outlier_search_data data_t;
	const double operator()(const ns_scatter_plot_element& e, bool & valid_data_point) const { valid_data_point = e.outlier_data.alive_but_not_moving_duration != -1; return e.outlier_data.alive_but_not_moving_duration; }
	ns_weak_movement_accessor* clone() { return new ns_weak_movement_accessor; }
};

template<class measurement_accessor_t>
unsigned long ns_emission_probabiliy_gaussian_diagonal_covariance_model<measurement_accessor_t>::training_data_buffer_size = 0;

template<class measurement_accessor_t>
double* ns_emission_probabiliy_gaussian_diagonal_covariance_model<measurement_accessor_t>::training_data_buffer = 0;

template<class measurement_accessor_t>
ns_lock ns_emission_probabiliy_gaussian_diagonal_covariance_model<measurement_accessor_t>::training_data_buffer_lock("tbl");

struct ns_survival_graph {
	ns_survival_graph() :vals(ns_graph_object::ns_graph_dependant_variable) {}
	ns_graph_object vals;
	std::string name;
	ns_color_8 color;
};

struct ns_survival_plot_data_grouping {
	ns_survival_plot_data_grouping() {}
	ns_survival_plot_data_grouping(const ns_64_bit& r, const std::string& s, const ns_color_8& c):output_location_id(r),group_name(s),color(c){	}
	ns_64_bit output_location_id;
	std::string group_name;
	ns_color_8 color;
};
struct ns_survival_telemetry_set{
	ns_survival_data_with_censoring 
		fast_movement_cessation,
		best_guess_survival,
		movement_survival,
		death_associated_expansion_survival;
	std::vector <unsigned long> time_axis;
};
struct ns_survival_telemetry_cache {
	std::vector< ns_survival_telemetry_set> s;
};

//first is annotation for x axis, second is annotation for y axis
struct ns_scatter_plot_coordinate {
	ns_scatter_plot_coordinate() {}
	ns_scatter_plot_coordinate(const ns_death_time_annotation* a) :using_value(false), annotation(a) {}
	ns_scatter_plot_coordinate(const double& a) :using_value(true), value(a) {}
	const ns_death_time_annotation* annotation;
	double value;
	bool using_value;
};

struct ns_plot_pair {
	ns_plot_pair() {}
	ns_plot_pair(const ns_scatter_plot_coordinate& a, const ns_scatter_plot_coordinate& b, ns_outlier_search_data & outlier_d) :
		first(a), second(b),outlier_data(outlier_d) {}
	ns_scatter_plot_coordinate first;
	ns_scatter_plot_coordinate second;
	ns_outlier_search_data outlier_data;
};

class ns_population_telemetry {
public:
	typedef enum { ns_none, ns_survival, ns_movement_vs_posture, ns_number_of_graph_types } ns_graph_contents;
	typedef enum { ns_group_by_device, ns_group_by_strain, ns_aggregate_all, ns_group_by_death_type, ns_survival_grouping_num} ns_survival_grouping;
	typedef enum { ns_plot_death_times_absolute, ns_plot_death_times_residual,ns_movement_plot_num } ns_movement_plot_type;
	typedef enum { ns_plot_best_guess, ns_plot_expansion_death, ns_plot_movement_death, ns_plot_fast_movement_death,ns_death_plot_num } ns_death_plot_type;
	typedef enum { ns_death_vs_observation_duration,ns_vigorous_vs_death, ns_movement_vs_expansion, ns_by_hand_machine,ns_regression_type_num } ns_regression_type;
	typedef enum {ns_plot_all,ns_plot_by_hand,ns_plot_machine,ns_plot_by_hand_num} ns_plot_by_hand_type;
	static std::string plot_by_hand_label(const ns_plot_by_hand_type& g) {
		switch (g) {
		case ns_plot_all:
			return "Show all";
		case ns_plot_by_hand:
			return "Show only by-hand annotated";
		case ns_plot_machine:
			return "Show only un-annotated";
		default: throw ns_ex("Unknown grouping type");
		}
	}
	static ns_plot_by_hand_type ns_plot_by_hand_type_from_label(const std::string& s) {
		for (unsigned int i = 0; i < (int)ns_survival_grouping_num; i++) {
			if (s == plot_by_hand_label((ns_plot_by_hand_type)i))
				return (ns_plot_by_hand_type)i;
		}
		throw ns_ex("Unknown grouping type:") << s;
	}

	static std::string survival_grouping_name(const ns_survival_grouping& g) {
		switch (g) {
		case ns_group_by_strain:
			return "By Strain";
		case ns_aggregate_all:
			return "All together";
		case ns_group_by_death_type:
			return "By Measurement Type";
		case ns_group_by_device:
			return "By Device";
		default: throw ns_ex("Unknown grouping type");
		}
	}
	static std::string death_plot_name(const ns_death_plot_type& g) {
		switch (g) {
		case ns_plot_best_guess:
			return "Best Estimate";
		case ns_plot_movement_death:
			return "Movement";
		case ns_plot_expansion_death:
			return "Expansion";
		case ns_plot_fast_movement_death:
			return "Quick movement";
		default: throw ns_ex("Unknown death type");
		}
	}
	static ns_color_8 death_type_color(const ns_death_plot_type& g) {
		switch (g) {
		case ns_plot_best_guess:
			return ns_color_8(0, 0, 0);
		case ns_plot_movement_death:
			return ns_color_8(255, 0, 0);
		case ns_plot_expansion_death:
			return ns_color_8(0, 0, 255);
		case ns_plot_fast_movement_death:
			return ns_color_8(0, 255, 0);
		default: throw ns_ex("Unknown death type");
		}
	}
	static std::string movement_plot_name(const ns_movement_plot_type& g) {
		switch (g) {
		case ns_plot_death_times_absolute:
			return "Values";
		case ns_plot_death_times_residual:
			return "Residuals";
		default: throw ns_ex("Unknown movement plot type");
		}
	}static std::string regression_type_name(const ns_regression_type& g) {
		switch (g) {
		case ns_vigorous_vs_death:
			return "Vigorous Movement Cessation vs. Death";
		case ns_movement_vs_expansion:
			return "Weak Movement Cessation vs. Expansion";
		case ns_by_hand_machine:
			return "By hand vs Machine";
		case ns_death_vs_observation_duration:
			return "Death vs. Observation";
		default: throw ns_ex("Unknown movement plot type");
		}
	}
	static ns_survival_grouping survival_grouping_type(const std::string& s) {
		for (unsigned int i = 0; i < (int)ns_survival_grouping_num; i++) {
			if (s == survival_grouping_name((ns_survival_grouping)i))
				return (ns_survival_grouping)i;
		}
		throw ns_ex("Unknown grouping type:") << s;
	}
	static ns_death_plot_type death_plot_type(const std::string& s) {
		for (unsigned int i = 0; i < (int)ns_death_plot_num; i++) {
			if (s == death_plot_name((ns_death_plot_type)i))
				return (ns_death_plot_type)i;
		}
		throw ns_ex("Unknown death type:") << s;
	}
	static ns_movement_plot_type movement_plot_type(const std::string& s) {
		for (unsigned int i = 0; i < (int)ns_movement_plot_num; i++) {
			if (s == movement_plot_name((ns_movement_plot_type)i))
				return (ns_movement_plot_type)i;
		}
		throw ns_ex("Unknown death type:") << s;
	}	
	static ns_regression_type regression_plot_type(const std::string& s) {
		for (unsigned int i = 0; i < (int)ns_regression_type_num; i++) {
			if (s == regression_type_name((ns_regression_type)i))
				return (ns_regression_type)i;
		}
		throw ns_ex("Unknown death type:") << s;
	}
	
//	void get_current_time_limits(unsigned long& start, unsigned long& stop) {
//
	//}
	ns_vector_2i border() const { return ns_vector_2i(10, 10); }
	ns_survival_grouping survival_grouping;
	ns_plot_by_hand_type by_hand_plot;
	ns_death_plot_type death_plot;
	ns_movement_plot_type movement_plot;
	ns_regression_type regression_plot;
	void plot_death_time_expansion(bool plot) {
		plot_death_time_expansion_ = false;
	}

	void load_last_measurement_cache(const ns_64_bit& experiment_id, const ns_64_bit& region_id, ns_sql& sql) {
		last_measurement_cache.clear();
		if (region_id != 0)
			sql << "SELECT region_info_id, MAX(capture_time) FROM sample_region_images WHERE region_info_id =" << region_id;
		else sql << "SELECT r.region_info_id, MAX(r.capture_time) FROM sample_region_images as r, sample_region_image_info as ri, capture_samples as s WHERE r.region_info_id = ri.id AND ri.sample_id = s.id AND s.experiment_id = " << experiment_id << " GROUP BY r.region_info_id";
		ns_sql_result res;
		sql.get_rows(res);
		for (unsigned int i = 0; i < res.size(); i++)
			last_measurement_cache[ns_atoi64(res[i][0].c_str())] = atol(res[i][1].c_str());
	}

private:

	bool plot_death_time_expansion_;
	bool _show;
	unsigned long group_id;
	ns_image_standard survival_image,survival_image_legend, movement_vs_posture_image, movement_vs_posture_image_legend;
	ns_graph survival,movement_vs_posture;
	ns_graph_specifics survival_specifics, movement_vs_posture_specifics;
	std::vector<ns_survival_graph> survival_curves;
	std::vector<ns_survival_graph> movement_vs_posture_vals_fully_specified;
	std::vector<ns_survival_graph> movement_vs_posture_vals_by_hand_annotated;
	std::vector<ns_survival_graph> movement_vs_posture_vals_not_fully_specified;
	std::vector<ns_survival_graph> movement_vs_posture_vals_outliers;
	std::string survival_curve_title, survival_curve_note, movement_vs_posture_title, movement_vs_posture_note;
	std::string movement_vs_posture_x_axis_label, movement_vs_posture_y_axis_label;
	ns_color_8 movement_vs_posture_x_axis_color, movement_vs_posture_y_axis_color;
	ns_graph_object unity_line, undefined_x_line, undefined_y_line;
	unsigned long time_at_which_animals_were_age_zero;
	double undefined_x_position, undefined_y_position;
	std::vector<double> temp1, temp2;

	std::map<ns_64_bit, unsigned long> last_measurement_cache; //last measurement made in each region, sorted by region_info_id;

	typedef std::tuple< ns_survival_grouping, ns_death_plot_type, ns_64_bit, std::string> ns_telemetry_cache_index;
	typedef std::map<ns_telemetry_cache_index, ns_survival_telemetry_cache> ns_telemetry_cache;
	ns_telemetry_cache telemetry_cache;

	std::map<ns_lookup_index, std::vector<ns_worm_lookup_info> > movement_id_lookup;

	unsigned long number_of_valid_elements, first_element;


	void draw_base_graph(const ns_graph_contents& graph_contents, const long marker_resize_factor, unsigned long start_time = 0, unsigned long stop_time = UINT_MAX) {
		survival_image.use_more_memory_to_avoid_reallocations();
		movement_vs_posture_image.use_more_memory_to_avoid_reallocations();

		if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Drawing population telemetry base graph."));
		if (graph_contents == ns_none)
			return;
		survival.clear();
		movement_vs_posture.clear();



		for (unsigned int i = 0; i < survival_curves.size(); i++) {
			bool death_observed(false);
			for (unsigned int j = 0; j < survival_curves[i].vals.y.size(); j++)
				if (survival_curves[i].vals.y[j] < 1) {
					death_observed = true;
					break;
				}
			if (!death_observed)
				continue;
			survival_curves[i].vals.type = ns_graph_object::ns_graph_dependant_variable;
			survival_curves[i].vals.properties.line_hold_order = ns_graph_properties::ns_zeroth;
			survival_curves[i].vals.properties.draw_vertical_lines = ns_graph_properties::ns_outline;
			survival_curves[i].vals.properties.line.draw = true;
			survival_curves[i].vals.properties.line.color = survival_curves[i].color;
			survival_curves[i].vals.properties.line.width = 2;
			survival_curves[i].vals.properties.point.draw = false;
			survival_curves[i].vals.properties.draw_negatives = false;
			survival_curves[i].vals.data_label = survival_curves[i].name;
			survival.add_reference(&survival_curves[i].vals);
		}
		survival.x_axis_label = "Age (days)";
		survival.y_axis_label = "Fraction Alive";


		ns_graph_axes axes;
		axes.boundary(2) = 0;
		axes.boundary(3) = 1;
		survival.x_axis_properties.text_decimal_places = 1;
		survival.y_axis_properties.text_decimal_places = 1;
		survival.set_graph_display_options("", axes, survival_image.properties().width / (float)survival_image.properties().height);

		if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Rendering survival graph"));
		survival_specifics = survival.draw(survival_image);
		survival.draw_legend(survival_curve_title,20,true,survival_image_legend);

		if (graph_contents != ns_movement_vs_posture)
			return;
		if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Rendering movement v posture graph"));

		double tmax(0), tmin(DBL_MAX), ymax(0), ymin(DBL_MAX);
		unsigned long number_of_points(0);
		const int number_of_movement_vs_posture_graphs(4);
		std::vector<ns_survival_graph>* all_movement_vs_posture_graphs[number_of_movement_vs_posture_graphs] = { &movement_vs_posture_vals_fully_specified, &movement_vs_posture_vals_not_fully_specified, &movement_vs_posture_vals_outliers, &movement_vs_posture_vals_by_hand_annotated };
		for (unsigned int g = 0; g < number_of_movement_vs_posture_graphs; g++) {
			std::vector<ns_survival_graph>& graph(*all_movement_vs_posture_graphs[g]);
			for (unsigned int j = 0; j < graph.size(); j++) {
				for (unsigned int i = 0; i < graph[j].vals.x.size(); i++) {
					number_of_points++;
					if (graph[j].vals.x[i] < tmin)
						tmin = graph[j].vals.x[i];
					if (movement_plot == ns_plot_death_times_absolute && graph[j].vals.y[i] < tmin)
						tmin = graph[j].vals.y[i];
					if (graph[j].vals.y[i] < ymin)
						ymin = graph[j].vals.y[i];
					if (graph[j].vals.x[i] > tmax)
						tmax = graph[j].vals.x[i];
					if (movement_plot == ns_plot_death_times_absolute && graph[j].vals.y[i] > tmax)
						tmax = graph[j].vals.y[i];
					if (graph[j].vals.y[i] > ymax)
						ymax = graph[j].vals.y[i];
				}
			}
		}
		if (number_of_points != 0) {
			unity_line.x.resize(2);
			unity_line.y.resize(2);
			if (movement_plot == ns_plot_death_times_absolute) {
				unity_line.x[0] = unity_line.y[0] = tmin;
				unity_line.x[1] = unity_line.y[1] = tmax;
			}
			else {
				if (ymax < 0)
					ymax = 0;
				if (ymin > 0)
					ymin = 0;
				unity_line.y[0] = unity_line.y[1] = 0;
				unity_line.x[0] = tmin;
				unity_line.x[1] = tmax;

			}
			unity_line.properties.line.draw = true;
			unity_line.properties.line.color = ns_color_8(100, 100, 100);
			unity_line.properties.line.width = 2;
			unity_line.properties.point.draw = false;
			undefined_x_line = unity_line;
			undefined_x_line.x[0] = undefined_x_line.x[1] = undefined_x_position;
			undefined_y_line = unity_line;
			undefined_y_line.y[0] = undefined_y_line.y[1] = undefined_y_position;
			movement_vs_posture.add_reference(&unity_line);
			//movement_vs_posture.add_reference(&undefined_x_line);
			//movement_vs_posture.add_reference(&undefined_y_line);
		}

		movement_vs_posture.x_axis_properties.text_decimal_places = 1;
		movement_vs_posture.y_axis_properties.text_decimal_places = 2;
		movement_vs_posture.x_axis_properties.text.color = ns_color_8(255, 0, 0);
		movement_vs_posture.x_axis_label = movement_vs_posture_x_axis_label;
		movement_vs_posture.y_axis_properties.text.color = ns_color_8(0, 0, 255);
		movement_vs_posture.y_axis_label = movement_vs_posture_y_axis_label;
		for (unsigned int g = 0; g < number_of_movement_vs_posture_graphs; g++) {
			for (unsigned int i = 0; i < all_movement_vs_posture_graphs[g]->size(); i++) {
				(*all_movement_vs_posture_graphs[g])[i].vals.type = ns_graph_object::ns_graph_dependant_variable;
				(*all_movement_vs_posture_graphs[g])[i].vals.properties.line.draw = false;
				(*all_movement_vs_posture_graphs[g])[i].vals.properties.point.draw = true;
				if (g==0)
					(*all_movement_vs_posture_graphs[g])[i].vals.data_label = (*all_movement_vs_posture_graphs[g])[i].name;

				(*all_movement_vs_posture_graphs[g])[i].vals.properties.point.color = (*all_movement_vs_posture_graphs[g])[i].color;
				(*all_movement_vs_posture_graphs[g])[i].vals.properties.point.width = 4;
				(*all_movement_vs_posture_graphs[g])[i].vals.properties.point.edge_color = ns_color_8(0, 0, 0);
				(*all_movement_vs_posture_graphs[g])[i].vals.properties.point.edge_width = 1;
				(*all_movement_vs_posture_graphs[g])[i].vals.properties.point.point_shape = ns_graph_color_set::ns_circle;

				movement_vs_posture.add_reference(&(*all_movement_vs_posture_graphs[g])[i].vals);
			}
		}
		for (unsigned int i = 0; i < movement_vs_posture_vals_not_fully_specified.size(); i++) 
			movement_vs_posture_vals_not_fully_specified[i].vals.properties.point.edge_color = ns_color_8(125, 125, 125);

		for (unsigned int i = 0; i < movement_vs_posture_vals_outliers.size(); i++) {
			movement_vs_posture_vals_outliers[i].vals.properties.point.edge_color = movement_vs_posture_vals_not_fully_specified[i].color;
			movement_vs_posture_vals_outliers[i].vals.properties.point.edge_width = 2;
			movement_vs_posture_vals_outliers[i].vals.properties.point.color = ns_color_8(255, 255, 255);
			movement_vs_posture_vals_outliers[i].vals.properties.point.width = 3;
		}
		for (unsigned int i = 0; i < movement_vs_posture_vals_by_hand_annotated.size(); i++) {
			movement_vs_posture_vals_by_hand_annotated[i].vals.properties.point.point_shape = ns_graph_color_set::ns_square;
			movement_vs_posture_vals_by_hand_annotated[i].vals.properties.point.width = 3;
		}


		ns_graph_axes axes2;
		if (movement_plot == ns_plot_death_times_absolute) {
			axes2.boundary(0) = axes2.boundary(2) = min(tmin * .95, tmin * 1.05);	//could be negative
			axes2.boundary(1) = axes2.boundary(3) = max(tmax * .95, tmax * 1.05);
		}
		else {
			axes2.boundary(0) = tmin * .95;
			axes2.boundary(1) = tmax * 1.05;
			axes2.boundary(2) = min(ymin*.95, ymin * 1.05);	//could be negative
			axes2.boundary(3) = max(ymax*.95,ymax * 1.05);
		}

		movement_vs_posture.set_graph_display_options("", axes2, (1.1*movement_vs_posture_image.properties().width) / (float)movement_vs_posture_image.properties().height);
		movement_vs_posture_specifics = movement_vs_posture.draw(movement_vs_posture_image);
		movement_vs_posture.draw_legend(movement_vs_posture_title, 20, true, movement_vs_posture_image_legend);
	
	}
	inline void map_value_from_survival_onto_image(const float& x, const float& y, unsigned long& x1, unsigned long& y1) {
		x1 = survival_specifics.boundary_bottom_and_left.x + (unsigned int)(survival_specifics.dx * (x - survival_specifics.axes.boundary(0) + survival_specifics.axes.axis_offset(0)));
		y1 = survival_image.properties().height - survival_specifics.boundary_bottom_and_left.y - (unsigned int)(survival_specifics.dy * (y - survival_specifics.axes.boundary(2) + survival_specifics.axes.axis_offset(1)));
	}

	inline void map_value_from_movement_vs_survival_onto_image(const float& x, const float& y, unsigned long& x1, unsigned long& y1) {
		x1 = survival_image.properties().width + border().x + movement_vs_posture_specifics.boundary_bottom_and_left.x + (unsigned int)(movement_vs_posture_specifics.dx * (x - movement_vs_posture_specifics.axes.boundary(0) + movement_vs_posture_specifics.axes.axis_offset(0)));
		y1 = movement_vs_posture_image.properties().height - movement_vs_posture_specifics.boundary_bottom_and_left.y - (unsigned int)(movement_vs_posture_specifics.dy * (y - movement_vs_posture_specifics.axes.boundary(2) + movement_vs_posture_specifics.axes.axis_offset(1)));
	}
	inline unsigned long map_pixel_from_image_onto_buffer(const unsigned long& x, const unsigned long& y, const ns_vector_2i& position, const ns_vector_2i& buffer_size) {
		return 4 * ((buffer_size.y - y - position.y - 1) * buffer_size.x + x + position.x);
	}
	void overlay_metadata(const ns_population_telemetry::ns_graph_contents graph_contents, const ns_vector_2i& position, const ns_vector_2i& buffer_size, const long marker_resize_factor, ns_tiled_gl_image & buffer) {

	}
	ns_graph_contents last_graph_contents;
	unsigned long last_start_time, last_stop_time;
	ns_vector_2i last_graph_size;
	float last_rescale_factor;
	ns_64_bit subject_region;
	ns_region_metadata stats_subset_strain_to_display;
public:
	void clear() {
		_show = false;
		movement_id_lookup.clear();
		survival_image.clear();
		movement_vs_posture_image.clear();
		survival.clear();
		movement_vs_posture.clear();
		survival_curves.clear();
		movement_vs_posture_vals_fully_specified.clear();
		movement_vs_posture_vals_not_fully_specified.clear();
		movement_vs_posture_vals_by_hand_annotated.clear();
		movement_vs_posture_vals_outliers.clear();
		unity_line.clear();
		last_graph_contents = ns_none;
		last_start_time = 0;
		last_stop_time = 0;
		subject_region = 0;
		stats_subset_strain_to_display.clear();
		//last_measurement_cache.clear();
	}
	unsigned long get_graph_time_from_graph_position(const float x) { //x is in relative time
	  return 0;
	  //xxx not done
		/*ns_analyzed_image_time_path* path(&region_data->movement_analyzer.group(group_id).paths[0]);
		long min_i(0);
		float min_dt(100000);
		unsigned long x_seconds = x * 60 * 60 * 24 + path->element(0).absolute_time;
		for (unsigned int i = 0; i < path->element_count(); i++) {
			const bool g(path->element(i).absolute_time >= x_seconds);
			const float dt = (path->element(i).absolute_time >= x_seconds) ? (path->element(i).absolute_time - x_seconds) : (x_seconds - path->element(i).absolute_time);
			if (dt < min_dt) {
				min_dt = dt;
				min_i = i;
			}
			if (g) break;
		}
		return path->element(min_i).absolute_time;*/

	}
	//currently does not return a correct y value.
	ns_vector_2d get_graph_value_from_click_position_(const unsigned long& x, const unsigned long& y, ns_graph_contents& graph_selected) const {
		const unsigned long permissible_time_error(12 * 60 * 60);
		ns_vector_2d res;
		const int movement_vs_posture_offset = survival_image.properties().width+survival_image_legend.properties().width + 2 * border().x;
		if (x <= movement_vs_posture_offset) {
			graph_selected = ns_survival;
			const unsigned long permissable_error(permissible_time_error * survival_specifics.dx); //how far away the user can click from the exact right position
			res.x = ((long)x - (long)survival_specifics.boundary_bottom_and_left.x - (long)border().x) / (survival_specifics.dx) + survival_specifics.axes.boundary(0) - survival_specifics.axes.axis_offset(0);
			if (x + permissable_error < border().x + survival_specifics.boundary_bottom_and_left.x || x - border().x >= survival_image.properties().width - survival_specifics.boundary_bottom_and_left.x+ permissable_error) {
				res.x = 0;
				graph_selected = ns_none;
			}
			res.y = ((long)survival_image.properties().height - survival_specifics.boundary_bottom_and_left.y - (y - border().y)) / survival_specifics.dy + survival_specifics.axes.boundary(2) - survival_specifics.axes.axis_offset(1);
			if (y +permissable_error < border().y + survival_specifics.boundary_bottom_and_left.y || y - border().y >= survival_image.properties().height - survival_specifics.boundary_bottom_and_left.y+permissable_error) {
				res.y = 0;
				graph_selected = ns_none;
			}

		}
		else {
			graph_selected = ns_movement_vs_posture;
			const unsigned long permissable_error(permissible_time_error * movement_vs_posture_specifics.dx);
			res.x = ((long)x - movement_vs_posture_offset - (long)movement_vs_posture_specifics.boundary_bottom_and_left.x) / (movement_vs_posture_specifics.dx) + movement_vs_posture_specifics.axes.boundary(0) - movement_vs_posture_specifics.axes.axis_offset(0);
			if (x + permissable_error < movement_vs_posture_offset + movement_vs_posture_specifics.boundary_bottom_and_left.x || x - movement_vs_posture_offset >= movement_vs_posture_image.properties().width - movement_vs_posture_specifics.boundary_bottom_and_left.x+ permissable_error) {
				res.x = 0;
				graph_selected = ns_none;
			}
			res.y = ((long)movement_vs_posture_image.properties().height - movement_vs_posture_specifics.boundary_bottom_and_left.y - (y - border().y)) / movement_vs_posture_specifics.dy + movement_vs_posture_specifics.axes.boundary(2) - movement_vs_posture_specifics.axes.axis_offset(1);
			if (y + permissable_error < border().y + movement_vs_posture_specifics.boundary_bottom_and_left.y || y - border().y >= movement_vs_posture_image.properties().height - movement_vs_posture_specifics.boundary_bottom_and_left.y+ permissable_error) {
				res.y = 0;
				graph_selected = ns_none;
			}
		}
		//convert into absolute time units
		if (graph_selected == ns_survival) {
			res.y = round(res.x * 60 * 60 * 24 + time_at_which_animals_were_age_zero);
		}
		else if (graph_selected == ns_movement_vs_posture) {
			//cout << "Looking at " << res.x << " " << res.y << ": ";
			res.x = round(res.x * 60 * 60 * 24 + time_at_which_animals_were_age_zero);
			if (movement_plot == ns_plot_death_times_absolute && regression_plot != ns_death_vs_observation_duration)
				res.y = round(res.y * 60 * 60 * 24 + time_at_which_animals_were_age_zero);	//death associated expansion
			else res.y = round(res.y * 60 * 60 * 24);
			double min_d(DBL_MAX);
			auto info = movement_id_lookup.end();
			for (auto p = movement_id_lookup.begin(); p != movement_id_lookup.end(); p++) {
				double d = (ns_vector_2d(p->first.movement_time, p->first.death_associated_expansion_time) - res).squared();
				if (d < min_d) {
					min_d = d;
					info = p;
				}
			}
			//cout << "Min dist = " << sqrt(min_d) / 60 / 60 << "\n";
			if (sqrt(min_d) < permissible_time_error) { //less than 12 hours
				if (info->second.size() > 1) {
					cout << "The click matches multiple individuals:\n";
					for (unsigned int i = 0; i < info->second.size(); i++)
						cout << "Region " << info->second.begin()->region_id << ", worm" << info->second.begin()->id.group_id << "\n";
					cout << "Showing the first matching individual.\n";
				}
				return ns_vector_2d(info->second.begin()->region_id, info->second.begin()->id.group_id);
			}
			else {
				graph_selected = ns_none;
				return ns_vector_2d(0, 0);
			}
		}
		return res;
	}

	ns_population_telemetry() :_show(false), by_hand_plot(ns_plot_all),plot_death_time_expansion_(true),last_graph_contents(ns_none), movement_plot(ns_plot_death_times_absolute),survival_grouping((ns_survival_grouping)0),unity_line(ns_graph_object::ns_graph_dependant_variable),undefined_x_line(ns_graph_object::ns_graph_dependant_variable),undefined_y_line(ns_graph_object::ns_graph_dependant_variable) {
		survival_image.init(ns_image_properties(0, 0, 3));
		movement_vs_posture_image.init(ns_image_properties(0, 0, 3));
	}

	void set_subject(const ns_64_bit region_id, const ns_region_metadata& strain) {
		subject_region = region_id;
		stats_subset_strain_to_display = strain;

	}
	
	void clear_cache() {
		telemetry_cache.clear();
	}
	void update_annotations_and_build_survival(const ns_death_time_annotation_compiler& compiler, const ns_region_metadata& metadata,ns_sql & sql) {
		survival_curves.resize(0);
		ns_lifespan_experiment_set survival_curve_set;
		compiler.generate_survival_curve_set(survival_curve_set, ns_death_time_annotation::ns_machine_annotations_if_no_by_hand, true, false,sql);
		//ns_survival_data_with_censoring best_guess_survival,movement_based_survival,death_associated_expansion_survival;


		const bool strain_override_specified = stats_subset_strain_to_display.device_regression_match_description() != "";
		const bool viewing_a_storyboard_for_a_specific_strain = metadata.device_regression_match_description() != "";
		const bool subject_region_specified = subject_region != 0;
		const bool single_region_storyboard = compiler.regions.size() == 1;

		const ns_64_bit time_at_zero = metadata.time_at_which_animals_had_zero_age;
		const ns_64_bit storyboard_region_id = metadata.region_id;


		ns_region_metadata data_to_process;
		const bool filter_by_strain = strain_override_specified || viewing_a_storyboard_for_a_specific_strain;
		if (filter_by_strain) {
			if (viewing_a_storyboard_for_a_specific_strain)
				data_to_process = metadata;
			else data_to_process = stats_subset_strain_to_display;
			survival_curve_note = "(" + data_to_process.device_regression_match_description() + ")";
			movement_vs_posture_note = "(" + data_to_process.device_regression_match_description() + ")";
		}
		data_to_process.time_at_which_animals_had_zero_age = metadata.time_at_which_animals_had_zero_age;

		ns_64_bit region_to_view = 0;
		if (!storyboard_region_id && subject_region_specified)
			region_to_view = subject_region;

		//the category into which each data from each region is placed. 
		//indexed by region_id
		std::map<ns_64_bit, ns_survival_plot_data_grouping> data_categories;
		const ns_telemetry_cache::key_type aggregate_key(ns_aggregate_all, death_plot, region_to_view, data_to_process.device_regression_match_description());
		ns_telemetry_cache::iterator p = telemetry_cache.find(aggregate_key);
		if (p == telemetry_cache.end()) {
			p = telemetry_cache.insert(ns_telemetry_cache::value_type(aggregate_key, ns_survival_telemetry_cache())).first;
			p->second.s.resize(1);
			survival_curve_set.generate_aggregate_risk_timeseries(data_to_process, filter_by_strain, "", region_to_view, p->second.s[0].best_guess_survival, p->second.s[0].movement_survival, p->second.s[0].death_associated_expansion_survival,p->second.s[0].fast_movement_cessation, p->second.s[0].time_axis, false);
		}
		time_at_which_animals_were_age_zero = metadata.time_at_which_animals_had_zero_age;

		last_graph_contents = ns_none;
		if (p->second.s[0].time_axis.size() == 0)
			return;
		if (survival_grouping == ns_aggregate_all || strain_override_specified && survival_grouping == ns_group_by_strain) {
			//set up survival curve
			//const bool plot_movement(death_plot == ns_plot_movement_death);
			const ns_telemetry_cache::key_type key(survival_grouping, death_plot, region_to_view, data_to_process.device_regression_match_description());
			ns_telemetry_cache::iterator p = telemetry_cache.find(key);
			if (p == telemetry_cache.end()) {
				p = telemetry_cache.insert(ns_telemetry_cache::value_type(key, ns_survival_telemetry_cache())).first;
				p->second.s.resize(1);
				survival_curve_set.generate_aggregate_risk_timeseries(data_to_process, filter_by_strain, "", region_to_view, p->second.s[0].best_guess_survival, p->second.s[0].movement_survival, p->second.s[0].death_associated_expansion_survival, p->second.s[0].fast_movement_cessation, p->second.s[0].time_axis, false);
			}
			ns_survival_data_with_censoring_timeseries* survival_data;
			switch (death_plot) {
			case ns_plot_movement_death:
				survival_data = &p->second.s[0].movement_survival.data; break;
			case ns_plot_expansion_death:
				survival_data = &p->second.s[0].death_associated_expansion_survival.data; break;
			case ns_plot_best_guess:
				survival_data = &p->second.s[0].best_guess_survival.data; break;
			case ns_plot_fast_movement_death:
				survival_data = &p->second.s[0].fast_movement_cessation.data; break;
			default: throw ns_ex("Unknown death time type requested");
			}

			unsigned long max_t_i(0);
			survival_curve_title = movement_vs_posture_title = "All Individuals";
			survival_curves.resize(1);
			if (filter_by_strain)
				survival_curves[0].name = data_to_process.device_regression_match_description();
			else survival_curves[0].name = "All animal types";
			survival_curve_note = "";
			survival_curves[0].color = death_type_color(death_plot);

			//load survival data truncating at the last zero found in each group.
			survival_curves[0].vals.type = ns_graph_object::ns_graph_dependant_variable;
			survival_curves[0].vals.y.resize(survival_data->probability_of_surviving_up_to_interval.size());
			survival_curves[0].vals.x.resize(survival_data->probability_of_surviving_up_to_interval.size());
			bool plotted_zero(false);
			for (unsigned int i = 0; i < survival_data->probability_of_surviving_up_to_interval.size(); i++) {
				survival_curves[0].vals.x[i] = p->second.s[0].time_axis[i];
				if (survival_data->probability_of_surviving_up_to_interval[i] < .00001) {
					if (plotted_zero) {
						survival_curves[0].vals.y[i] = -1;
						continue;
					}
					plotted_zero = true;
				}
				max_t_i = i;
				survival_curves[0].vals.y[i] = survival_data->probability_of_surviving_up_to_interval[i];
			}
			/*if (max_t_i < p->second.s[0].time_axis.size()) {
				survival_curves[0].vals.y.resize(max_t_i);
				p->second.s[0].time_axis.resize(max_t_i);
			}*/
			//add info for grouping data on the scatter plot
			for (auto p = compiler.regions.begin(); p != compiler.regions.end(); p++)
				data_categories[p->first] = ns_survival_plot_data_grouping(p->first, survival_curves[0].name, survival_curves[0].color);
		}
		else if (survival_grouping == ns_group_by_death_type) {
			const ns_telemetry_cache::key_type key(survival_grouping, death_plot, region_to_view, data_to_process.device_regression_match_description());
			ns_telemetry_cache::iterator p = telemetry_cache.find(key);
			if (p == telemetry_cache.end()) {
				p = telemetry_cache.insert(ns_telemetry_cache::value_type(key, ns_survival_telemetry_cache())).first;
				p->second.s.resize(1);
				survival_curve_set.generate_aggregate_risk_timeseries(data_to_process, filter_by_strain, "", region_to_view, p->second.s[0].best_guess_survival, p->second.s[0].movement_survival, p->second.s[0].death_associated_expansion_survival, p->second.s[0].fast_movement_cessation, p->second.s[0].time_axis, false);
			}
			survival_curve_title = "Death Type";
			movement_vs_posture_title = "All Individuals";
			std::string group_label;
			if (filter_by_strain)
				group_label = data_to_process.device_regression_match_description();
			else group_label = "All animal types";
			unsigned long max_t_i(0);

			const int number_of_survival_curve_types(4);
			survival_curves.resize(number_of_survival_curve_types);
			survival_curves[0].name = "Best Estimate";
			survival_curves[0].color = ns_color_8(0, 0, 0);
			survival_curves[1].name = "Vigorous Movement";
			survival_curves[1].color = ns_color_8(0, 255, 0);
			survival_curves[2].name = "Weak Movement";
			survival_curves[2].color = ns_color_8(255, 0, 0);
			survival_curves[3].name = "Death-Associated Expansion";
			survival_curves[3].color = ns_color_8(0, 0, 255);


			//load survival data truncating at the last zero found in each group.
			ns_survival_data_with_censoring_timeseries* survival_data[number_of_survival_curve_types] = { &p->second.s[0].best_guess_survival.data,
																										&p->second.s[0].fast_movement_cessation.data,
																										&p->second.s[0].movement_survival.data,
																										&p->second.s[0].death_associated_expansion_survival.data };
			for (unsigned int j = 0; j < number_of_survival_curve_types; j++) {
				survival_curves[j].vals.type = ns_graph_object::ns_graph_dependant_variable;
				survival_curves[j].vals.y.resize(survival_data[j]->probability_of_surviving_up_to_interval.size());
				survival_curves[j].vals.x.resize(survival_data[j]->probability_of_surviving_up_to_interval.size());
				bool plotted_zero(false);
				for (unsigned int i = 0; i < survival_data[j]->probability_of_surviving_up_to_interval.size(); i++) {

					survival_curves[j].vals.x[i] = p->second.s[0].time_axis[i];
					if (survival_data[j]->probability_of_surviving_up_to_interval[i] < .000001) {
						if (plotted_zero) {
							survival_curves[j].vals.y[i] = -1;
							continue;
						}
						plotted_zero = true;
					}
					if (max_t_i < i)
						max_t_i = i;
					survival_curves[j].vals.y[i] = survival_data[j]->probability_of_surviving_up_to_interval[i];
				}
			}
			/*if (max_t_i < p->second.s[0].time_axis.size()) {
				survival_curves[0].vals.y.resize(max_t_i);
				survival_curves[1].vals.y.resize(max_t_i);
				p->second.s[0].time_axis.resize(max_t_i);
			}*/
			//add info for grouping data on the scatter plot
			for (auto p = compiler.regions.begin(); p != compiler.regions.end(); p++)
				data_categories[p->first] = ns_survival_plot_data_grouping(p->first, group_label, ns_color_8(0,0,0));
		}
		else if (survival_grouping == ns_group_by_strain) {
			survival_curve_title = movement_vs_posture_title = "Animal Type";
			std::map<std::string, std::pair<ns_region_metadata, ns_survival_plot_data_grouping> > all_strains;
			for (unsigned int i = 0; i < survival_curve_set.size(); i++)
				all_strains[survival_curve_set.curve(i).metadata.device_regression_match_description()] = std::pair<ns_region_metadata, ns_survival_plot_data_grouping>(survival_curve_set.curve(i).metadata, ns_survival_plot_data_grouping());

			survival_curves.resize(all_strains.size());

			for (auto p = compiler.regions.begin(); p != compiler.regions.end(); p++)
				data_categories[p->first] = ns_survival_plot_data_grouping(p->first, p->second.metadata.device_regression_match_description(), survival_curves[0].color);
			const ns_telemetry_cache::key_type key(survival_grouping, death_plot, region_to_view, data_to_process.device_regression_match_description());
			ns_telemetry_cache::iterator tel = telemetry_cache.find(key);
			if (tel == telemetry_cache.end()) {
				tel = telemetry_cache.insert(ns_telemetry_cache::value_type(key, ns_survival_telemetry_cache())).first;
				tel->second.s.resize(all_strains.size());
				unsigned int i = 0;
				for (auto p = all_strains.begin(); p != all_strains.end(); p++) {
					survival_curve_set.generate_aggregate_risk_timeseries(p->second.first, true, "", region_to_view, tel->second.s[i].best_guess_survival, tel->second.s[i].movement_survival, tel->second.s[i].death_associated_expansion_survival, tel->second.s[i].fast_movement_cessation, tel->second.s[i].time_axis, false);
					i++;
				}
			}
			unsigned long max_t_i(0);
			{
				unsigned long survival_curve_id = 0;
				for (auto p = all_strains.begin(); p != all_strains.end(); p++) {

					ns_survival_data_with_censoring_timeseries* survival_data;
					switch (death_plot) {
					case ns_plot_movement_death:
						survival_data = &tel->second.s[survival_curve_id].movement_survival.data; break;
					case ns_plot_expansion_death:
						survival_data = &tel->second.s[survival_curve_id].death_associated_expansion_survival.data; break;
					case ns_plot_best_guess:
						survival_data = &tel->second.s[survival_curve_id].best_guess_survival.data; break;
					case ns_plot_fast_movement_death:
						survival_data = &tel->second.s[survival_curve_id].fast_movement_cessation.data; break;
					default: throw ns_ex("Unknown death time type requested");
					}

					survival_curves[survival_curve_id].vals.type = ns_graph_object::ns_graph_dependant_variable;
					survival_curves[survival_curve_id].vals.y.resize(survival_data->probability_of_surviving_up_to_interval.size());
					survival_curves[survival_curve_id].vals.x.resize(survival_data->probability_of_surviving_up_to_interval.size());

					bool plotted_zero(false);
					for (unsigned int i = 0; i < survival_data->probability_of_surviving_up_to_interval.size(); i++) {
						survival_curves[survival_curve_id].vals.x[i] = tel->second.s[survival_curve_id].time_axis[i];
						if (survival_data->probability_of_surviving_up_to_interval[i] < .00001) {
							if (plotted_zero) {
								survival_curves[survival_curve_id].vals.y[i] = -1;
								continue;
							}
							plotted_zero = true;
						}
						if (max_t_i < i)
							max_t_i = i;
						survival_curves[survival_curve_id].vals.y[i] = survival_data->probability_of_surviving_up_to_interval[i];
					}

					survival_curves[survival_curve_id].color = ns_rainbow<ns_color_8>(survival_curve_id / (float)all_strains.size());
					survival_curves[survival_curve_id].name = p->first;
					//prep lookup table for grouping scatter plot
					p->second.second.color = survival_curves[survival_curve_id].color;
					p->second.second.group_name = survival_curves[survival_curve_id].name = p->first;
					survival_curve_id++;
				}
			}
			//do not include any times in any survival curve after the last death in the experiment
			/*for (unsigned int i = 0; i < survival_curves.size(); i++) {
				if (max_t_i < survival_curves[i].vals.y.size()) {
					survival_curves[i].vals.y.resize(max_t_i);
					survival_curves[i].vals.x.resize(max_t_i);
					tel->second.s[i].time_axis.resize(max_t_i);
					tel->second.s[i].time_axis.resize(max_t_i);
				}
			}*/
			//add info for grouping data on the scatter plot
			for (auto p = compiler.regions.begin(); p != compiler.regions.end(); p++)
				data_categories[p->first] = all_strains[p->second.metadata.device_regression_match_description()].second;
		}
		else if (survival_grouping == ns_group_by_device) {
			survival_curve_title = movement_vs_posture_title = "Device Name";
			std::map<std::string, ns_survival_plot_data_grouping> all_devices;
			for (unsigned int i = 0; i < survival_curve_set.size(); i++)
				all_devices.emplace(std::pair<std::string, ns_survival_plot_data_grouping>(survival_curve_set.curve(i).metadata.device, ns_survival_plot_data_grouping()));
			survival_curves.resize(all_devices.size());
			const ns_telemetry_cache::key_type key(survival_grouping, death_plot, region_to_view, data_to_process.device_regression_match_description());
			ns_telemetry_cache::iterator tel = telemetry_cache.find(key);
			if (tel == telemetry_cache.end()) {
				tel = telemetry_cache.insert(ns_telemetry_cache::value_type(key, ns_survival_telemetry_cache())).first;
				tel->second.s.resize(all_devices.size());
				int i = 0;
				for (auto p = all_devices.begin(); p != all_devices.end(); p++) {
					survival_curve_set.generate_aggregate_risk_timeseries(data_to_process, filter_by_strain, p->first, region_to_view, tel->second.s[i].best_guess_survival, tel->second.s[i].movement_survival, tel->second.s[i].death_associated_expansion_survival, tel->second.s[i].fast_movement_cessation, tel->second.s[i].time_axis, false);
					++i;
				}
			}
			if (tel->second.s.size() != all_devices.size())
				throw ns_ex("Yikes!");
			unsigned long survival_curve_id = 0;
			unsigned long max_t_i(0);
			unsigned long longest_time_axis_id = 0;
			for (auto p = all_devices.begin(); p != all_devices.end(); p++) {
				//set.generate_aggregate_risk_timeseries(data_to_process, filter_by_strain, p->first, region_to_view, best_guess_survival_sub, movement_survival_sub, death_associated_expansion_survival_sub, time_axis, true);
				ns_survival_data_with_censoring_timeseries* survival_data;
				switch (death_plot) {
				case ns_plot_movement_death:
					survival_data = &tel->second.s[survival_curve_id].movement_survival.data; break;
				case ns_plot_expansion_death:
					survival_data = &tel->second.s[survival_curve_id].death_associated_expansion_survival.data; break;
				case ns_plot_best_guess:
					survival_data = &tel->second.s[survival_curve_id].best_guess_survival.data; break;
				case ns_plot_fast_movement_death:
					survival_data = &tel->second.s[survival_curve_id].fast_movement_cessation.data; break;
				default: throw ns_ex("Unknown death time type requested");
				}

				survival_curves[survival_curve_id].vals.type = ns_graph_object::ns_graph_dependant_variable;
				survival_curves[survival_curve_id].vals.y.resize(survival_data->probability_of_surviving_up_to_interval.size());
				survival_curves[survival_curve_id].vals.x.resize(survival_data->probability_of_surviving_up_to_interval.size());
				bool plotted_zero(false);

				for (unsigned int i = 0; i < survival_data->probability_of_surviving_up_to_interval.size(); i++) {
					survival_curves[survival_curve_id].vals.x[i] = tel->second.s[survival_curve_id].time_axis[i];
					if (survival_data->probability_of_surviving_up_to_interval[i] < .00001) {
						if (plotted_zero) {
							survival_curves[survival_curve_id].vals.y[i] = -1;
							continue;
						}
						plotted_zero = true;
					}
					if (max_t_i < i) {
						longest_time_axis_id = survival_curve_id;
						max_t_i = i;
					}
					survival_curves[survival_curve_id].vals.y[i] = survival_data->probability_of_surviving_up_to_interval[i];
				}
				survival_curves[survival_curve_id].color = ns_rainbow<ns_color_8>(survival_curve_id / (float)all_devices.size());
				survival_curves[survival_curve_id].name = p->first;
				//prep lookup table for grouping scatter plot
				p->second.color = survival_curves[survival_curve_id].color;
				p->second.group_name = survival_curves[survival_curve_id].name = p->first;
				survival_curve_id++;
			}
			/*if (max_t_i < survival_curves[survival_curve_id].vals.y.size()) {
				tel->second.s[survival_curve_id].time_axis.resize(max_t_i);
				for (unsigned int i = 0; i < survival_curves.size(); i++)
					survival_curves[i].vals.y.resize(max_t_i);
			}*/
			//add info for grouping data on the scatter plot
			for (auto p = compiler.regions.begin(); p != compiler.regions.end(); p++)
				data_categories[p->first] = all_devices[p->second.metadata.device];
		}

		//make sure that each survival curve is plotted from time 0 until the last time of death
		//do this by adding a 1 at time zero if needed.
		bool found_value_at_or_before_age_zero(false);
		for (unsigned int j = 0; j < survival_curves.size(); j++) {
			if (survival_curves[j].vals.x.empty() || survival_curves[j].vals.x[0] <= time_at_which_animals_were_age_zero)
				continue;
			//attention: this likely requires a copy of the whole vector.
			survival_curves[j].vals.x.insert(survival_curves[j].vals.x.begin(), time_at_which_animals_were_age_zero);
			survival_curves[j].vals.y.insert(survival_curves[j].vals.y.begin(), 1);
		}
		/*
		unsigned long first_valid_timepoint(0);
		for (long i = 0; i < time_axis.size(); i++) {
			if (time_axis[i] >= time_at_which_animals_were_age_zero) {
				first_valid_timepoint = i;
				break;
			}
		}
		time_axis = std::vector<unsigned long>(time_axis.begin() + first_valid_timepoint, time_axis.end());*/
		for (unsigned int i = 0; i < survival_curves.size(); i++) {
			for (unsigned int t = 0; t < survival_curves[i].vals.x.size(); t++)
				survival_curves[i].vals.x[t] = (survival_curves[i].vals.x[t] - time_at_which_animals_were_age_zero) / (60.0 * 60.0 * 24.0);
		}
		/*
		for (long i = 0; i < time_axis.size(); i++)
			time_axis[i] -= time_at_which_animals_were_age_zero;
		*/


		//now make the scatter plots

		movement_vs_posture_vals_fully_specified.resize(0);
		movement_vs_posture_vals_by_hand_annotated.resize(0);
		movement_vs_posture_vals_not_fully_specified.resize(0);
		movement_vs_posture_vals_outliers.resize(0);


		movement_id_lookup.clear();

		//set up groups to plot based on the mappings we set up before
		{
			std::map<std::string, unsigned long> group_mappings_to_data;
			for (auto p = data_categories.begin(); p != data_categories.end(); p++)
				group_mappings_to_data[p->second.group_name] = 0;
			movement_vs_posture_vals_fully_specified.resize(group_mappings_to_data.size());
			movement_vs_posture_vals_not_fully_specified.resize(group_mappings_to_data.size());
			movement_vs_posture_vals_by_hand_annotated.resize(group_mappings_to_data.size());
			movement_vs_posture_vals_outliers.resize(group_mappings_to_data.size());
			{
				int tm = 0;
				for (auto p = group_mappings_to_data.begin(); p != group_mappings_to_data.end(); p++) {
					p->second = tm;
					tm++;
				}
			}

			for (auto p = data_categories.begin(); p != data_categories.end(); p++) {
				p->second.output_location_id = group_mappings_to_data[p->second.group_name];
				movement_vs_posture_vals_fully_specified[p->second.output_location_id].name = p->second.group_name;
				movement_vs_posture_vals_fully_specified[p->second.output_location_id].color = p->second.color;
				movement_vs_posture_vals_not_fully_specified[p->second.output_location_id].name = p->second.group_name;
				movement_vs_posture_vals_not_fully_specified[p->second.output_location_id].color = p->second.color;
				movement_vs_posture_vals_by_hand_annotated[p->second.output_location_id].name = p->second.group_name;
				movement_vs_posture_vals_by_hand_annotated[p->second.output_location_id].color = p->second.color;
				movement_vs_posture_vals_outliers[p->second.output_location_id].name = p->second.group_name;
				movement_vs_posture_vals_outliers[p->second.output_location_id].color = p->second.color;
			}
		}

		//first we build the data structure points_to_plot which contains the graph contents.
		//later we analyze and plot it.
		std::map<unsigned long, std::vector<ns_scatter_plot_element> > points_to_plot;
		double max_x(0), max_y(0), max_y_x_diff(0);
		for (ns_death_time_annotation_compiler::ns_region_list::const_iterator r = compiler.regions.begin(); r != compiler.regions.end(); r++) {
			if (region_to_view != 0 && r->first != region_to_view)
				continue;
			if (filter_by_strain && r->second.metadata.device_regression_match_description() != data_to_process.device_regression_match_description())
				continue;
			for (ns_death_time_annotation_compiler_region::ns_location_list::const_iterator l = r->second.locations.begin(); l != r->second.locations.end(); l++) {

				ns_multiple_worm_cluster_death_annotation_handler multiple_worm_handler;
				//if (l->properties.number_of_worms()>1)
				ns_dying_animal_description_set_const set;
				l->generate_dying_animal_description_const(true, set);

				if (l->properties.is_excluded() || l->properties.is_censored() || l->properties.flag.event_should_be_excluded())
					continue;
				//each worm can add multiple points to the plot, depending on what the user has asked to display
				//currently only one point per worm is displayed.
				ns_plot_pair pair_to_plot;

				//each location can have multiple worms in a group.  We plot each individually, keeping track of the annotations that mark the x and y position of each point.
				for (unsigned int i = 0; i < set.descriptions.size(); i++) {
					pair_to_plot.outlier_data = ns_outlier_search_data(set.descriptions[i],l->properties.number_of_worms()); 
					if (regression_plot == ns_vigorous_vs_death) {
						movement_vs_posture_x_axis_label = "Vigorous Movement Cessation Time (days)";
						movement_vs_posture_y_axis_label = "Death Time (days)";
						if (set.descriptions[i].by_hand.best_guess_death_annotation != 0)
							pair_to_plot.second = set.descriptions[i].by_hand.best_guess_death_annotation;
						else pair_to_plot.second = set.descriptions[i].machine.best_guess_death_annotation;
						if (set.descriptions[i].by_hand.last_fast_movement_annotation != 0)
							pair_to_plot.first = set.descriptions[i].by_hand.last_fast_movement_annotation;
						else pair_to_plot.first = set.descriptions[i].machine.last_fast_movement_annotation;
					}
					if (regression_plot == ns_movement_vs_expansion) {
						movement_vs_posture_x_axis_label = "Movement Cessation Time (days)";
						movement_vs_posture_y_axis_label = "Death-Associated Expansion Time (days)";
						movement_vs_posture_note = "(Only individuals with both types of deaths are shown)";
						if (set.descriptions[i].by_hand.death_associated_expansion_start != 0)
							pair_to_plot.second = set.descriptions[i].by_hand.death_associated_expansion_start;
						else pair_to_plot.second = set.descriptions[i].machine.death_associated_expansion_start;
						if (set.descriptions[i].by_hand.movement_based_death_annotation != 0)
							pair_to_plot.first = set.descriptions[i].by_hand.movement_based_death_annotation;
						else pair_to_plot.first = set.descriptions[i].machine.movement_based_death_annotation;
					}
					else if (regression_plot == ns_by_hand_machine) {

						movement_vs_posture_x_axis_label = "By Hand Event Time (days)";
						movement_vs_posture_y_axis_label = "Machine Event Time (days)";
						if (death_plot == ns_plot_movement_death) {
							pair_to_plot.first = set.descriptions[i].by_hand.movement_based_death_annotation;
							pair_to_plot.second = set.descriptions[i].machine.movement_based_death_annotation;
						}
						else {
							pair_to_plot.first = set.descriptions[i].by_hand.death_associated_expansion_start;
							pair_to_plot.second = set.descriptions[i].machine.death_associated_expansion_start;
						}

					}
					else if (regression_plot == ns_death_vs_observation_duration) {

						movement_vs_posture_x_axis_label = "Death Time (days)";
						movement_vs_posture_y_axis_label = "Duration observed (days)";



						if (death_plot == ns_plot_movement_death) {
							if (set.descriptions[i].by_hand.movement_based_death_annotation != 0)
								pair_to_plot.first = set.descriptions[i].by_hand.movement_based_death_annotation;
							else
								pair_to_plot.first = set.descriptions[i].machine.movement_based_death_annotation;
						}
						else if (death_plot == ns_plot_expansion_death) {
							if (set.descriptions[i].by_hand.death_associated_expansion_start != 0)
								pair_to_plot.first = set.descriptions[i].by_hand.death_associated_expansion_start;
							else
								pair_to_plot.first = set.descriptions[i].machine.death_associated_expansion_start;
						}
						else if (death_plot == ns_plot_best_guess) {
							const ns_death_time_annotation* movement,
								* death_associated_expansion;
							if (set.descriptions[i].by_hand.movement_based_death_annotation != 0)
								movement = set.descriptions[i].by_hand.movement_based_death_annotation;
							else
								movement = set.descriptions[i].machine.movement_based_death_annotation;

							if (set.descriptions[i].by_hand.death_associated_expansion_start != 0)
								death_associated_expansion = set.descriptions[i].by_hand.death_associated_expansion_start;
							else
								death_associated_expansion = set.descriptions[i].machine.death_associated_expansion_start;
							pair_to_plot.first = ns_dying_animal_description_group<ns_death_time_annotation_time_interval>::calculate_best_guess_death_annotation(movement, death_associated_expansion);
						}
						else throw ns_ex("Unknown death plot type!");
						double observation_duration = set.descriptions[i].machine.observation_duration();
						if (observation_duration != -1) {
							pair_to_plot.second = ns_scatter_plot_coordinate(observation_duration);
						}
						else {
							auto p = last_measurement_cache.find(r->first);
							if (p != last_measurement_cache.end() && set.descriptions[i].machine.last_fast_movement_annotation != 0)
								pair_to_plot.second = ns_scatter_plot_coordinate(
									p->second -
									set.descriptions[i].machine.last_fast_movement_annotation->time.best_estimate_event_time_for_possible_partially_unbounded_interval());
						}
					}
				}



				const ns_64_bit region_info_id(r->first);


				const unsigned long group_id = data_categories[region_info_id].output_location_id;
				auto p = points_to_plot.emplace(std::pair<unsigned long, std::vector<ns_scatter_plot_element> >(group_id, std::vector<ns_scatter_plot_element>())).first;
				p->second.resize(p->second.size() + 1);
				ns_scatter_plot_element& point = *p->second.rbegin();
				point.region_info_id = region_info_id;
				point.stationary_path_id = l->properties.stationary_path_id;
				point.outlier_data = pair_to_plot.outlier_data;
				if (pair_to_plot.first.using_value) {
					if (pair_to_plot.first.value > 0)
						point.x_raw = pair_to_plot.first.value;
					else point.x_specified = false;
				}
				else {
					if (pair_to_plot.first.annotation != 0 && pair_to_plot.first.annotation->time.best_estimate_event_time_for_possible_partially_unbounded_interval() > 0)
						point.x_raw = pair_to_plot.first.annotation->time.best_estimate_event_time_for_possible_partially_unbounded_interval();
					else point.x_specified = false;
				}

				if (pair_to_plot.second.using_value) {
					if (pair_to_plot.second.value > 0)
						point.y_raw = pair_to_plot.second.value;
					else point.y_specified = false;
				}
				else {
					if (pair_to_plot.second.annotation != 0 && pair_to_plot.second.annotation->time.best_estimate_event_time_for_possible_partially_unbounded_interval() > 0)
						point.y_raw = pair_to_plot.second.annotation->time.best_estimate_event_time_for_possible_partially_unbounded_interval();
					else  point.y_specified = false;
				}


				// find max values(that we will use later)
				if (point.x_specified && point.x_raw > max_x) max_x = point.x_raw;
				if (point.y_specified && point.y_raw > max_y) max_y = point.y_raw;
				if (point.x_specified && point.y_specified && (point.y_raw - point.x_raw > max_y_x_diff))
					max_y_x_diff = point.y_raw - point.x_raw;

			}
		}

		//we want to find and tag outlier points
		//So, we fit a 2-d gaussian to the data
		//and then flag points that are on the tails of the distribution.
		ns_emission_probabiliy_gaussian_diagonal_covariance_model<ns_measurement_accessor> normal_fit;
		normal_fit.setup_gmm(2, 1);
		normal_fit.dimensions.reserve(2);
		normal_fit.dimensions.push_back(ns_covarying_gaussian_dimension<ns_measurement_accessor>(new ns_weak_movement_accessor, "w"));
		normal_fit.dimensions.push_back(ns_covarying_gaussian_dimension<ns_measurement_accessor>(new ns_alive_but_non_moving_movement_accessor, "a"));

		unsigned long number_of_points(0);
		std::vector< const std::vector<ns_scatter_plot_element>* > data_holder(points_to_plot.size());
		{
			int i = 0;
			for (auto p = points_to_plot.begin(); p != points_to_plot.end(); p++) {
				data_holder[i] = &p->second;
				number_of_points += p->second.size();
				++i;
			}
		}
		//ofstream o("c:\\server\\outlier_debug.csv");
		//o << "val1,val2,p,outlier\n";
		const double p_cutoff = .02;
		try {
			normal_fit.build_from_data(data_holder, number_of_points);
			{
				for (auto p = points_to_plot.begin(); p != points_to_plot.end(); p++) {
					for (unsigned int i = 0; i < p->second.size(); i++) {
						if (p->second[i].outlier_data.alive_but_not_moving_duration == -1 || p->second[i].outlier_data.weak_movement_duration == -1) {
							p->second[i].outlier_data.is_an_outlier = false;
							continue;
						}
						const double prob = normal_fit.point_emission_likelihood(p->second[i]);
						p->second[i].outlier_data.is_an_outlier = prob < p_cutoff;
						//			o << p->second[i].outlier_data.alive_but_not_moving_duration << "," << p->second[i].outlier_data.weak_movement_duration << "," << prob << "," << p->second[i].outlier_data.is_an_outlier << "\n";
					}
				}
			}
		}
		catch (ns_ex & ex) {
			cerr <<"Problem during outlier detection: " << ex.text() << "\n";

		}
		//o.close();




		std::random_device dev;
		std::mt19937 rng(dev());
		std::uniform_real_distribution<> rnd_dist(.95, 1.05);
		//set up the position at which animals with undefined values are placed (10% larger than largest value, proportional to the lifespan not linux epoch!)
		undefined_x_position = (max_x - metadata.time_at_which_animals_had_zero_age) * 1.15;
		if (movement_plot == ns_plot_death_times_residual)
			undefined_y_position = max_y_x_diff * 1.15;
		else if (regression_plot == ns_death_vs_observation_duration)
			undefined_y_position = (max_y) * 1.15;
		else if (max_y < metadata.time_at_which_animals_had_zero_age)
			undefined_y_position = (max_x - metadata.time_at_which_animals_had_zero_age) * 1.15;	//if no values are specified for any point
		else
			undefined_y_position = (max_y - metadata.time_at_which_animals_had_zero_age) * 1.15;

		//now we analyze and plot the points_to_plot structure
		//now replace all the unspecified times with the max value so they fall on top or on the side of the chart.
		for (auto p = points_to_plot.begin(); p != points_to_plot.end(); ++p) {
			movement_vs_posture_vals_fully_specified[p->first].vals.x.resize(0);
			movement_vs_posture_vals_fully_specified[p->first].vals.y.resize(0);
			movement_vs_posture_vals_by_hand_annotated[p->first].vals.x.resize(0);
			movement_vs_posture_vals_by_hand_annotated[p->first].vals.y.resize(0);
			movement_vs_posture_vals_not_fully_specified[p->first].vals.x.resize(0);
			movement_vs_posture_vals_not_fully_specified[p->first].vals.y.resize(0);
			movement_vs_posture_vals_outliers[p->first].vals.x.resize(0);
			movement_vs_posture_vals_outliers[p->first].vals.y.resize(0);

			movement_vs_posture_vals_fully_specified[p->first].vals.x.reserve(p->second.size());
			movement_vs_posture_vals_fully_specified[p->first].vals.y.reserve(p->second.size());
			movement_vs_posture_vals_by_hand_annotated[p->first].vals.x.reserve(p->second.size());
			movement_vs_posture_vals_by_hand_annotated[p->first].vals.y.reserve(p->second.size());
			movement_vs_posture_vals_not_fully_specified[p->first].vals.x.reserve(p->second.size());
			movement_vs_posture_vals_not_fully_specified[p->first].vals.y.reserve(p->second.size());
			movement_vs_posture_vals_outliers[p->first].vals.x.reserve(p->second.size()/5);
			movement_vs_posture_vals_outliers[p->first].vals.y.reserve(p->second.size()/5);

			for (unsigned int j = 0; j < p->second.size(); j++) {
				bool fully_specified = true;
				//handle residuals and missing data for y
				if (movement_plot == ns_plot_death_times_residual) {
					if (!p->second[j].x_specified || !p->second[j].y_specified) {
						fully_specified = false;
						p->second[j].y_raw = undefined_y_position * rnd_dist(rng);  //add jitter
					}
					else p->second[j].y_raw -= p->second[j].x_raw;
				}
				else if (!p->second[j].y_specified) {
					fully_specified = false;
					if (regression_plot == ns_death_vs_observation_duration)
						p->second[j].y_raw = (undefined_y_position)*rnd_dist(rng);  //add jitter 
					else
						p->second[j].y_raw = (undefined_y_position)*rnd_dist(rng) + metadata.time_at_which_animals_had_zero_age;  //add jitter 
				}

				//handle missing data for x
				if (!p->second[j].x_specified) {
					fully_specified = false;
					p->second[j].x_raw = (undefined_x_position)*rnd_dist(rng) + metadata.time_at_which_animals_had_zero_age;  //add jitter
				}
				std::vector<ns_survival_graph>* graph_to_add(&movement_vs_posture_vals_fully_specified);
				if (p->second[j].outlier_data.by_hand_annotation_exists) {
					if (this->by_hand_plot == ns_plot_machine)
						graph_to_add = 0;
					else
						graph_to_add = &movement_vs_posture_vals_by_hand_annotated;
				}
				else if (this->by_hand_plot == ns_plot_by_hand)
					graph_to_add = 0;
				else if (!fully_specified)
					graph_to_add = &movement_vs_posture_vals_not_fully_specified;
				else if (p->second[j].outlier_data.is_an_outlier)
					graph_to_add = &movement_vs_posture_vals_outliers;
				if (graph_to_add == 0)
					continue;
				if (movement_plot == ns_plot_death_times_residual || regression_plot == ns_death_vs_observation_duration)
					(*graph_to_add)[p->first].vals.y.push_back(p->second[j].y_raw / 60.0 / 60.0 / 24);

				else(*graph_to_add)[p->first].vals.y.push_back(((double)p->second[j].y_raw - metadata.time_at_which_animals_had_zero_age) / 60.0 / 60.0 / 24);

				(*graph_to_add)[p->first].vals.x.push_back(((double)p->second[j].x_raw - metadata.time_at_which_animals_had_zero_age) / 60.0 / 60.0 / 24);


				ns_lookup_index li(p->second[j].x_raw, p->second[j].y_raw);

				movement_id_lookup[li].push_back(ns_worm_lookup_info(p->second[j].region_info_id, p->second[j].stationary_path_id));
			}
		}
		undefined_x_position = undefined_x_position / 60.0 / 60.0 / 24.0;
		undefined_y_position = undefined_y_position / 60.0 / 60.0 / 24.0;

	}
	void draw(const ns_graph_contents graph_contents, const ns_vector_2i& position, const ns_vector_2i& graph_size, const ns_vector_2i& buffer_size, const float marker_resize_factor, ns_tiled_gl_image & buffer, const unsigned long start_time = 0, const unsigned long stop_time = UINT_MAX) {
		
		if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Starting to draw telemetry."));
		if (survival_image.properties().height == 0 || graph_contents != last_graph_contents || last_start_time != start_time || last_stop_time != stop_time || last_rescale_factor != marker_resize_factor || !(last_graph_size == graph_size)) {
			ns_image_properties prop;
			prop.components = 3;
			
			unsigned long legend_width = 215;
			if (legend_width > prop.width)
			if (graph_contents == graph_size.x)
				throw ns_ex("Legend is wider than graph!");

			prop.width = (graph_size.x - 3 * border().x-2*legend_width)/2;
			prop.height = graph_size.y - 2*border().y;
			
			ns_image_properties legend_prop(prop.height, legend_width,  3);
			survival_image.init(prop);
			movement_vs_posture_image.init(prop);
			survival_image_legend.init(legend_prop);
			movement_vs_posture_image_legend.init(legend_prop);
			try {

				draw_base_graph(graph_contents, marker_resize_factor, start_time, stop_time);
				last_graph_contents = graph_contents;
				last_start_time = start_time;
				last_stop_time = stop_time;
				last_rescale_factor = marker_resize_factor;
				last_graph_size = graph_size;
			}
			catch (...) {
				survival_image.init(ns_image_properties(0, 0, 3));
				movement_vs_posture_image.init(ns_image_properties(0, 0, 3));
				survival_image_legend.init(ns_image_properties(0, 0, 3));
				movement_vs_posture_image_legend.init(ns_image_properties(0, 0, 3));
				throw;
			}
		}
		if (graph_contents == ns_none)
			return;

		
		if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Drawing everything"));

		const unsigned long movement_x_offset(2 * border().x + survival_image.properties().width+ survival_image_legend.properties().width);

		const long yo = (graph_size.y - 1);
		//top margin
		for (unsigned int y = 0; y < border().y; y++)
			for (unsigned int x = 0; x < graph_size.x; x++) {
				for (unsigned int c = 0; c < 3; c++)
					buffer(x + position.x, yo-(y + position.y))[c] = 0;
				buffer(x + position.x, yo - (y + position.y))[3] = 255;
			}
		//TOP GRAPH
		for (unsigned int y = 0; y < survival_image.properties().height; y++) {
			//left margin
			for (unsigned int x = 0; x < border().x; x++) {
				for (unsigned int c = 0; c < 3; c++)
					buffer(x + position.x, yo - (y + border().y + position.y))[c] = 0;
				buffer(x + position.x, yo - (y + border().y + position.y))[3] = 0;
			}
			//graph
			for (unsigned int x = 0; x < survival_image.properties().width; x++) {
				for (unsigned int c = 0; c < 3; c++)	
					buffer(x + border().x + position.x, yo - (y + border().y + position.y))[c] = survival_image[y][3 * x + c];
				buffer(x + border().x + position.x, yo - (y + border().y + position.y))[3] = 255;
			}
			//graph legend
			for (unsigned int x = 0; x < survival_image_legend.properties().width; x++) {
				for (unsigned int c = 0; c < 3; c++)
					buffer(x + border().x + survival_image.properties().width+ position.x, yo - (y + border().y + position.y))[c] = survival_image_legend[y][3 * x + c];
				buffer(x + border().x + survival_image.properties().width + position.x, yo - (y + border().y + position.y))[3] = 255;
			}

			//right margin
			for (unsigned int x = survival_image.properties().width + survival_image_legend.properties().width + border().x; x < movement_x_offset; x++) {
				for (unsigned int c = 0; c < 3; c++)
					buffer(x + position.x, yo - (y + border().y + position.y))[c] = 0;
				buffer(x + position.x, yo - (y + border().y + position.y))[3] = 255;
			}
		}
		//bottom margin
		for (unsigned int y = survival_image.properties().height+border().y; y < graph_size.y; y++)
			for (unsigned int x = 0; x < movement_x_offset; x++) {
				for (unsigned int c = 0; c < 3; c++)
					buffer(x + position.x, yo - (y + position.y))[c] = 0;
				buffer(x + position.x, yo - (y + position.y))[3] = 255;
			}

		if (graph_contents == ns_population_telemetry::ns_movement_vs_posture ) {
			//BOTTOM GRAPH
			for (unsigned int y = 0; y < movement_vs_posture_image.properties().height; y++) {
				/*//left margin
				for (unsigned int x = 0; x < border().x; x++)
					for (unsigned int c = 0; c < 3; c++)
						buffer[map_pixel_from_image_onto_buffer(x+ movement_x_offset, y +border.y, position, buffer_size) + c] = 0;*/
						//graph
				//graph
				for (unsigned int x = 0; x < movement_vs_posture_image.properties().width; x++) {
					for (unsigned int c = 0; c < 3; c++)
						buffer(x + movement_x_offset + position.x, yo - (y + border().y + position.y))[c]= movement_vs_posture_image[y][3 * x + c];
					buffer(x + movement_x_offset + position.x, yo - (y + border().y + position.y))[3] = 255;
				}
				//graph legend
				for (unsigned int x = 0; x < movement_vs_posture_image_legend.properties().width; x++) {
					for (unsigned int c = 0; c < 3; c++)
						buffer(x + movement_x_offset + movement_vs_posture_image.properties().width + position.x, yo - (y + border().y + position.y))[c] = movement_vs_posture_image_legend[y][3 * x + c];
					buffer(x + movement_x_offset + movement_vs_posture_image.properties().width + position.x, yo - (y + border().y + position.y))[3] = 255;
				}

				//right margin
				for (unsigned int x = movement_vs_posture_image.properties().width + movement_vs_posture_image_legend.properties().width + movement_x_offset; x < graph_size.x; x++) {
					for (unsigned int c = 0; c < 3; c++)
						buffer(x + position.x, yo - (y + border().y + position.y))[c] = 0;
					buffer(x + position.x, yo - (y + border().y + position.y))[3] = 255;
				}
			}
			//bottom margin
			for (unsigned int y = movement_vs_posture_image.properties().height + border().y; y < graph_size.y; y++)
				for (unsigned int x = movement_x_offset; x < graph_size.x; x++) {
					for (unsigned int c = 0; c < 3; c++)
						buffer(x + position.x, yo - (y + position.y))[c] = 0;
					buffer(x + position.x, yo - (y + position.y))[3] = 255;
				}
		}
		overlay_metadata(graph_contents, position, buffer_size, marker_resize_factor, buffer);
	}
};
