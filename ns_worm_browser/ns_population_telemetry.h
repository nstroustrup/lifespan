#pragma once
#include "ns_graph.h"
#include "ns_survival_curve.h"
#include "ns_death_time_annotation.h"
#include "ns_vector.h"

struct ns_worm_lookup_info {
	ns_worm_lookup_info() {}
	ns_worm_lookup_info(const ns_64_bit& r, const ns_stationary_path_id& i):region_id(r),id(i) {}
	ns_64_bit region_id;
	ns_stationary_path_id id;
};
struct ns_lookup_index {
	ns_lookup_index(const unsigned long mt, const unsigned long dt) :movement_time(mt), death_associated_expansion_time(dt) {}
	unsigned long movement_time, death_associated_expansion_time;
};
bool operator <(const ns_lookup_index& a, const ns_lookup_index& b);

struct ns_survival_graph {
	ns_survival_graph() :vals(ns_graph_object::ns_graph_dependant_variable) {}
	ns_graph_object vals;
	std::string name;
	ns_color_8 color;
};
class ns_population_telemetry {
public:
	typedef enum { ns_none, ns_survival, ns_movement_vs_posture, ns_number_of_graph_types } ns_graph_contents;
	typedef enum { ns_group_by_device, ns_group_by_strain, ns_aggregate_all, ns_group_by_death_type, ns_survival_grouping_num} ns_survival_grouping;
	typedef enum { ns_plot_death_times_absolute, ns_plot_death_times_residual,ns_movement_plot_num } ns_movement_plot_type;
	typedef enum { ns_plot_movement_death,ns_plot_expansion_death, ns_death_plot_num } ns_death_plot_type;
	typedef enum { ns_plot_death_types, ns_by_hand_machine,ns_regression_type_num } ns_regression_type;
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
		case ns_plot_movement_death:
			return "Movement";
		case ns_plot_expansion_death:
			return "Expansion";
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
		case ns_plot_death_types:
			return "Death Types";
		case ns_by_hand_machine:
			return "By hand vs Machine";
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
	ns_death_plot_type death_plot;
	ns_movement_plot_type movement_plot;
	ns_regression_type regression_plot;

private:


	bool _show;
	unsigned long group_id;
	ns_image_standard survival_image, movement_vs_posture_image;
	ns_graph survival,movement_vs_posture;
	ns_graph_specifics survival_specifics, movement_vs_posture_specifics;
	std::vector<ns_survival_graph> survival_curves;
	std::vector<ns_survival_graph> movement_vs_posture_vals;
	std::string movement_vs_posture_x_axis_label, movement_vs_posture_y_axis_label;
	ns_color_8 movement_vs_posture_x_axis_color, movement_vs_posture_y_axis_color;
	ns_graph_object time_vals, unity_line;
	vector <unsigned long> time_axis;
	unsigned long time_at_which_animals_were_age_zero;
	std::vector<double> temp1, temp2;

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

		time_vals.x.resize(time_axis.size());
		for (unsigned int i = 0; i < time_axis.size(); i++)
			time_vals.x[i] = time_axis[i] / 60.0 / 60.0 / 24.0;

		time_vals.type = ns_graph_object::ns_graph_independant_variable;
		survival.add_reference(&time_vals);

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

		if (graph_contents != ns_movement_vs_posture)
			return;
		if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Rendering movement v posture graph"));

