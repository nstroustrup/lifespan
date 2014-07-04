#include "ns_movement_visualization_generator.h"
#include <iostream>

using namespace std;

void ns_movement_visualization_generator::make_x_axis(const ns_worm_movement_summary_series & movement_data, ns_graph_object & graph_x_axis,const ns_region_metadata & metadata) const{
	const std::vector<ns_worm_movement_measurement_summary> * measurements(&movement_data.measurements);

	graph_x_axis.x.resize(measurements->size());
	for (unsigned int i = 0; i < graph_x_axis.x.size(); i++){
		graph_x_axis.x[i] = ((*measurements)[i].time)/(60.0*60.0*24.0);
	}

}

void ns_movement_visualization_generator::create_survival_curve_for_capture_time(const long marker_time_t, const ns_region_metadata & metadata,
	const ns_survival_data_with_censoring & plate, const ns_survival_data_with_censoring & strain, const std::vector<unsigned long > & plate_time, 
	const std::vector<unsigned long> & strain_time,const std::string & title, const bool draw_dark,const bool optimize_for_small_graph, ns_image_standard & image,ns_graph & graph) const{
	//if (!data.set_is_on_common_time())
	//	throw ns_ex("ns_movement_visualization_generator::create_survival_curve_for_capture_time()::requires data to be on a common time set");
	
	unsigned long plate_last_death(0),strain_last_death(0);
	for (unsigned int i = 0; i < plate.data.number_of_animals_at_risk.size(); i++){
		plate_last_death = i;
		if (plate.data.number_of_animals_at_risk[i] == 0)
			break;
	}
	for (unsigned int i = 0; i < strain.data.number_of_animals_at_risk.size(); i++){
		strain_last_death = i;
		if (strain.data.number_of_animals_at_risk[i] == 0)
			break;
	}
	
	if (plate_time.size() == 0)
		throw ns_ex("ns_movement_visualization_generator::create_survival_curve_for_capture_time()::The supplied plate time vector is empty");

	unsigned long latest_time(plate_time[plate_last_death]);
	if (!strain_time.empty())
	if (latest_time < strain_time[strain_last_death])
		latest_time = strain_time[strain_last_death];

	unsigned long marker_time(marker_time_t);
	if (marker_time > latest_time)
		marker_time = latest_time;

	ns_graph_object	plate_survival(ns_graph_object::ns_graph_dependant_variable),
					strain_survival(ns_graph_object::ns_graph_dependant_variable),
					plate_marker(ns_graph_object::ns_graph_dependant_variable),
					strain_marker(ns_graph_object::ns_graph_dependant_variable),
					censored_markers(ns_graph_object::ns_graph_dependant_variable);

	plate_survival.y.resize(plate_last_death+1+1);
	plate_survival.x.resize(plate_last_death+1+1);
	strain_survival.y.resize(strain_last_death+1+1,0);
	strain_survival.x.resize(strain_last_death+1+1,0);
	//plate_marker.y.resize(plate_time.size()+1,-1);
	//plate_marker.x.resize(plate_time.size()+1,-1);
	//strain_marker.y.resize(strain_time.size()+1,-1);
	//strain_marker.x.resize(strain_time.size()+1,-1);
	unsigned long censored_size = plate_last_death;
	if (censored_size < strain_last_death)
		censored_size = strain_last_death;
	censored_markers.y.resize(censored_size+1+1,-1);
	censored_markers.x.resize(censored_size+1+1,-1);
	plate_marker.y.resize(1,-1);
	plate_marker.x.resize(1,-1);
	strain_marker.y.resize(1,-1);
	strain_marker.x.resize(1,-1);


	
	


	unsigned long plate_marker_index(0),
				  strain_marker_index(0);
	for (unsigned int i = 0; i <= plate_last_death; i++){
		if (marker_time >= plate_time[i]){
			plate_marker_index = i;
		}
		else break;
	}	
	for (unsigned int i = 0; i < strain_last_death; i++){
		if (marker_time >= strain_time[i]){
			strain_marker_index = i;

		}
		else break;
	}

	const unsigned long  total_strain_deaths(strain.data.total_number_of_deaths),
					total_plate_deaths(plate.data.total_number_of_deaths),
					total_strain_censored(strain.data.total_number_of_censoring_events),
					total_plate_censored(plate.data.total_number_of_censoring_events),
					number_of_strain_deaths((strain_marker_index<strain_time.size())?strain.data.cumulative_number_of_deaths[strain_marker_index]:0),
					number_of_plate_deaths((plate_marker_index<plate_time.size())?plate.data.cumulative_number_of_deaths[plate_marker_index]:0);

	const double plate_fraction_surviving((plate_marker_index<plate_time.size())?plate.data.probability_of_surviving_up_to_interval[plate_marker_index]:0),
			strain_fraction_surviving((strain_marker_index<strain_time.size())?strain.data.probability_of_surviving_up_to_interval[strain_marker_index]:0);
	unsigned long number_of_strain_censored(0),
				  number_of_plate_censored(0);
	if (strain_marker_index<= strain_last_death && !strain_time.empty()){
		for (unsigned int i = 0; i <= strain_marker_index; i++)
			number_of_strain_censored+=strain.data.number_of_censoring_events[i];
	}
	if (plate_marker_index<= plate_last_death){
		for (unsigned int i = 0; i <= plate_marker_index; i++)
			number_of_plate_censored+=plate.data.number_of_censoring_events[i];
	}


	plate_survival.y[0] = strain_survival.y[0] = 1;
	censored_markers.x[0] = plate_marker.x[0] = strain_marker.x[0] = plate_survival.x[0] = strain_survival.x[0] = 0;

	for (unsigned int i = 0; i < plate_survival.y.size()-1; i++){
		plate_survival.y[i+1] = plate.data.probability_of_surviving_up_to_interval[i];
		if (plate_survival.y[i] <= 0)
			plate_survival.y[i+1] = -1;
		plate_survival.x[i+1] = (plate_time[i]-metadata.time_at_which_animals_had_zero_age)/60.0/60.0/24.0;
		plate_marker.x[0] = (marker_time-metadata.time_at_which_animals_had_zero_age)/60.0/60.0/24.0;
		censored_markers.x[i+1] = (plate_time[i]-metadata.time_at_which_animals_had_zero_age)/60.0/60.0/24.0;
		if (i == plate_marker_index)
			plate_marker.y[0] = plate.data.probability_of_surviving_up_to_interval[i];
	}
	if (!strain_time.empty())
	for (unsigned int i = 0; i < strain_survival.y.size()-1; i++){
		strain_survival.y[i+1] = strain.data.probability_of_surviving_up_to_interval[i];
		if (strain_survival.y[i] <= 0)
			strain_survival.y[i+1] = -1;
		strain_survival.x[i+1] = (strain_time[i]-metadata.time_at_which_animals_had_zero_age)/60.0/60.0/24.0;
		strain_marker.x[0] = (marker_time-metadata.time_at_which_animals_had_zero_age)/60.0/60.0/24.0;
		if (i == strain_marker_index)
			strain_marker.y[0] = strain.data.probability_of_surviving_up_to_interval[i];
	}
	//for (unsigned int i = 0; i < strain_survival.y.size()-1; i++){
	//	if (strain.data.number_of_censoring_events[i] > 0)
	//		censored_markers.y[i+1] = strain.data.probability_of_surviving_up_to_interval[i];
	//}

	


	//kaplan meyer plots show the number of individuals left alive up to the time point measured.  Thus the marker should							  
	//be at the bottom of the vertical steps on the plot.
	
	

	

	plate_survival.properties.point.draw = false;
	plate_survival.properties.line.draw = true;
	plate_survival.properties.line.width = 4;
	plate_survival.properties.line_hold_order = ns_graph_properties::ns_zeroth;
	plate_survival.properties.draw_vertical_lines = ns_graph_properties::ns_outline;
	plate_survival.properties.draw_negatives = false;
	graph.x_axis_properties.text_decimal_places = 0;
	graph.y_axis_properties.text_decimal_places = 0;
	if (draw_dark){
		graph.x_axis_properties.line.color=ns_color_8(255,255,255);
		graph.x_axis_properties.text.color=ns_color_8(255,255,255);
		graph.x_axis_properties.text.draw = true;
		graph.x_axis_properties.point.color=ns_color_8(255,255,255);
		graph.x_axis_properties.area_fill.color=ns_color_8(0,0,0);
		graph.y_axis_properties = 
			graph.area_properties = 
			graph.title_properties = 
			graph.x_axis_properties;
		
	}
	if (optimize_for_small_graph){
		graph.x_axis_properties.text_size*=4;
		graph.y_axis_properties.text_size*=4;
		graph.area_properties.text_size*=4;
		graph.x_axis_properties.line.width*=4;
		graph.y_axis_properties.line.width*=4;
		graph.area_properties.line.width*=4;
		plate_survival.properties.line.width*=4;
	}
	strain_survival.properties = plate_survival.properties;
	if (draw_dark){
		plate_survival.properties.line.color=ns_color_8(255,255,0);
		strain_survival.properties.line.color=ns_color_8(200,0,0);
	}
	else{
		plate_survival.properties.line.color=ns_color_8(0,0,0);
		strain_survival.properties.line.color=ns_color_8(50,0,0);
	}

	plate_marker.properties = plate_survival.properties;
	plate_marker.properties.line.draw = 0;
	plate_marker.properties.area_fill.draw = 0;
	plate_marker.properties.point.draw = true;
	plate_marker.properties.point.color = plate_marker.properties.area_fill.color;
	plate_marker.properties.point.width = 15 + 10*(int)optimize_for_small_graph;
	plate_marker.properties.point.edge_width = plate_marker.properties.point.width/3;
	plate_marker.properties.point.edge_color = ns_color_8(255,255,255);
	plate_marker.properties.draw_negatives = false;
	censored_markers.properties = strain_marker.properties = plate_marker.properties;
	censored_markers.properties.point.color = plate_survival.properties.line.color;
	censored_markers.properties.point.point_shape = ns_graph_color_set::ns_vertical_line;

	ns_graph_object graph_x_axis(ns_graph_object::ns_graph_independant_variable);

	/*
	unsigned long index_at_which_all_plate_animals_are_dead(strain_survival.y.size());
	for (unsigned int i = 0; i < strain_survival.y.size(); i++){
		if (strain_survival.y[i] < .001){
			index_at_which_all_plate_animals_are_dead = i;
		}
	}
	index_at_which_all_plate_animals_are_dead=(5*index_at_which_all_plate_animals_are_dead)/4;
	if (index_at_which_all_plate_animals_are_dead>strain_survival.y.size())
		index_at_which_all_plate_animals_are_dead = strain_survival.y.size();

	graph_x_axis.x.resize(index_at_which_all_plate_animals_are_dead );
	strain_survival.y.resize(index_at_which_all_plate_animals_are_dead );
	strain_marker.y.resize(index_at_which_all_plate_animals_are_dead );
	plate_survival.y.resize(index_at_which_all_plate_animals_are_dead );
	plate_marker.y.resize(index_at_which_all_plate_animals_are_dead );
	*/

//	graph.contents.push_back(graph_x_axis);
	graph.contents.push_back(censored_markers);
	if (!strain_time.empty()){
		graph.contents.push_back(strain_survival);
		graph.contents.push_back(strain_marker);
	}
	graph.contents.push_back(plate_survival);
	graph.contents.push_back(plate_marker);

	
	if (image.properties().height > 0){
		const unsigned long number_of_lines(7);
		ns_font & font(font_server.default_font());
		font.set_height(image.properties().height/ number_of_lines);

		for (unsigned int y = 0; y <  image.properties().height; y++){
			for (unsigned int x = 0; x <  3*image.properties().width; x++){
				image[y][x] = 0;
			}
		}
		string text;
		text = "Age: ";
		const unsigned long seconds_since_birth(marker_time - metadata.time_at_which_animals_had_zero_age);
		float age = seconds_since_birth/(24*60*60.0);
		string units(" days");
		if (seconds_since_birth < 4*24*60*60){
			age = seconds_since_birth/(60*60.0);
			units = " hours";
		}
		unsigned long line_num(1);
		text += ns_to_string_short(age,2) + units;
		font.draw_color(8,line_num*image.properties().height/(number_of_lines-1),ns_color_8(255,255,255),text,image);
		line_num++;
		text = "Plate Survival: ";
		text += ns_to_string_short(plate_fraction_surviving,2);
		font.draw_color(8,line_num*image.properties().height/(number_of_lines-1),ns_color_8(255,255,255),text,image);
		line_num++;
		text = "Plate Dead: ";
		text += ns_to_string( number_of_strain_deaths);
		text += "/";
		text += ns_to_string( total_strain_deaths);
		font.draw_color(8,line_num*image.properties().height/(number_of_lines-1),ns_color_8(255,255,255),text,image);
		line_num++;
		text = "Plate Censored: ";
		text += ns_to_string( number_of_strain_censored);
		text += "/";
		text += ns_to_string( total_strain_censored);
		font.draw_color(8,line_num*image.properties().height/(number_of_lines-1),ns_color_8(255,255,255),text,image);
		line_num++;
		if (!strain_time.empty()){
			text = "Strain Survival: ";
			text += ns_to_string_short(strain_fraction_surviving,2);
			font.draw_color(8,line_num*image.properties().height/(number_of_lines-1),ns_color_8(255,255,255),text,image);
		}
	}

}
void ns_movement_visualization_generator::create_area_graph_for_capture_time(const long marker_time, const ns_worm_movement_summary_series & data, ns_graph & graph,  const std::string & title, const bool draw_dark,const bool optimize_for_small_graph,const ns_region_metadata & metadata) const{
	graph.clear();
	unsigned int number_of_measurements = (unsigned int)data.measurements.size();
	unsigned int marker_position = 0;
	if (marker_time == -1)
		marker_position = (unsigned int)data.measurements.size();
	else{
		for (unsigned int i = 0; i < data.measurements.size(); i++){
			if (data.measurements[i].time >= (unsigned long)marker_time){
				marker_position= i;
				break;
			}
		}
	}

	std::vector<ns_graph_object> worm_states_flat((int)ns_movement_total,ns_graph_object(ns_graph_object::ns_graph_dependant_variable));
	std::vector<ns_graph_object> worm_states_sum((int)ns_movement_total,ns_graph_object(ns_graph_object::ns_graph_dependant_variable));
	std::vector<ns_graph_object> current_position_markers((int)ns_movement_total,ns_graph_object(ns_graph_object::ns_graph_dependant_variable));
	
	for (unsigned int i = 0; i < worm_states_flat.size(); i++){
		worm_states_flat[i].y.resize(number_of_measurements);
		worm_states_sum[i].y.resize(number_of_measurements);
		current_position_markers[i].y.resize(number_of_measurements,-1);
	}
	
	for (unsigned int i = 0; i < number_of_measurements; i++){
		worm_states_flat[(int)ns_movement_stationary].y[i]	= data.measurements[i].all_measurement_types_total.number_stationary;
		worm_states_flat[(int)ns_movement_posture].y[i]		= data.measurements[i].all_measurement_types_total.number_changing_posture;
		worm_states_flat[(int)ns_movement_slow].y[i]		= data.measurements[i].all_measurement_types_total.number_moving_slow;
		worm_states_flat[(int)ns_movement_fast].y[i]		= data.measurements[i].all_measurement_types_total.number_moving_fast;
		worm_states_flat[(int)ns_movement_machine_excluded].y[i]	= data.measurements[i].all_measurement_types_excluded_total.total_animals_inferred();
		worm_states_flat[(int)ns_movement_by_hand_excluded].y[i]		= 0;//data.measurements[i].all_measurement_types_total.number_by_hand_excluded;
		worm_states_flat[(int)ns_movement_death_posture_relaxation].y[i]  = 0;//data.measurements[i].all_measurement_types_total.number_death_posture_relaxing;
	}

	for (unsigned int i = 0; i < number_of_measurements; i++){
		for (unsigned int s= 0; s < (unsigned int)ns_movement_total; s++)
				worm_states_sum[s].y[i] = 0;
	}
	
	std::vector<ns_movement_state> area_order((unsigned int)ns_movement_total,ns_movement_not_calculated);
	area_order[0]=ns_movement_stationary;
	area_order[1]=ns_movement_posture;
	area_order[2]=ns_movement_slow;
	area_order[3]=ns_movement_fast;
	area_order[4]=ns_movement_death_posture_relaxation;
	area_order[5]=ns_movement_machine_excluded;
	area_order[6]=ns_movement_by_hand_excluded;

	for (unsigned int i = 0; i < number_of_measurements; i++){
		for (unsigned int s= 0; s < (unsigned int)ns_movement_total; s++){
			const ns_movement_state & cur_state(area_order[s]);
			for (unsigned int j = 0; j <= s; j++){
				const ns_movement_state & sum_state(area_order[j]);
				worm_states_sum[(int)cur_state].y[i] += worm_states_flat[(int)sum_state].y[i];
			}
		}
	}

	if (marker_position < current_position_markers[0].y.size()){
		for (unsigned int i = 0; i < current_position_markers.size(); i++){
		//	cerr << "cpm[" << i << "] = " <<  worm_states_flat[i].y[marker_position] << "\n";
			if (marker_time!= -1)
				current_position_markers[i].y[marker_position] = worm_states_sum[i].y[marker_position];
		}
	}

	/*for (unsigned int i = 0; i < number_of_measurements; i++)
		for (unsigned int s= 0; s < (unsigned int)worm_states_flat.size(); s++)
			worm_states_sum[s].y[i] += worm_states_flat[s].y[i];*/

	for (unsigned int i = 0; i < worm_states_sum.size(); i++)
		worm_states_sum[i].properties.area_fill.color = ns_movement_colors::color((ns_movement_state)i);

	if (draw_dark)
		worm_states_sum[(int)ns_movement_stationary].properties.area_fill.color= worm_states_sum[(int)ns_movement_stationary].properties.area_fill.color/2;




	for (unsigned int i = 0; i < (unsigned int)worm_states_flat.size(); i++){
		//worm_states_sum[i].properties.area_fill.draw = true;
		//worm_states_sum[i].properties.area_fill.opacity = 1;
		//worm_states_sum[i].properties.line.color = worm_states_sum[i].properties.area_fill.color/2;
		if (!optimize_for_small_graph){
			worm_states_sum[i].properties.area_fill.draw = true;
			worm_states_sum[i].properties.area_fill.opacity = 1;
			worm_states_sum[i].properties.line.color = ns_color_8(150,150,150);//worm_states_sum[i].properties.area_fill.color;
			worm_states_sum[i].properties.line.draw = false;
			worm_states_sum[i].properties.line.width = 2;
			worm_states_sum[i].properties.line_hold_order = ns_graph_properties::ns_zeroth;
		}
		else{
			worm_states_sum[i].properties.line.color = worm_states_sum[i].properties.area_fill.color;
			worm_states_sum[i].properties.area_fill.draw = false;
			worm_states_sum[i].properties.line.draw = true;
			worm_states_sum[i].properties.line.width = 5;
			worm_states_sum[i].properties.line_hold_order = ns_graph_properties::ns_first;
		}
		worm_states_sum[i].properties.point.draw = false;
	}

	/*worm_states_flat[(int)ns_movement_total].properties.line.color=ns_color_8(0,0,0);
	worm_states_flat[(int)ns_movement_total].properties.area_fill.draw = false;
	worm_states_flat[(int)ns_movement_total].properties.line.draw = true;
	worm_states_flat[(int)ns_movement_total].properties.line_hold_order = ns_graph_properties::ns_zeroth;
	worm_states_flat[(int)ns_movement_total].properties.line.width = 2;
	worm_states_flat[(int)ns_movement_total].properties.point.draw = false;*/

	
	for(unsigned int i = 0; i < (unsigned int)worm_states_sum.size(); i++){
		current_position_markers[i].properties = worm_states_sum[i].properties;
		current_position_markers[i].properties.line.draw = false;
		current_position_markers[i].properties.area_fill.draw = false;
		current_position_markers[i].properties.point.draw = true;
		current_position_markers[i].properties.point.color = current_position_markers[i].properties.area_fill.color;
		current_position_markers[i].properties.point.width = 10 + 10*(int)optimize_for_small_graph;
		current_position_markers[i].properties.point.edge_width = current_position_markers[i].properties.point.width/3;
		current_position_markers[i].properties.point.edge_color = ns_color_8(255,255,255);
		current_position_markers[i].properties.draw_negatives = false;
	}

	graph.x_axis_properties.text_decimal_places = 0;
	graph.y_axis_properties.text_decimal_places = 0;
	if (draw_dark){
		graph.x_axis_properties.line.color=ns_color_8(255,255,255);
		graph.x_axis_properties.text.color=ns_color_8(255,255,255);
		graph.x_axis_properties.text.draw = true;
		graph.x_axis_properties.point.color=ns_color_8(255,255,255);
		graph.x_axis_properties.area_fill.color=ns_color_8(0,0,0);
		graph.y_axis_properties = 
			graph.area_properties = 
			graph.title_properties = 
			graph.x_axis_properties;
	}
	
	if (optimize_for_small_graph){
		graph.x_axis_properties.text_size*=4;
		graph.y_axis_properties.text_size*=4;
		graph.area_properties.text_size*=4;
		graph.x_axis_properties.line.width*=4;
		graph.y_axis_properties.line.width*=4;
		graph.area_properties.line.width*=4;
	}

	ns_graph_object graph_x_axis(ns_graph_object::ns_graph_independant_variable);
	make_x_axis(data,graph_x_axis,metadata);

	graph.contents.push_back(graph_x_axis);
	
	for (int i = (int)worm_states_flat.size()-1; i >= 0; i--)
		graph.contents.push_back(worm_states_sum[i]);
	if (marker_time != -1){
		for (int i = 0; i < (int)current_position_markers.size(); i++)
			graph.contents.push_back(current_position_markers[i]);
	}
	ns_graph_axes axes;
	axes.tick(0) = 2;
	axes.tick(0) = 1;
	axes.boundary(0) = 0;
	graph.set_graph_display_options(title,axes,1.5);
}