		double tmax(0), tmin(DBL_MAX), ymax(0), ymin(DBL_MAX);
		unsigned long number_of_points(0);
		for (unsigned int j = 0; j < movement_vs_posture_vals.size(); j++) {
			for (unsigned int i = 0; i < movement_vs_posture_vals[j].vals.x.size(); i++) {
				number_of_points++;
				if (movement_vs_posture_vals[j].vals.x[i] < tmin)
					tmin = movement_vs_posture_vals[j].vals.x[i];
				if (movement_plot == ns_plot_death_times_absolute && movement_vs_posture_vals[j].vals.y[i] < tmin)
					tmin = movement_vs_posture_vals[j].vals.y[i];
				if (movement_vs_posture_vals[j].vals.y[i] < ymin)
					ymin = movement_vs_posture_vals[j].vals.y[i];
				if (movement_vs_posture_vals[j].vals.x[i] > tmax)
					tmax = movement_vs_posture_vals[j].vals.x[i];
				if (movement_plot == ns_plot_death_times_absolute && movement_vs_posture_vals[j].vals.y[i] > tmax)
					tmax = movement_vs_posture_vals[j].vals.y[i];
				if (movement_vs_posture_vals[j].vals.y[i] > ymax)
					ymax = movement_vs_posture_vals[j].vals.y[i];
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
			movement_vs_posture.add_reference(&unity_line);
		}

		movement_vs_posture.x_axis_properties.text_decimal_places = 1;
		movement_vs_posture.y_axis_properties.text_decimal_places = 2;
		movement_vs_posture.x_axis_properties.text.color = ns_color_8(255, 0, 0);
		movement_vs_posture.x_axis_label = movement_vs_posture_x_axis_label;
		movement_vs_posture.y_axis_properties.text.color = ns_color_8(0, 0, 255);
		movement_vs_posture.y_axis_label = movement_vs_posture_y_axis_label;
		for (unsigned int i = 0; i < movement_vs_posture_vals.size(); i++) {
			movement_vs_posture_vals[i].vals.type = ns_graph_object::ns_graph_dependant_variable;
			movement_vs_posture_vals[i].vals.properties.line.draw = false;
			movement_vs_posture_vals[i].vals.properties.point.draw = true;
			movement_vs_posture_vals[i].vals.properties.point.color = movement_vs_posture_vals[i].color;
			movement_vs_posture_vals[i].vals.properties.point.width = 4;
			movement_vs_posture_vals[i].vals.properties.point.edge_color = ns_color_8(0, 0, 0);
			movement_vs_posture_vals[i].vals.properties.point.edge_width = 1;
			movement_vs_posture_vals[i].vals.properties.point.point_shape = ns_graph_color_set::ns_circle;
			movement_vs_posture.add_reference(&movement_vs_posture_vals[i].vals);
		}


		ns_graph_axes axes2;
		if (movement_plot == ns_plot_death_times_absolute) {
			axes2.boundary(0) = axes2.boundary(2) = tmin;
			axes2.boundary(1) = axes2.boundary(3) = tmax;
		}
		else {
			axes2.boundary(0) = tmin;
			axes2.boundary(1) = tmax;
			axes2.boundary(2) = ymin;
			axes2.boundary(3) = ymax;
		}

		movement_vs_posture.set_graph_display_options("", axes2, movement_vs_posture_image.properties().width / (float)movement_vs_posture_image.properties().height);
		movement_vs_posture_specifics = movement_vs_posture.draw(movement_vs_posture_image);
		/*for (unsigned long y = 0; y < movement_vs_posture_image.properties().height; y++)
			for (unsigned long x = 0; x < movement_vs_posture_image.properties().width; x++)
				for (unsigned long c = 0; c < 3; c++)
					movement_vs_posture_image[y][3 * x + c] = 125;*/




	}
	inline void map_value_from_survival_onto_image(const float& x, const float& y, unsigned long& x1, unsigned long& y1) {
		x1 = survival_specifics.boundary.x + (unsigned int)(survival_specifics.dx * (x - survival_specifics.axes.boundary(0) + survival_specifics.axes.axis_offset(0)));
		y1 = survival_image.properties().height - survival_specifics.boundary.y - (unsigned int)(survival_specifics.dy * (y - survival_specifics.axes.boundary(2) + survival_specifics.axes.axis_offset(1)));
	}