void ns_movement_visualization_generator::create_scatter_proportion_graph_for_capture_time(const long marker_time, const ns_worm_movement_summary_series & data, ns_graph & graph,  const std::string & title, const bool draw_dark,const bool optimize_for_small_graph,const ns_region_metadata & metadata) const{
	graph.clear();
	if (data.measurements.size() == 0)
		return;
	std::vector<ns_worm_movement_measurement_summary> r;
	r.insert(r.begin(),data.measurements.begin(),data.measurements.end());

	ns_graph_object graph_x_axis(ns_graph_object::ns_graph_independant_variable);
	make_x_axis(data,graph_x_axis,metadata);

	//do not include points in which no worms are found
	std::vector<double>::iterator x = graph_x_axis.x.begin();
	for (std::vector<ns_worm_movement_measurement_summary>::iterator p = r.begin(); p != r.end();){
		if (p->all_measurement_types_total.total_animals_inferred() == 0){
			p = r.erase(p);
			x = graph_x_axis.x.erase(x);
		}
		else {
			p++;
			x++;
		}
	}
	
	std::vector<ns_graph_object> worm_states_percent((int)ns_movement_total,ns_graph_object(ns_graph_object::ns_graph_dependant_variable));
	ns_graph_object total_worms(ns_graph_object::ns_graph_dependant_variable);
	ns_graph_object number_of_worms_involving_multiple_worm_disambiguation(ns_graph_object::ns_graph_dependant_variable);

	for (unsigned int i = 0; i < worm_states_percent.size(); i++){
		worm_states_percent[i].y.resize(r.size());
		worm_states_percent[i].hyperlinks.resize(r.size());
	}
	
	std::string hyperlink = "http://fontanacluster.med.harvard.edu/image_server_web/ns_view_region_images.php?region_image_id=";
	total_worms.y.resize(r.size());
	number_of_worms_involving_multiple_worm_disambiguation.y.resize(r.size());

	//total_worms.hyperlinks.resize(number_of_measurements);
	for (unsigned int i = 0; i < r.size(); i++){
		total_worms.y[i] =	r[i].all_measurement_types_total.total_animals_inferred();
		//total_worms.hyperlinks[i] = hyperlink + ns_to_string(r[i].region_short_1.region_images_id);
	}

	for (unsigned int i = 0; i < r.size(); i++){

		worm_states_percent[(int)ns_movement_stationary].y[i]	= r[i].all_measurement_types_total.number_stationary / total_worms.y[i];
		worm_states_percent[(int)ns_movement_posture].y[i]		= r[i].all_measurement_types_total.number_changing_posture / total_worms.y[i];
		worm_states_percent[(int)ns_movement_slow].y[i]			= r[i].all_measurement_types_total.number_moving_slow / total_worms.y[i];
		worm_states_percent[(int)ns_movement_fast].y[i]			= r[i].all_measurement_types_total.number_moving_fast / total_worms.y[i];
		worm_states_percent[(int)ns_movement_machine_excluded].y[i]			= r[i].all_measurement_types_excluded_total.total_animals_inferred()/ (r[i].all_measurement_types_excluded_total.total_animals_inferred() + total_worms.y[i]);
		worm_states_percent[(int)ns_movement_by_hand_excluded].y[i]			= 0;//r[i].number_by_hand_excluded/ (r[i].all_measurement_types_excluded_total.number_total + total_worms.y[i]);
		number_of_worms_involving_multiple_worm_disambiguation.y[i]	=	0;//r[i].number_of_worms_involving_multiple_worm_disambiguation/(r[i].number_machine_excluded+r[i].number_by_hand_excluded + total_worms.y[i]);
	//	worm_states_percent[(int)ns_movement_stationary].hyperlinks[i]	=
	//	worm_states_percent[(int)ns_movement_posture].hyperlinks[i]	=
	//	worm_states_percent[(int)ns_movement_slow].hyperlinks[i]		=
	//	worm_states_percent[(int)ns_movement_fast].hyperlinks[i]		=	hyperlink + ns_to_string(r[i].region_short_1.region_images_id);
	}

	
	for (unsigned int i = 0; i < worm_states_percent.size(); i++)
		worm_states_percent[i].properties.point.color = ns_movement_colors::color((ns_movement_state)i);

	for (unsigned int i = 0; i < (unsigned int)worm_states_percent.size(); i++){
		worm_states_percent[i].properties.point.edge_width = 0;
		worm_states_percent[i].properties.area_fill.draw = false;
		worm_states_percent[i].properties.line.draw = true;
		worm_states_percent[i].properties.line.color = worm_states_percent[i].properties.point.color;
		worm_states_percent[i].properties.line.width = 2;
		if (worm_states_percent[i].y.size() <= 100)
			worm_states_percent[i].properties.point.draw = true;
		else 
			worm_states_percent[i].properties.point.draw = false;
		worm_states_percent[i].properties.point.width = 6;
		if (optimize_for_small_graph){
			worm_states_percent[i].properties.point.width = 12;
			total_worms.properties.text_size*=4;
		}
	}
	
	number_of_worms_involving_multiple_worm_disambiguation.properties = worm_states_percent[0].properties;
	number_of_worms_involving_multiple_worm_disambiguation.properties.line.color = 
		number_of_worms_involving_multiple_worm_disambiguation.properties.point.color = ns_color_8(0,0,125);

	total_worms.properties.area_fill.draw = false;
	total_worms.properties.line.draw = false;
	total_worms.properties.point.draw = true;
	total_worms.properties.point.width = 3;
	total_worms.properties.point.color= ns_color_8(128,128,128);
	if (optimize_for_small_graph){
		total_worms.properties.point.width = 12;
		total_worms.properties.point.color= ns_color_8(200,200,200);
	}

	ns_graph &states_graph(graph);
	ns_graph total_graph;	

	if (draw_dark){
		states_graph.x_axis_properties.line.color=ns_color_8(255,255,255);
		states_graph.x_axis_properties.text.color=ns_color_8(255,255,255);
		states_graph.x_axis_properties.point.color=ns_color_8(255,255,255);
		states_graph.x_axis_properties.area_fill.color=ns_color_8(0,0,0);
		states_graph.x_axis_properties.text_decimal_places = 0;


		states_graph.y_axis_properties = 
			states_graph.area_properties =
			states_graph.title_properties = 
			total_graph.x_axis_properties = 
			total_graph.y_axis_properties = 
			total_graph.area_properties = 
			total_graph.title_properties = states_graph.x_axis_properties;			
		states_graph.y_axis_properties.text_decimal_places = 1;
	}
	if (optimize_for_small_graph){
		states_graph.x_axis_properties.text_size*=5;
		states_graph.title_properties.text_size*=5;
	}
	else
		total_graph.y_axis_properties.text_size= (unsigned long)(0.75*total_graph.y_axis_properties.text_size);
	

	states_graph.contents.push_back(graph_x_axis);
	total_graph.contents.push_back(graph_x_axis);
	states_graph.contents.push_back(number_of_worms_involving_multiple_worm_disambiguation);

	for (int i = (int)worm_states_percent.size()-1; i >= 0; i--)
		states_graph.contents.push_back(worm_states_percent[i]);
	total_graph.contents.push_back(total_worms);
	ns_graph_axes states_axes;
	states_axes.tick(0)=1;
	states_axes.tick(2)=.1 ;
	//axes.tick(2)=5;
	///axes.tick(3)=1;
	states_axes.boundary(2) = 0;
	states_axes.boundary(3) = 1;
	
	ns_graph_axes total_axes;
	total_axes.boundary(2) = 0;
	total_axes.boundary(3) = data.total_summary_statistics.maximum_count_for_movement_state[(int)ns_movement_total];
	
	states_graph.set_graph_display_options(title,states_axes,1.5);
//	total_graph.set_graph_display_options(title,states_axes,.75);

	//states_graph.concatenate(total_graph);
}


	
void ns_movement_visualization_generator::create_time_path_analysis_visualization(const ns_image_server_captured_image_region & region_image, const ns_death_time_annotation_compiler_region & compiler_region,const ns_image_standard & grayscale, ns_image_standard & out,ns_sql & sql){
	unsigned long thickness = 4;
	
	const ns_color_8 excluded_color = ns_color_8(50,50,255);

	ns_image_properties prop(grayscale.properties());
	prop.components = 3;
	out.init(prop);
	for (unsigned int y = 0; y < prop.height; ++y){
		for (unsigned long x = 0; x < prop.width; ++x){
			out[y][3*x+0] = 255-grayscale[y][x];
			out[y][3*x+1] = 255-grayscale[y][x];
			out[y][3*x+2] = 255-grayscale[y][x];
		}
	}


/*	ns_death_time_annotation_set movement_annotations;
//	movement_annotations.events.reserve(death_time_annotations.size());
//	for (unsigned long i = 0; i < death_time_annotations.size(); i++){

		switch(death_time_annotations[i].type){
			case ns_fast_moving_worm_observed:
			case ns_slow_moving_worm_observed:
			case ns_posture_changing_worm_observed:
			case ns_stationary_worm_observed:
					movement_annotations.push_back(death_time_annotations[i]);
			}
	}
*/
	
	ns_image_worm_detection_results results;
	results.id = region_image.region_detection_results_id;
	results.load_from_db(false,false,sql);
	ns_image_server_captured_image_region region_t(region_image);
	results.load_images_from_db(region_t,sql);
	const std::vector<const ns_detected_worm_info *> detected_worms(results.actual_worm_list());
	ns_font & font(font_server.default_font());
	const unsigned long font_height(25);
	font.set_height(font_height);

	std::vector<const ns_death_time_annotation *> representative_state_event_for_location(compiler_region.locations.size(),0);
	for (unsigned int i = 0; i < compiler_region.locations.size(); i++){
		for (unsigned int j = 0; j < compiler_region.locations[i].annotations.size(); j++){
			if (ns_movement_event_is_a_state_observation(compiler_region.locations[i].annotations[j].type) && 
								compiler_region.locations[i].annotations[j].time.period_end == region_image.capture_time){
				if (representative_state_event_for_location[i] != 0){
					cerr << "Found multiple state events for a time!\n";
				}
				else representative_state_event_for_location[i] = &compiler_region.locations[i].annotations[j];
			}
		}
	}
	
	std::vector<const ns_death_time_annotation *> fast_moving_animal_matches(detected_worms.size(),0);
	for (unsigned int i = 0; i < compiler_region.fast_moving_animals.size(); i++){
		for (unsigned long w = 0; w < detected_worms.size(); w++){
			if (detected_worms[w]->region_size == compiler_region.fast_moving_animals[i].size &&
				detected_worms[w]->region_position_in_source_image == compiler_region.fast_moving_animals[i].position){
				fast_moving_animal_matches[w] = &compiler_region.fast_moving_animals[i];
			}
		}
	}

	//for debugging
	std::vector<char> locations_matched(representative_state_event_for_location.size(),0);
	unsigned long locations_with_matches(0);
	//for real use
	std::vector<ns_death_time_annotation_compiler_region::ns_location_list::const_iterator> location_matches(detected_worms.size(),compiler_region.locations.end());
	unsigned long unmatched_detected_worms(0);
	for (unsigned long w = 0; w < detected_worms.size(); w++){
		for (unsigned int i = 0; i < representative_state_event_for_location.size(); i++){
			if (representative_state_event_for_location[i] == 0)
				continue;
			if (detected_worms[w]->region_size == representative_state_event_for_location[i]->size &&
				detected_worms[w]->region_position_in_source_image == representative_state_event_for_location[i]->position){
					if(location_matches[w] != compiler_region.locations.end()){
						cerr << "Found multiple locations that match detected worm!\n";
					}
					else{
						//for debugging
						locations_matched[i] = 1;
						locations_with_matches++;
						//for real use
						location_matches[w] = compiler_region.locations.begin()+i;
					}
			}
		}
		if (fast_moving_animal_matches[w] == 0 && location_matches[w]== compiler_region.locations.end())
			unmatched_detected_worms++;
	}
	if (unmatched_detected_worms > 0)
		cerr << "Could not match up " << unmatched_detected_worms << " of " << detected_worms.size() << " animals.\n";
	
	for (unsigned long w = 0; w < detected_worms.size(); w++){
		const ns_death_time_annotation_compiler_region::ns_location_list::const_iterator location(location_matches[w]);
		const ns_death_time_annotation * fast_animal_match(fast_moving_animal_matches[w]);
		const ns_vector_2i & pos = detected_worms[w]->region_position_in_source_image;
		//if we can't find info on the object, paint it white.
		if (fast_animal_match == 0 && location== compiler_region.locations.end()){
			const ns_color_8 color(255,255,255);
			ns_vector_2i size = detected_worms[w]->region_size;
			out.draw_line_color_thick(pos,pos+ns_vector_2i(size.x,0),color,3);
			out.draw_line_color_thick(pos,pos+ns_vector_2i(0,size.y),color,3);
			out.draw_line_color_thick(pos+ns_vector_2i(0,size.y),pos+size,color,3);
			out.draw_line_color_thick(pos+ns_vector_2i(size.x,0),pos+size,color,3);
			
			continue;
		}
		ns_color_8  color;
		if (location != compiler_region.locations.end()){
			color = (ns_movement_colors::color(ns_movement_event_state(representative_state_event_for_location[location-compiler_region.locations.begin()]->type)));
			if (location->properties.is_excluded())
			color = excluded_color;
		}
		else color = ns_movement_colors::color(ns_movement_fast);


		for (unsigned int y = 0; y < detected_worms[w]->bitmap().properties().height; y++){
			for (unsigned int x = 0; x < detected_worms[w]->bitmap().properties().width; x++){
				if (detected_worms[w]->bitmap()[y][x]){
					out[pos.y + y][3*(pos.x + x)+0]
						= (ns_8_bit)(.75*color.x + .25*out[pos.y + y][3*(pos.x + x)+0]);
					out[pos.y + y][3*(pos.x + x)+1]
						= (ns_8_bit)(.75*color.y + .25*out[pos.y + y][3*(pos.x + x)+1]);
					out[pos.y + y][3*(pos.x + x)+2]
						= (ns_8_bit)(.75*color.z + .25*out[pos.y + y][3*(pos.x + x)+2]);
				}
			}
		}
		ns_color_8 edge_color(color);
		if (location != compiler_region.locations.end() &&
			location->properties.number_of_worms_at_location_marked_by_hand > 1){
		//	font.draw_color(pos.x + (detected_worms[w]->region_size.x*3)/4,pos.y+font_height,ns_color_8(255,255,255),std::string("") + 
		//		ns_to_string(location->properties.number_of_worms_at_location_marked_by_hand),out);
			edge_color = ns_color_8(255,180,120);
			const int edge_width(3);
			// use signed ints for x and y so that "x + dx" type expressions are also properly signed.
			for (int y = 0; y < detected_worms[w]->edge_bitmap().properties().height; y++){
				for (int x = 0; x < detected_worms[w]->edge_bitmap().properties().width; x++){
					if (detected_worms[w]->edge_bitmap()[y][x]){
						for (int dx = -edge_width; dx <= edge_width; dx++)
							for (int dy = -edge_width; dy <= edge_width; dy++){
								if ( dx + dy > edge_width)
									continue; //round edges
								if (x + dx < 0 || x + dx >= detected_worms[w]->edge_bitmap().properties().width)
									continue;
								if (y + dy < 0 || y + dy >= detected_worms[w]->edge_bitmap().properties().height)
									continue;
								out[pos.y + y+dy][3*(pos.x + x+dx)+0]
									= (ns_8_bit)(.2*(ns_8_bit)(edge_color.x) + .8*out[pos.y + y+dy][3*(pos.x + x+dx)+0]);
								out[pos.y + y+dy][3*(pos.x + x+dx)+1]
									= (ns_8_bit)(.2*(ns_8_bit)(edge_color.y) + .8*out[pos.y + y+dy][3*(pos.x + x+dx)+1]);
								out[pos.y + y+dy][3*(pos.x + x+dx)+2]
									= (ns_8_bit)(.2*(ns_8_bit)(edge_color.z) + .8*out[pos.y + y+dy][3*(pos.x + x+dx)+2]);
							}
					}
				}
			}

		}	
	}
	for (unsigned int i = 0; i < compiler_region.locations.size(); i++){
		if (representative_state_event_for_location[i] == 0 || locations_matched[i])
			continue;
		const ns_death_time_annotation & a(*representative_state_event_for_location[i]);
		if (compiler_region.locations[i].properties.inferred_animal_location){
			ns_color_8 color = ns_movement_colors::color(ns_movement_event_state(a.type));
			if (compiler_region.locations[i].properties.is_excluded())
				color = excluded_color;
			unsigned long thickness = 4;
			out.draw_line_color_thick(a.position,a.position + ns_vector_2i(a.size.x,0),color,thickness,.8);
			out.draw_line_color_thick(a.position,a.position + ns_vector_2i(0,a.size.y),color,thickness,.8);
			out.draw_line_color_thick(a.position+ns_vector_2i(a.size.x,0),a.position + ns_vector_2i(a.size.x,a.size.y),color,thickness,.6);
			out.draw_line_color_thick(a.position+ns_vector_2i(0,a.size.y),a.position + ns_vector_2i(a.size.x,a.size.y),color,thickness,.6);
		}
	}
}