	inline void map_value_from_movement_vs_survival_onto_image(const float& x, const float& y, unsigned long& x1, unsigned long& y1) {
		x1 = survival_image.properties().width + border().x + movement_vs_posture_specifics.boundary.x + (unsigned int)(movement_vs_posture_specifics.dx * (x - movement_vs_posture_specifics.axes.boundary(0) + movement_vs_posture_specifics.axes.axis_offset(0)));
		y1 = movement_vs_posture_image.properties().height - movement_vs_posture_specifics.boundary.y - (unsigned int)(movement_vs_posture_specifics.dy * (y - movement_vs_posture_specifics.axes.boundary(2) + movement_vs_posture_specifics.axes.axis_offset(1)));
	}
	inline unsigned long map_pixel_from_image_onto_buffer(const unsigned long& x, const unsigned long& y, const ns_vector_2i& position, const ns_vector_2i& buffer_size) {
		return 3 * ((buffer_size.y - y - position.y - 1) * buffer_size.x + x + position.x);
	}
	void overlay_metadata(const ns_population_telemetry::ns_graph_contents graph_contents, const ns_vector_2i& position, const ns_vector_2i& buffer_size, const long marker_resize_factor, ns_8_bit* buffer) {

	}
	ns_graph_contents last_graph_contents;
	unsigned long last_start_time, last_stop_time;
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
		movement_vs_posture_vals.clear();
		time_axis.clear();
		unity_line.clear();
		last_graph_contents = ns_none;
		last_start_time = 0;
		last_stop_time = 0;
	}
	unsigned long get_graph_time_from_graph_position(const float x) { //x is in relative time
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
		const int movement_vs_posture_offset = survival_image.properties().width + 2 * border().x;
		if (x <= movement_vs_posture_offset) {
			graph_selected = ns_survival;
			const unsigned long permissable_error(permissible_time_error * survival_specifics.dx); //how far away the user can click from the exact right position
			res.x = ((long)x - (long)survival_specifics.boundary.x - (long)border().x) / (survival_specifics.dx) + survival_specifics.axes.boundary(0) - survival_specifics.axes.axis_offset(0);
			if (x + permissable_error < border().x + survival_specifics.boundary.x || x - border().x >= survival_image.properties().width - survival_specifics.boundary.x+ permissable_error) {
				res.x = 0;
				graph_selected = ns_none;
			}
			res.y = ((long)survival_image.properties().height - survival_specifics.boundary.y - (y - border().y)) / survival_specifics.dy + survival_specifics.axes.boundary(2) - survival_specifics.axes.axis_offset(1);
			if (y +permissable_error < border().y + survival_specifics.boundary.y || y - border().y >= survival_image.properties().height - survival_specifics.boundary.y+permissable_error) {
				res.y = 0;
				graph_selected = ns_none;
			}

		}
		else {
			graph_selected = ns_movement_vs_posture;
			const unsigned long permissable_error(permissible_time_error * movement_vs_posture_specifics.dx);
			res.x = ((long)x - movement_vs_posture_offset - (long)movement_vs_posture_specifics.boundary.x) / (movement_vs_posture_specifics.dx) + movement_vs_posture_specifics.axes.boundary(0) - movement_vs_posture_specifics.axes.axis_offset(0);
			if (x + permissable_error < movement_vs_posture_offset + movement_vs_posture_specifics.boundary.x || x - movement_vs_posture_offset >= movement_vs_posture_image.properties().width - movement_vs_posture_specifics.boundary.x+ permissable_error) {
				res.x = 0;
				graph_selected = ns_none;
			}
			res.y = ((long)movement_vs_posture_image.properties().height - movement_vs_posture_specifics.boundary.y - (y - border().y)) / movement_vs_posture_specifics.dy + movement_vs_posture_specifics.axes.boundary(2) - movement_vs_posture_specifics.axes.axis_offset(1);
			if (y + permissable_error < border().y + movement_vs_posture_specifics.boundary.y || y - border().y >= movement_vs_posture_image.properties().height - movement_vs_posture_specifics.boundary.y+ permissable_error) {
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
			res.x = round(res.x * 60 * 60 * 24 + time_at_which_animals_were_age_zero);  //movement
			if (movement_plot == ns_plot_death_times_absolute)
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

	ns_population_telemetry() :_show(false), last_graph_contents(ns_none), movement_plot(ns_plot_death_times_absolute),survival_grouping((ns_survival_grouping)0),unity_line(ns_graph_object::ns_graph_dependant_variable), time_vals(ns_graph_object::ns_graph_independant_variable) {
		survival_image.init(ns_image_properties(0, 0, 3));
		movement_vs_posture_image.init(ns_image_properties(0, 0, 3));
	}

	void set_subject(const ns_64_bit region_id, const ns_region_metadata& strain) {
		subject_region = region_id;
		stats_subset_strain_to_display = strain;

	}
	void update_annotations_and_build_survival(const ns_death_time_annotation_compiler & compiler, const ns_region_metadata & metadata) {
		survival_curves.resize(0);
		ns_lifespan_experiment_set set;
		compiler.generate_survival_curve_set(set, ns_death_time_annotation::ns_machine_annotations_if_no_by_hand, true, false);
		ns_survival_data_with_censoring movement_survival,death_associated_expansion_survival;


		const bool strain_override_specified = stats_subset_strain_to_display.device_regression_match_description() != "";
		const bool viewing_a_storyboard_for_a_specific_strain = metadata.device_regression_match_description() != "";
		const bool subject_region_specified = subject_region != 0;
		const bool single_region_storyboard = compiler.regions.size() == 1;

		const ns_64_bit time_at_zero = metadata.time_at_which_animals_had_zero_age;
		const ns_64_bit storyboard_region_id = metadata.region_id;
	
		
		ns_region_metadata data_to_process;
		const bool filter_by_strain = strain_override_specified || viewing_a_storyboard_for_a_specific_strain;
		if (filter_by_strain)
			if (viewing_a_storyboard_for_a_specific_strain)
				data_to_process = metadata;
			else data_to_process = stats_subset_strain_to_display;
		data_to_process.time_at_which_animals_had_zero_age = metadata.time_at_which_animals_had_zero_age;

		ns_64_bit region_to_view = 0;
		if (!storyboard_region_id && subject_region_specified)
			region_to_view = subject_region;


		set.generate_aggregate_risk_timeseries(data_to_process, filter_by_strain, "",region_to_view, movement_survival, death_associated_expansion_survival, time_axis,false);
		unsigned long time_offset = 0;

		time_at_which_animals_were_age_zero = metadata.time_at_which_animals_had_zero_age;
		if (time_axis.size() != 0) {
			bool found_value_at_or_before_age_zero(false);
			for (long i = 0; i < time_axis.size(); i++) {
				if (time_axis[i] <= time_at_which_animals_were_age_zero) {
					found_value_at_or_before_age_zero = true;
					break;
				}
			}
			//add a 1 at time zero if no values are specified in the curve itself
			if (!found_value_at_or_before_age_zero) {
					time_offset = 1;
					time_axis.resize(time_axis.size() + 1);
					for (long i = time_axis.size() - 1; i >= 1; i--)
						time_axis[i] = (time_axis[i - 1]);
					time_axis[0] = time_at_which_animals_were_age_zero;
			}
			else time_offset = 0;
			if (survival_grouping == ns_aggregate_all || strain_override_specified && survival_grouping == ns_group_by_strain) {
				const bool plot_movement(death_plot == ns_plot_movement_death);
				ns_survival_data_with_censoring_timeseries* survival_data = (plot_movement) ? &movement_survival.data : &death_associated_expansion_survival.data;
				unsigned long max_t_i(0);

				survival_curves.resize(1);
				survival_curves[0].name = plot_movement ? "Movement Death" : "Expansion Death";
				survival_curves[0].color = plot_movement ? ns_color_8(255, 0, 0) : ns_color_8(0, 0, 255);
				//load survival data truncating at the last zero found in each group.

				survival_curves[0].vals.type = ns_graph_object::ns_graph_dependant_variable;
				survival_curves[0].vals.y.resize(survival_data->probability_of_surviving_up_to_interval.size() + time_offset);
				if (time_offset>0)
				survival_curves[0].vals.y[0] = 1;
				bool plotted_zero(false);
				for (unsigned int i = 0; i < survival_data->probability_of_surviving_up_to_interval.size(); i++) {
					if (survival_data->probability_of_surviving_up_to_interval[i] < .001) {
						if (plotted_zero) {
							survival_curves[0].vals.y[i + time_offset] = -1;
							continue;
						}
						plotted_zero = true;
						max_t_i = i;
					}
					else max_t_i = i;
					survival_curves[0].vals.y[i + time_offset] = survival_data->probability_of_surviving_up_to_interval[i];
				}
				if (max_t_i < time_axis.size()) {
					survival_curves[0].vals.y.resize(max_t_i);
					time_axis.resize(max_t_i);
				}
			}
			else if (survival_grouping == ns_group_by_death_type) {

				unsigned long max_t_i(0);

				survival_curves.resize(2);
				survival_curves[0].name = "Movement Death";
				survival_curves[0].color = ns_color_8(255, 0, 0);
				survival_curves[1].name = "Expansion Death";
				survival_curves[1].color = ns_color_8(0, 0, 255);
				//load survival data truncating at the last zero found in each group.
				ns_survival_data_with_censoring_timeseries* survival_data[2] = { &movement_survival.data, &death_associated_expansion_survival.data };
				for (unsigned int j = 0; j < survival_curves.size(); j++) {
					survival_curves[j].vals.type = ns_graph_object::ns_graph_dependant_variable;
					survival_curves[j].vals.y.resize(survival_data[j]->probability_of_surviving_up_to_interval.size() + time_offset);
					if (time_offset > 0)
						survival_curves[j].vals.y[0] = 1;
					bool plotted_zero(false);
					for (unsigned int i = 0; i < survival_data[j]->probability_of_surviving_up_to_interval.size(); i++) {
						if (survival_data[j]->probability_of_surviving_up_to_interval[i] < .001) {
							if (plotted_zero) {
								survival_curves[j].vals.y[i + time_offset] = -1;
								continue;
							}
							plotted_zero = true;
						}
						else max_t_i = i;
						survival_curves[j].vals.y[i + time_offset] = survival_data[j]->probability_of_surviving_up_to_interval[i];
					}
				}
				if (max_t_i < time_axis.size()) {
					survival_curves[0].vals.y.resize(max_t_i);
					survival_curves[1].vals.y.resize(max_t_i);
					time_axis.resize(max_t_i);
				}
			}
			else if (survival_grouping == ns_group_by_strain) {
				std::map<std::string, ns_region_metadata> all_strains;
				for (auto p = set.curves.begin(); p != set.curves.end(); p++)
					all_strains[p->metadata.device_regression_match_description()] = p->metadata;
				survival_curves.resize(all_strains.size());
				ns_survival_data_with_censoring movement_survival_sub, death_associated_expansion_survival_sub;
				unsigned long survival_curve_id = 0;
				unsigned long max_t_i(0);
				for (auto p = all_strains.begin(); p != all_strains.end(); p++) {
					set.generate_aggregate_risk_timeseries(p->second, true, "", region_to_view, movement_survival_sub, death_associated_expansion_survival_sub, time_axis, true);
					ns_survival_data_with_censoring_timeseries* survival_data = (death_plot == ns_plot_movement_death) ? &movement_survival_sub.data : &death_associated_expansion_survival_sub.data;

					survival_curves[survival_curve_id].vals.type = ns_graph_object::ns_graph_dependant_variable;
					survival_curves[survival_curve_id].vals.y.resize(survival_data->probability_of_surviving_up_to_interval.size() + time_offset);
					if (time_offset > 0)
					survival_curves[survival_curve_id].vals.y[0] = 1;
					bool plotted_zero(false);
					for (unsigned int i = 0; i < survival_data->probability_of_surviving_up_to_interval.size(); i++) {
						if (survival_data->probability_of_surviving_up_to_interval[i] < .001) {
							if (plotted_zero) {
								survival_curves[survival_curve_id].vals.y[i+ time_offset] = -1;
								continue;
							}
							plotted_zero = true;
							max_t_i = i;
						}
						else max_t_i = i;
						survival_curves[survival_curve_id].vals.y[i+ time_offset] = survival_data->probability_of_surviving_up_to_interval[i];
					}

					survival_curves[survival_curve_id].color = ns_rainbow<ns_color_8>(survival_curve_id / (float)all_strains.size());
					survival_curves[survival_curve_id].name = p->first;
					survival_curve_id++;
				}
				if (max_t_i < time_axis.size()) {
					time_axis.resize(max_t_i);
					for (unsigned int i = 0; i < survival_curves.size(); i++)
						survival_curves[i].vals.y.resize(max_t_i);
					time_axis.resize(max_t_i);
				}

			}
			else if (survival_grouping == ns_group_by_device) {
				std::set<std::string> all_devices;
				for (auto p = set.curves.begin(); p != set.curves.end(); p++)
					all_devices.insert(all_devices.begin(), p->metadata.device);
				survival_curves.resize(all_devices.size());
				ns_survival_data_with_censoring movement_survival_sub, death_associated_expansion_survival_sub;
				unsigned long survival_curve_id = 0;
				unsigned long max_t_i(0);
				for (auto p = all_devices.begin(); p != all_devices.end(); p++) {
					set.generate_aggregate_risk_timeseries(data_to_process, filter_by_strain, *p, region_to_view, movement_survival_sub, death_associated_expansion_survival_sub, time_axis, true);
					ns_survival_data_with_censoring_timeseries* survival_data = (death_plot == ns_plot_movement_death) ? &movement_survival_sub.data : &death_associated_expansion_survival_sub.data;

					survival_curves[survival_curve_id].vals.type = ns_graph_object::ns_graph_dependant_variable;
					survival_curves[survival_curve_id].vals.y.resize(survival_data->probability_of_surviving_up_to_interval.size() + time_offset);
					if (time_offset!=0)
					survival_curves[survival_curve_id].vals.y[0] = 1;
					bool plotted_zero(false);
					for (unsigned int i = 0; i < survival_data->probability_of_surviving_up_to_interval.size(); i++) {
						if (survival_data->probability_of_surviving_up_to_interval[i] < .001) {
							if (plotted_zero) {
								survival_curves[survival_curve_id].vals.y[i+ time_offset] = -1;
								continue;
							}
							plotted_zero = true;
							max_t_i = i;
						}
						else max_t_i = i;
						survival_curves[survival_curve_id].vals.y[i + time_offset] = survival_data->probability_of_surviving_up_to_interval[i];
					}
					survival_curves[survival_curve_id].color = ns_rainbow<ns_color_8>(survival_curve_id / (float)all_devices.size());
					survival_curves[survival_curve_id].name = *p;
					survival_curve_id++;
				}
				if (max_t_i < time_axis.size()) {
					time_axis.resize(max_t_i);
					for (unsigned int i = 0; i < survival_curves.size(); i++)
						survival_curves[i].vals.y.resize(max_t_i);
				}

			}
			unsigned long first_valid_timepoint(0);
			for (long i = 0; i < time_axis.size(); i++) {
				if (time_axis[i] >= time_at_which_animals_were_age_zero) {
					first_valid_timepoint = i;
					break;
				}
			}
			time_axis = std::vector<unsigned long>(time_axis.begin() + first_valid_timepoint, time_axis.end());
			for (unsigned int i = 0; i < survival_curves.size(); i++)
				survival_curves[i].vals.y = std::vector<double>(survival_curves[i].vals.y.begin() + first_valid_timepoint, survival_curves[i].vals.y.end());
			
			for (long i = 0; i < time_axis.size(); i++) 
				time_axis[i] -= time_at_which_animals_were_age_zero;
			

			movement_vs_posture_vals.resize(0);


			movement_id_lookup.clear();

			std::vector<ns_color_8> colors;
			for (ns_death_time_annotation_compiler::ns_region_list::const_iterator r = compiler.regions.begin(); r != compiler.regions.end(); r++) {
				if (region_to_view != 0 && r->first != region_to_view)
					continue;
				if (filter_by_strain && r->second.metadata.device_regression_match_description() != data_to_process.device_regression_match_description())
					continue;
				for (ns_death_time_annotation_compiler_region::ns_location_list::const_iterator l = r->second.locations.begin(); l != r->second.locations.end(); l++) {
					ns_dying_animal_description_set_const set;
					l->generate_dying_animal_description_const(true, set);
					for (unsigned int i = 0; i < set.descriptions.size(); i++) {

						std::vector<const ns_death_time_annotation*> x_axis, y_axis;
						if (regression_plot == ns_plot_death_types) {
							if (colors.size() == 0) {
								colors.push_back(ns_color_8(255, 0, 0));
								movement_vs_posture_x_axis_color = ns_color_8(255, 0, 0);
								movement_vs_posture_y_axis_color = ns_color_8(0, 0, 255);
							}
							
							movement_vs_posture_x_axis_label= "Movement Cessation Time (days)"; 
							movement_vs_posture_y_axis_label= "Death-Associated Expansion (days)";
							if (set.descriptions[i].by_hand.death_associated_expansion_start != 0)
								y_axis.push_back(set.descriptions[i].by_hand.death_associated_expansion_start);
							else y_axis.push_back(set.descriptions[i].machine.death_associated_expansion_start);
							if (set.descriptions[i].by_hand.death_annotation != 0)
								x_axis.push_back(set.descriptions[i].by_hand.death_annotation);
							else x_axis.push_back(set.descriptions[i].machine.death_annotation);
						}
						else if (regression_plot == ns_by_hand_machine){
							if (colors.size() == 0) {
								colors.push_back(ns_color_8(255, 0, 0));
								colors.push_back(ns_color_8(0, 0, 255));
								movement_vs_posture_x_axis_color = movement_vs_posture_y_axis_color = ns_color_8(0, 0, 0);
							}

							movement_vs_posture_x_axis_label = "By Hand Event Time (days)";
							movement_vs_posture_y_axis_label = "Machine Event Time (days)";
							x_axis.push_back(set.descriptions[i].by_hand.death_annotation);
							y_axis.push_back(set.descriptions[i].machine.death_annotation);
							x_axis.push_back(set.descriptions[i].by_hand.death_associated_expansion_start);
							y_axis.push_back(set.descriptions[i].machine.death_associated_expansion_start);
						}
						if (x_axis.size() == 0)
							throw ns_ex("Malformed expression");
						movement_vs_posture_vals.resize(x_axis.size());
						for (int i = 0; i < x_axis.size(); i++) {
							if (y_axis[i] != 0 && x_axis[i] != 0 &&
								x_axis[i]->time.best_estimate_event_time_for_possible_partially_unbounded_interval() != 0 &&
								y_axis[i]->time.best_estimate_event_time_for_possible_partially_unbounded_interval() != 0) {
								if (l->properties.is_excluded() || l->properties.is_censored() || l->properties.flag.event_should_be_excluded())
									continue;
								unsigned long mtt(x_axis[i]->time.best_estimate_event_time_for_possible_partially_unbounded_interval()),
									ett(y_axis[i]->time.best_estimate_event_time_for_possible_partially_unbounded_interval());
								double mt(((double)mtt - metadata.time_at_which_animals_had_zero_age) / 60.0 / 60.0 / 24),
									et(((double)ett - metadata.time_at_which_animals_had_zero_age) / 60.0 / 60.0 / 24);
								if (movement_plot == ns_plot_death_times_residual) {
									et -= mt;
									ett -= mtt;
								}

								//cout << "Adding " << mt << "," << et << "\n";
								movement_id_lookup[ns_lookup_index(mtt, ett)].push_back(ns_worm_lookup_info(x_axis[i]->region_info_id, l->properties.stationary_path_id));
								movement_vs_posture_vals[i].vals.x.push_back(mt);
								movement_vs_posture_vals[i].vals.y.push_back(et);
								movement_vs_posture_vals[i].color = colors[i];
							}
						}
					}

				}
			}
		}

		last_graph_contents = ns_none;

	}
	void draw(const ns_graph_contents graph_contents, const ns_vector_2i& position, const ns_vector_2i& graph_size, const ns_vector_2i& buffer_size, const float marker_resize_factor, ns_8_bit* buffer, const unsigned long start_time = 0, const unsigned long stop_time = UINT_MAX) {
		
		if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Starting to draw telemetry."));
		if (survival_image.properties().height == 0 || graph_contents != last_graph_contents || last_start_time != start_time || last_stop_time != stop_time || last_rescale_factor != marker_resize_factor) {
			ns_image_properties prop;
			prop.components = 3;
			if (graph_contents == ns_survival){
				prop.width = graph_size.x - 2 * border().x;
				prop.height = graph_size.y - 2 * border().y;
			}
			else {
				prop.width = graph_size.x / 2 - 3 * border().x;
				prop.height = graph_size.y - 3 * border().y;
			}

			survival_image.init(prop);
			movement_vs_posture_image.init(prop);
			try {

				draw_base_graph(graph_contents, marker_resize_factor, start_time, stop_time);
				last_graph_contents = graph_contents;
				last_start_time = start_time;
				last_stop_time = stop_time;
				last_rescale_factor = marker_resize_factor;
			}
			catch (...) {
				survival_image.init(ns_image_properties(0, 0, 3));
				movement_vs_posture_image.init(ns_image_properties(0, 0, 3));
				throw;
			}
		}
		if (graph_contents == ns_none)
			return;

		
		if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Drawing everything"));

		const unsigned long movement_x_offset(2 * border().x + survival_image.properties().width);
		//top margin
		for (unsigned int y = 0; y < border().y; y++)
			for (unsigned int x = 0; x < graph_size.x; x++)
				for (unsigned int c = 0; c < 3; c++)
					buffer[map_pixel_from_image_onto_buffer(x, y, position, buffer_size) + c] = 0;
		//TOP GRAPH
		for (unsigned int y = 0; y < survival_image.properties().height; y++) {
			//left margin
			for (unsigned int x = 0; x < border().x; x++)
				for (unsigned int c = 0; c < 3; c++)
					buffer[map_pixel_from_image_onto_buffer(x, y + border().y, position, buffer_size) + c] = 0;
			//graph
			for (unsigned int x = 0; x < survival_image.properties().width; x++) {
				for (unsigned int c = 0; c < 3; c++)
					buffer[map_pixel_from_image_onto_buffer(x + border().x, y + border().y, position, buffer_size) + c] = survival_image[y][3 * x + c];
			}

			//right margin
			for (unsigned int x = survival_image.properties().width + border().x; x < movement_x_offset; x++)
				for (unsigned int c = 0; c < 3; c++)
					buffer[map_pixel_from_image_onto_buffer(x, y + border().y, position, buffer_size) + c] = 0;
		}
		//bottom margin
		for (unsigned int y = survival_image.properties().height+border().y; y < graph_size.y; y++)
			for (unsigned int x = 0; x < movement_x_offset; x++)
				for (unsigned int c = 0; c < 3; c++)
					buffer[map_pixel_from_image_onto_buffer(x, y, position, buffer_size) + c] = 0;

		if (graph_contents == ns_population_telemetry::ns_movement_vs_posture ) {
			//BOTTOM GRAPH
			for (unsigned int y = 0; y < movement_vs_posture_image.properties().height; y++) {
				/*//left margin
				for (unsigned int x = 0; x < border().x; x++)
					for (unsigned int c = 0; c < 3; c++)
						buffer[map_pixel_from_image_onto_buffer(x+ movement_x_offset, y +border.y, position, buffer_size) + c] = 0;*/
						//graph
				for (unsigned int x = 0; x < movement_vs_posture_image.properties().width; x++) {
					for (unsigned int c = 0; c < 3; c++)
						buffer[map_pixel_from_image_onto_buffer(x + movement_x_offset, y + border().y, position, buffer_size) + c] = movement_vs_posture_image[y][3 * x + c];
				}

				//right margin
				for (unsigned int x = movement_vs_posture_image.properties().width; x < graph_size.x- movement_x_offset; x++)
					for (unsigned int c = 0; c < 3; c++)
						buffer[map_pixel_from_image_onto_buffer(x + movement_x_offset, y + border().y, position, buffer_size) + c] = 0;
			}
			//bottom margin
			for (unsigned int y = movement_vs_posture_image.properties().height + border().y; y < graph_size.y; y++)
				for (unsigned int x = movement_x_offset; x < graph_size.x; x++)
					for (unsigned int c = 0; c < 3; c++)
						buffer[map_pixel_from_image_onto_buffer(x, y, position, buffer_size) + c] = 0;
		}
		
		overlay_metadata(graph_contents, position, buffer_size, marker_resize_factor, buffer);
	}
};