#include "ns_experiment_storyboard.h"
#include "ns_detected_worm_info.h"
#include "ns_by_hand_lifespan.h"
#include "ns_time_path_image_analyzer.h"
#include "ns_hand_annotation_loader.h"
#include "ns_machine_analysis_data_loader.h"

#include "ns_font.h"
#include "ns_xml.h"

using namespace std;

//triangle drawing from http://www.sunshine2k.de/coding/java/TriangleRasterization/TriangleRasterization.html
void ns_fill_bottom_flat_triangle(const ns_vector_2i & t1, const ns_vector_2i & b2, const ns_vector_2i & b3,const ns_color_8 &c,const double & opacity,ns_image_standard & im){
  float invslope1 = (b2.x - t1.x) / (float)(b2.y - t1.y);
  float invslope2 = (b3.x - t1.x) / (float)(b3.y - t1.y);

  float curx1 = t1.x;
  float curx2 = t1.x;
  const bool fwds(invslope1 < invslope2);
  if (fwds){
	  for (int scanlineY = t1.y; scanlineY <= b2.y; scanlineY++)
	  {
		for (unsigned int x = curx1; x < curx2; x++){
			im[scanlineY][3*x+0] = (1-opacity)*im[scanlineY][3*x+0] + opacity*c.x;
			im[scanlineY][3*x+1] = (1-opacity)*im[scanlineY][3*x+1] + opacity*c.y;
			im[scanlineY][3*x+2] = (1-opacity)*im[scanlineY][3*x+2] + opacity*c.z;
		}
		curx1 += invslope1;
		curx2 += invslope2;
	  }
  }
  else{
	    for (int scanlineY = t1.y; scanlineY <= b2.y; scanlineY++)
	  {
		for (unsigned int x = curx2; x < curx1; x++){
			im[scanlineY][3*x+0] = (1-opacity)*im[scanlineY][3*x+0] + opacity*c.x;
			im[scanlineY][3*x+1] = (1-opacity)*im[scanlineY][3*x+1] + opacity*c.y;
			im[scanlineY][3*x+2] = (1-opacity)*im[scanlineY][3*x+2] + opacity*c.z;
		}
		curx1 += invslope1;
		curx2 += invslope2;
	  }
  }
}

void ns_fill_top_flat_triangle(const ns_vector_2i & t1, const ns_vector_2i & t2, const ns_vector_2i & b3,const ns_color_8 &c,const double & opacity,ns_image_standard & im){
  float invslope1 = (b3.x - t1.x) / (float)(b3.y - t1.y);
  float invslope2 = (b3.x - t2.x) / (float)(b3.y - t2.y);

  float curx1 = b3.x;
  float curx2 = b3.x;

  const bool fwds(invslope1 > invslope2);
  if (fwds){
	  for (int scanlineY = b3.y; scanlineY > t1.y; scanlineY--)
	  {
		curx1 -= invslope1;
		curx2 -= invslope2; 
		for (unsigned int x = curx1; x < curx2; x++){
			im[scanlineY][3*x+0] = (1-opacity)*im[scanlineY][3*x+0] + opacity*c.x;
			im[scanlineY][3*x+1] = (1-opacity)*im[scanlineY][3*x+1] + opacity*c.y;
			im[scanlineY][3*x+2] = (1-opacity)*im[scanlineY][3*x+2] + opacity*c.z;
		}
	  }
  }
  else{
	   for (int scanlineY = b3.y; scanlineY > t1.y; scanlineY--){
			curx1 -= invslope1;
			curx2 -= invslope2; 
			for (unsigned int x = curx2; x < curx1; x++){
				im[scanlineY][3*x+0] = (1-opacity)*im[scanlineY][3*x+0] + opacity*c.x;
				im[scanlineY][3*x+1] = (1-opacity)*im[scanlineY][3*x+1] + opacity*c.y;
				im[scanlineY][3*x+2] = (1-opacity)*im[scanlineY][3*x+2] + opacity*c.z;
			}
		}
  }
}

bool operator <(const ns_experiment_storyboard_timepoint_element & a, const ns_experiment_storyboard_timepoint_element & b){
	if (a.event_annotation.time.period_end_was_not_observed != b.event_annotation.time.period_end_was_not_observed)
		return a.event_annotation.time.period_end_was_not_observed < b.event_annotation.time.period_end_was_not_observed;	
	
	if (!a.event_annotation.time.period_end_was_not_observed && a.event_annotation.time.period_end != b.event_annotation.time.period_end)
		return a.event_annotation.time.period_end < b.event_annotation.time.period_end;

	if (a.event_annotation.time.period_start_was_not_observed != b.event_annotation.time.period_start_was_not_observed)
		return a.event_annotation.time.period_start_was_not_observed > b.event_annotation.time.period_start_was_not_observed;
	if (!a.event_annotation.time.period_start_was_not_observed && a.event_annotation.time.period_start != b.event_annotation.time.period_start)
		return a.event_annotation.time.period_start < b.event_annotation.time.period_start;
	return false;
}

#define MAX_SUB_IMAGE_WIDTH 4000
#define INTER_COLUMN_MARGIN 8

struct ns_results_lookup{
	ns_experiment_storyboard_timepoint_element * e;
	ns_64_bit region_id;
	ns_64_bit results_id,interpolated_results_id;

	bool operator() (const ns_results_lookup & a,const ns_results_lookup & b){
		return a.results_id < b.results_id;
	}
};
void ns_experiment_storyboard_timepoint::load_images(bool use_color,ns_sql & sql){
		
	const string col(ns_processing_step_db_column_name(ns_process_region_vis)),
			col2(ns_processing_step_db_column_name(ns_process_region_interpolation_vis));
	vector<ns_results_lookup> sorted_events(events.size());
	unsigned long number_of_events_with_detection_performed(0);
	for (unsigned int i = 0; i < events.size(); i++){

		unsigned long event_time;
		if (events[i].annotation_whose_image_should_be_used.time.fully_unbounded())
			throw ns_ex("Attempting to load image for fully unbounded event!");
		if (events[i].annotation_whose_image_should_be_used.time.period_end_was_not_observed)
			event_time = events[i].annotation_whose_image_should_be_used.time.period_start;
		else 
			event_time = events[i].annotation_whose_image_should_be_used.time.period_end;
		sql << "SELECT id, worm_detection_results_id,problem, worm_interpolation_results_id FROM sample_region_images WHERE "
			"region_info_id = " << events[i].annotation_whose_image_should_be_used.region_info_id
			<< " AND capture_time = " << event_time;
		
		ns_sql_result res;
		sql.get_rows(res);
		if (res.size() == 0)
			throw ns_ex("Could not load worm detection information for region with info_id ") << events[i].event_annotation.region_info_id << " AND capture_time = " << event_time;

		sorted_events[i].e = & events[i];
		sorted_events[i].region_id = ns_atoi64(res[0][0].c_str());
		sorted_events[i].results_id = ns_atoi64(res[0][1].c_str());
		sorted_events[i].interpolated_results_id = ns_atoi64(res[0][3].c_str());
		number_of_events_with_detection_performed += (sorted_events[i].results_id>0?1:0);
	}
	if (number_of_events_with_detection_performed != events.size()){
		throw ns_ex("Only ") << number_of_events_with_detection_performed << " of " << events.size() << " images have been processed to detect worms.";
	}
	std::sort(sorted_events.begin(),sorted_events.end(),ns_results_lookup());

	
	ns_image_worm_detection_results results,interpolated_results;
	for (unsigned int i = 0; i < sorted_events.size(); i++){
		
		//we want to color worms with close neighbors differently, so they stand out.
		//we color worms in groups differently from the original.
		unsigned int output_worm_disambiguation_colors(0);
		if (sorted_events[i].e->neighbor_group_size > 1)
			output_worm_disambiguation_colors = 1;
		if (sorted_events[i].e->neighbor_group_id_of_which_this_element_is_an_in_situ_duplicate == sorted_events[i].e->neighbor_group_id)
			output_worm_disambiguation_colors = 3;  //this is the earliest event in the neighbor group
		else if (sorted_events[i].e->neighbor_group_id_of_which_this_element_is_an_in_situ_duplicate > 0)
			output_worm_disambiguation_colors = 2;  

		if (i == 0 || sorted_events[i-1].results_id  != sorted_events[i].results_id){
			try{
			results.clear();
			results.detection_results_id = sorted_events[i].results_id;
			if (results.detection_results_id == 0)
				throw ns_ex("Found an unspecified results id for region ID ") << sorted_events[i].e->event_annotation.region_info_id << " (Image #" << sorted_events[i].region_id << ")";
			results.load_from_db(false,false,sql,false);
			ns_image_server_captured_image_region reg;
			reg.load_from_db(sorted_events[i].region_id,&sql);
			if (!use_color || output_worm_disambiguation_colors == 0)
				results.load_images_from_db(reg,sql,false, true);
			else 
				results.load_images_from_db(reg,sql,false,false);
			}
			catch(ns_ex & ex_){
				ns_ex ex;
				ex << "Encountered problem processing an event annotation in region " << sorted_events[i].e->event_annotation.region_info_id << ":";
				ns_region_metadata reg;
				try{
					reg.load_from_db(sorted_events[i].e->event_annotation.region_info_id,"",sql);
					ex <<  "(" << reg.experiment_name << "::" << reg.plate_name() << ")";
				}
				catch(...){}
				ex << ": " << ex_.text();
				throw ex;
			}
		}
		if (i == 0 || sorted_events[i-1].interpolated_results_id  != sorted_events[i].interpolated_results_id){
		
			interpolated_results.clear();
			interpolated_results.detection_results_id = sorted_events[i].interpolated_results_id;
			if (interpolated_results.detection_results_id != 0){
				try{
				interpolated_results.load_from_db(false,true,sql,false);
				ns_image_server_captured_image_region reg;
				reg.load_from_db(sorted_events[i].region_id,&sql);
				if (!use_color || !output_worm_disambiguation_colors)
					interpolated_results.load_images_from_db(reg,sql,true, true);
				else 
					interpolated_results.load_images_from_db(reg,sql,true,false);
				}
				catch(ns_ex & ex_){
					ns_ex ex;
					ex << "Region ID " << sorted_events[i].e->event_annotation.region_info_id;
					ns_region_metadata reg;
					try{
						reg.load_from_db(sorted_events[i].e->event_annotation.region_info_id,"",sql);
						ex <<  "(" << reg.experiment_name << "::" << reg.plate_name() << ")";
					}
					catch(...){}
					ex << ": " << ex_.text();
					throw ex;
				}
			}
		}
		
		
		const std::vector<const ns_detected_worm_info *> &actual_worms(results.actual_worm_list());
		const std::vector<const ns_detected_worm_info *> &interpolated_worms(interpolated_results.actual_worm_list());
		std::vector<const ns_detected_worm_info *> worms;
		worms.insert(worms.end(),actual_worms.begin(),actual_worms.end());
		worms.insert(worms.end(),interpolated_worms.begin(),interpolated_worms.end());
		float im_resolution(72);
		if (worms.size() != 0)
			im_resolution = worms[0]->absolute_grayscale().properties().resolution;

		//sorted_events[i].e->image.prepare_to_recieve_image(ns_image_properties(sorted_events[i].e->size().y,sorted_events[i].e->size().x,1,im_resolution));
		
		//bool found(false);
		const ns_detected_worm_info * current_worm(0);
		const ns_detected_worm_info * current_worm_in_incorrect_image(0);
		for (unsigned int j = 0; j < worms.size(); j++){
			if (worms[j]->region_position_in_source_image == sorted_events[i].e->annotation_whose_image_should_be_used.position){
				if (worms[j]->interpolated != sorted_events[i].e->annotation_whose_image_should_be_used.inferred_animal_location){
					current_worm_in_incorrect_image = worms[j];
					image_server.add_subtext_to_current_event("Found worm in incorrect image.\n", &sql);
					if (sorted_events[i].e->image.properties().width > sorted_events[i].e->image_image_size().x ||
						sorted_events[i].e->image.properties().height > sorted_events[i].e->image_image_size().y)
						image_server.add_subtext_to_current_event(std::string("Incorrect image had an incorrect size: ") + ns_to_string(sorted_events[i].e->image.properties().width) + "," 
																						+ ns_to_string(sorted_events[i].e->image.properties().height)
																						 +" vs an expected " + 
							ns_to_string(sorted_events[i].e->image_image_size().x)+
																						 "," +
							ns_to_string(sorted_events[i].e->image_image_size().y),&sql);
				}
				else{
					current_worm = worms[j];	
				//	if (sorted_events[i].e->annotation_whose_image_should_be_used.inferred_animal_location)
				//		cerr << "Found interpolated worm image.\n";
		//			if (sorted_events[i].e->event_annotation.position == ns_vector_2i(2579,300))
	//					cerr << "WHA";
					//images aren't loaded yet--this check does nothing
					//if (sorted_events[i].e->image.properties().width > sorted_events[i].e->image_image_size().x ||
					//	sorted_events[i].e->image.properties().height > sorted_events[i].e->image_image_size().y)
					//	throw ns_ex("An unusual context image size was encountered: ") << sorted_events[i].e->image.properties().width << "," 
					//																	<< sorted_events[i].e->image.properties().height
					//																	 << " vs an expected " <<  
					//																	 sorted_events[i].e->image_image_size().x << 
					//																	 "," <<
					//																  sorted_events[i].e->image_image_size().y;
					break;
				}
			}
		}
		if (current_worm == 0){
			ns_ex ex("Could not find the image corresponding to the ");
			ex << (sorted_events[i].e->annotation_whose_image_should_be_used.inferred_animal_location?"interpolated":"non-interpolated") << " worm #" <<
				sorted_events[i].e->annotation_whose_image_should_be_used.stationary_path_id.group_id 
				<< "There were " << actual_worms.size() << " actual and " << interpolated_worms.size() << " interpolated worms identified at this timepoint. ";
			sql << "select s.name, r.name,r.sample_id from sample_region_image_info as r, capture_samples as s "
					"WHERE r.id = " << sorted_events[i].e->annotation_whose_image_should_be_used.region_info_id << " AND s.id = r.sample_id";
			ns_sql_result res;
			sql.get_rows(res);
			
			if (res.size() == 0)
				ex << "Additionally, metadata for the region info id " << sorted_events[i].e->annotation_whose_image_should_be_used.region_info_id << " could not be found.";
			else 
				ex << "The worm was located on: " << res[0][0] << "::" << res[0][1] << "::";
			ex << sorted_events[i].e->annotation_whose_image_should_be_used.description();

			throw ex;
		}

		if (!use_color)
			current_worm->context_image().absolute_grayscale.pump(sorted_events[i].e->image,1024);
		else if (use_color){
			ns_image_properties prop(current_worm->context_image().absolute_grayscale.properties());
			prop.components = 3;
			sorted_events[i].e->image.prepare_to_recieve_image(prop);
			if (output_worm_disambiguation_colors == 0){
				for (unsigned int y = 0; y < prop.height; y++){
					for (unsigned int x = 0; x < prop.width; x++){
						sorted_events[i].e->image[y][3*x] = current_worm->context_image().absolute_grayscale[y][x];
						sorted_events[i].e->image[y][3*x+1] = current_worm->context_image().absolute_grayscale[y][x];
						sorted_events[i].e->image[y][3*x+2] = current_worm->context_image().absolute_grayscale[y][x];
					}
				}
				
			}
			else{
				//const float background(.75);
				const ns_vector_2i off(current_worm->get_region_offset_in_context_image());
				for (unsigned int y = 0; y < prop.height; y++){
					for (unsigned int x = 0; x < prop.width; x++){

					/*	if (y >= sorted_events[i].e->image.properties().height ||
							x >= 3*sorted_events[i].e->image.properties().width)
							throw ns_ex("Yikes!");*/
						bool background(true);
						//float f(background);
						if (y >= off.y && x >= off.x && 
							y < off.y+current_worm->bitmap().properties().height &&
							x < off.x+current_worm->bitmap().properties().width)
							background = !current_worm->bitmap()[y-off.y][x-off.x];

						float d(current_worm->context_image().absolute_grayscale[y][x]*1.10);
						if (d>255)
								d = 255;
						switch(output_worm_disambiguation_colors){
							case 1: 
								//color the neighbors we've moved into place blue
								if (background){
									sorted_events[i].e->image[y][3*x] = current_worm->context_image().absolute_grayscale[y][x]*.85;
									sorted_events[i].e->image[y][3*x+1] = current_worm->context_image().absolute_grayscale[y][x]*.85;
									sorted_events[i].e->image[y][3*x+2] = current_worm->context_image().absolute_grayscale[y][x];
								}
								else{
									sorted_events[i].e->image[y][3*x] = d;
									sorted_events[i].e->image[y][3*x+1] = d;
									sorted_events[i].e->image[y][3*x+2] = current_worm->context_image().absolute_grayscale[y][x];
								}
								break;
							case 2:
								//color the "originals" red
								if (background){
									sorted_events[i].e->image[y][3*x] = current_worm->context_image().absolute_grayscale[y][x];
									sorted_events[i].e->image[y][3*x+1] = current_worm->context_image().absolute_grayscale[y][x]*.85;
									sorted_events[i].e->image[y][3*x+2] = current_worm->context_image().absolute_grayscale[y][x]*.85;
								}
								else{
									sorted_events[i].e->image[y][3*x] = current_worm->context_image().absolute_grayscale[y][x];
									sorted_events[i].e->image[y][3*x+1] = d;
									sorted_events[i].e->image[y][3*x+2] = d;
								}
								break;
							case 3:
									//color the earliest "original" green
									if (background){
										sorted_events[i].e->image[y][3*x] = current_worm->context_image().absolute_grayscale[y][x]*.85;
										sorted_events[i].e->image[y][3*x+1] = current_worm->context_image().absolute_grayscale[y][x];
										sorted_events[i].e->image[y][3*x+2] = current_worm->context_image().absolute_grayscale[y][x]*.85;
									}
									else{
										sorted_events[i].e->image[y][3*x] = d;
										sorted_events[i].e->image[y][3*x+1] = current_worm->context_image().absolute_grayscale[y][x];
										sorted_events[i].e->image[y][3*x+2] = d;
									}
									break;
						}
					}
				}
			}

		}
	}
}

void ns_experiment_storyboard_timepoint::clear_images(){
	for (unsigned int i = 0; i < events.size(); i++){
		events[i].image.clear();
	}
}

class ns_annotation_orderer{
public:
	bool operator()(const ns_experiment_storyboard_timepoint_element * a, const ns_experiment_storyboard_timepoint_element * b){
		//first put earlier time points first
		if (a->storyboard_time == b->storyboard_time){
			//then put points closer to (0,0) first
			int ad(a->event_annotation.position.squared()),
				bd(b->event_annotation.position.squared());
			if (ad == bd)
				//then put smaller objects first
				return a->event_annotation.size.squared() < b->event_annotation.size.squared();
			return ad < bd;

		}
		return a->storyboard_time < b->storyboard_time;
	}
};


struct ns_timepoint_element_neighbor_list{
	ns_timepoint_element_neighbor_list():earliest_death(INT_MAX),center_sum(0,0),region_id(0),
		group_id_of_which_this_element_is_an_in_situ_duplicate(0){}
	std::vector<ns_experiment_storyboard_timepoint_element *> elements;
	ns_64_bit region_id;
	unsigned long group_id;
	unsigned group_id_of_which_this_element_is_an_in_situ_duplicate;
	ns_vector_2i center_sum;
	unsigned long earliest_death;
};


class ns_timepoint_element_neighbor_list_orderer{
public:
	bool operator()(const ns_timepoint_element_neighbor_list * a, const ns_timepoint_element_neighbor_list * b){
		//first put earlier time points first
		if (a->earliest_death == b->earliest_death){
			//keep neighbor groups (and elements that are members of the same group) together
			unsigned long a_group((a->group_id_of_which_this_element_is_an_in_situ_duplicate>0)?a->group_id_of_which_this_element_is_an_in_situ_duplicate:a->group_id),
						  b_group((b->group_id_of_which_this_element_is_an_in_situ_duplicate>0)?b->group_id_of_which_this_element_is_an_in_situ_duplicate:b->group_id);
			if (a_group == b_group){
				//then put points closer to (0,0) first
				int ad((a->center_sum/a->elements.size()).squared()),
					bd((b->center_sum/b->elements.size()).squared());
				if (ad == bd)
					//then put smaller objects first
					return (*a->elements.begin())->event_annotation.size.squared() < (*b->elements.begin())->event_annotation.size.squared();
				return ad < bd;
			}
			return a_group < b_group;

		}
		return a->earliest_death < b->earliest_death;
	}
};

typedef std::pair<unsigned long,unsigned long> ns_region_neighbor_group;
void ns_experiment_storyboard_timepoint::calculate_worm_positions(){
	size = ns_vector_2i(0,0);
	if (events.size() == 0)
		return;
	
	std::vector<ns_experiment_storyboard_timepoint_element *> events_p(events.size());
	
	//don't do any further sorting than was done originally.
	for (unsigned int i = 0; i < events.size(); i++){
		events_p[i] = &events[i];
	}

	std::vector<unsigned long> cumulative_height(events.size());
	cumulative_height[0] = events_p[0]->image_image_size().y;
	for (unsigned int i = 1; i < events_p.size(); i++){
		cumulative_height[i] = cumulative_height[i-1] +  events_p[i]->image_image_size().y;
	}
	unsigned long worms_per_column(sqrt((double)events.size())*.6); //golden ratio
	if (worms_per_column == 0)
		worms_per_column = 1;
	if (events.size()%worms_per_column == 1)
		worms_per_column++;
	unsigned long average_column_height((*cumulative_height.rbegin())/worms_per_column);
	if (average_column_height > 700)
		average_column_height = 700;
	//unsigned long cum(cumulative_height[i]);
	const unsigned long max_column_height(average_column_height);

	//calculate the size and position of each column
	std::vector<ns_vector_2i> column_positions(1,ns_vector_2i(INTER_COLUMN_MARGIN,INTER_COLUMN_MARGIN));
	std::vector<ns_vector_2i> column_sizes(1,ns_vector_2i(0,0));
	std::vector<unsigned long> last_element_index_in_column(1,0);
	unsigned long current_column_id(0);
	unsigned long worms_in_column(0);
	for (unsigned int i = 0; i < events_p.size(); i++){
		if (column_sizes	[current_column_id].y + events_p[i]->image_image_size().y > max_column_height){
			//column_sizes	[current_column_id].y = current_position.y;
			//if (size.y < column_sizes	[current_column_id].y)
				//size.y = column_sizes	[current_column_id].y;

			current_column_id++;
			column_sizes.resize(current_column_id+1,ns_vector_2i(0,INTER_COLUMN_MARGIN));
			column_positions.resize(current_column_id +1, 
				ns_vector_2i(column_positions[current_column_id-1].x + column_sizes[current_column_id-1].x,0));
			last_element_index_in_column.push_back(0);
		}
		last_element_index_in_column[current_column_id] = i;
		if (column_sizes[current_column_id].x < events_p[i]->image_image_size().x+INTER_COLUMN_MARGIN)
			column_sizes[current_column_id].x = events_p[i]->image_image_size().x+INTER_COLUMN_MARGIN;

		column_sizes[current_column_id].y += events_p[i]->image_image_size().y+INTER_COLUMN_MARGIN;
	}

	size.y = column_positions[0].y + column_sizes[0].y;
	for (unsigned int i = 1; i < column_positions.size(); i++){
		if (size.y < column_positions[i].y + column_sizes[i].y)
			size.y = column_positions[i].y + column_sizes[i].y;
	}
	size.x = column_positions.rbegin()->x + column_sizes.rbegin()->x;

	//output each column, top to bottom.
	unsigned long current_event_id(0);
	for (unsigned int i = 0; i < column_positions.size(); i++){
		ns_vector_2i current_position(column_positions[i].x,size.y-INTER_COLUMN_MARGIN);
		for (;current_event_id <= last_element_index_in_column[i]; current_event_id++){
			current_position.y-=events_p[current_event_id]->image_image_size().y+INTER_COLUMN_MARGIN;
			events_p[current_event_id]->position_on_time_point = current_position;
		}
	}
}
void ns_experiment_storyboard_timepoint_element::specify_by_hand_annotations(const ns_death_time_annotation & sticky_properties, const std::vector<ns_death_time_annotation> & movement_events){
	event_annotation.clear_sticky_properties();
	event_annotation.excluded = sticky_properties.excluded;
	event_annotation.number_of_worms_at_location_marked_by_hand = sticky_properties.number_of_worms_at_location_marked_by_hand;
	event_annotation.flag = sticky_properties.flag;
	//overwrite any existing movement events with newly specified ones.
	for (ns_by_hand_movement_annotation_list::iterator p = by_hand_movement_annotations_for_element.begin(); p != by_hand_movement_annotations_for_element.end();p++)
			p->matched = false;
	for (unsigned int i = 0; i < movement_events.size(); i++){

		if (movement_events[i].animal_id_at_position > 0 &&
			movement_events[i].animal_id_at_position >= 
			sticky_properties.number_of_worms_at_location_marked_by_hand)
			throw ns_ex("Submitting invalid animal id at position!");
		

		//affix the data to the new stationary path.
		ns_death_time_annotation a(movement_events[i]);
		a.stationary_path_id = this->event_annotation.stationary_path_id;
		a.position	= this->event_annotation.position;
		a.size = this->event_annotation.size;
		a.annotation_source = ns_death_time_annotation::ns_storyboard;

		bool event_found(false);
		
		for (ns_by_hand_movement_annotation_list::iterator p = by_hand_movement_annotations_for_element.begin(); p != by_hand_movement_annotations_for_element.end();){	
			if (p->annotation.type == movement_events[i].type &&
				p->annotation.animal_id_at_position == movement_events[i].animal_id_at_position){
					if (event_found){
						cerr << "Found duplicate annotation.\n";
						p = by_hand_movement_annotations_for_element.erase(p);
					}
					else{
						event_found = true;
						p->annotation = a;
						p->loaded_from_disk = false;
						p->matched = true;
						p++;
					}
			}
			else
				p++;
		}
		if (!event_found){
			by_hand_movement_annotations_for_element.push_back(ns_by_hand_movement_annotation(a,false));
			by_hand_movement_annotations_for_element.rbegin()->matched = true;
		}
	}
	for (ns_by_hand_movement_annotation_list::iterator p = by_hand_movement_annotations_for_element.begin(); p != by_hand_movement_annotations_for_element.end();){	
		if (p->annotation.animal_id_at_position == 0 && p->annotation.type == ns_additional_worm_entry ||
			p->annotation.animal_id_at_position > 0 &&
			p->annotation.animal_id_at_position >= sticky_properties.number_of_worms_at_location_marked_by_hand){
			cerr << "Ignoring first animal entry annotation, or invalid animal id";
			p = by_hand_movement_annotations_for_element.erase(p);
		}
		else if (!p->loaded_from_disk && !p->matched)	//remove left over old annotations
			p = by_hand_movement_annotations_for_element.erase(p);
		else p++;
	}
		
	
}
void ns_experiment_storyboard_timepoint_element::simplify_and_condense_by_hand_movement_annotations(){
	//we take this opportunity to clear up multiple redundant annotations for worms.
	//we take all the sticky properties accumulated in various by hand annotations and put them in the single annotation element
	//only movement annotations remain in the by_hand_movement_annotation_for_element array
	//and those annotations have sticky properties removed for simplicity.
	for (ns_by_hand_movement_annotation_list::iterator p = by_hand_movement_annotations_for_element.begin(); p != by_hand_movement_annotations_for_element.end();){
//		if (p->number_of_extra_worms_at_location > 0)
//			cerr < "extra worm found";
		p->annotation.transfer_sticky_properties(event_annotation);
		if (p->annotation.type == ns_no_movement_event)
			p = by_hand_movement_annotations_for_element.erase(p);
		else{

			p->annotation.clear_sticky_properties();
			//ensure that by hand annotations created on previous analyses of the data set are linked properly to this one.
			p->annotation.position = event_annotation.position;
			p->annotation.size = event_annotation.size;
			p->annotation.stationary_path_id = event_annotation.stationary_path_id;
			p++;
		}
	}
}

const ns_experiment_storyboard_timepoint_element & ns_experiment_storyboard::find_animal(const ns_64_bit region_info_id,const ns_stationary_path_id & id) const{
	for (unsigned i = 0; i < divisions.size(); i++){
		for (unsigned int j = 0; j < divisions[i].events.size(); j++){
			if (divisions[i].events[j].event_annotation.region_info_id == region_info_id && divisions[i].events[j].event_annotation.stationary_path_id == id)
				return divisions[i].events[j];
		}
	}
	throw ns_ex("ns_experiment_storyboard::find_animal()::Could not find animal");
}

std::string ns_death_time_annotation_region_details(const ns_death_time_annotation & a){
	return std::string("Region Info Id: ") + ns_to_string(a.region_info_id) + "::" + ns_to_string(a.stationary_path_id.group_id);
}

typedef enum {ns_inspect_for_non_worms,ns_inspect_for_multiworm_clumps, ns_number_of_flavors} ns_storyboard_flavor;


ns_vector_2i ns_experiment_storyboard_timepoint_element::event_object_size()const { return event_annotation.size; }
ns_vector_2i ns_experiment_storyboard_timepoint_element::event_image_size()const { return event_annotation.size + ns_worm_collage_storage::context_border_size() * 2; }
ns_vector_2i ns_experiment_storyboard_timepoint_element::image_object_size()const { return annotation_whose_image_should_be_used.size; }
ns_vector_2i ns_experiment_storyboard_timepoint_element::image_image_size()const { return annotation_whose_image_should_be_used.size + ns_worm_collage_storage::context_border_size() * 2; }



std::string ns_experiment_storyboard_spec::to_string() const {
	return ns_to_string(region_id) + ns_to_string(sample_id) + ns_to_string(experiment_id) + (use_by_hand_annotations ? "1" : "0") + ns_to_string((int)event_to_mark)
		+ strain_to_use.strain + strain_to_use.strain_condition_1 + strain_to_use.strain_condition_2 + (use_absolute_time ? "1" : "0") + ns_to_string(delay_time_after_event) + ns_to_string(minimum_distance_to_juxtipose_neighbors)
		+ (choose_images_from_time_of_last_death ? "1" : "0");
}

void ns_experiment_storyboard_spec::set_flavor(const ns_storyboard_flavor & f){
	use_by_hand_annotations = false;
	use_absolute_time = false;
	event_to_mark = ns_movement_cessation;
	switch(f){
	//allows us to check for mutliple worm clumps
		case ns_inspect_for_non_worms:
			choose_images_from_time_of_last_death = false;
			delay_time_after_event = 1*60*60*24;
			break;
	//allows us to look for non-worms at times before the worms have disintergated
		case ns_inspect_for_multiworm_clumps:
			choose_images_from_time_of_last_death = true;
			delay_time_after_event = 0;
			break;
		default: throw ns_ex("Unknown Storyboard Flavor:") << (int)f;
	}
}


ns_experiment_storyboard_compiled_event_set::ns_experiment_storyboard_compiled_event_set() {
	//spec needs to be initialized, but it it has it's own default initializer to do this, so we're fine.
}

void ns_experiment_storyboard::check_that_all_time_path_information_is_valid(ns_sql & sql) {
	build_worm_detection_id_lookup_table(sql);
	std::set<ns_64_bit> region_ids;
	for (unsigned i = 0; i < divisions.size(); i++) {
		for (unsigned int j = 0; j < divisions[i].events.size(); j++) {
			region_ids.insert(region_ids.begin(), divisions[i].events[j].event_annotation.region_info_id);
		}
	}
	std::vector<ns_ex> unfixable_errors;
	for (std::set<ns_64_bit>::iterator id = region_ids.begin(); id != region_ids.end(); id++) {
	//	cout << " Validating region " << *id << "\n";
	//	if (*id == 53771)
	//		cerr << "checking it!";
		/*std::map<ns_64_bit, std::map<ns_64_bit, ns_reg_info> >::const_iterator p = worm_detection_id_lookup.find(*id);
		if (p == worm_detection_id_lookup.end())
			throw ns_ex("Could not find region id ") << *id << " in worm detection table.";*/
		try {
			ns_image_server_image im = ns_time_path_image_movement_analyzer::get_movement_quantification_id(*id, sql);
			ns_acquire_for_scope<ifstream> i(image_server_const.image_storage.request_metadata_from_disk(im, false, &sql));
			i().close();
			i.release();
		}
		catch (ns_ex & ex) {
			sql << "SELECT r.name,s.name FROM sample_region_image_info as r, capture_samples as s WHERE r.id = " << *id << " AND r.sample_id = s.id";
			ns_sql_result res;
			sql.get_rows(res);
			std::string name = ns_to_string(*id);
			if (res.size() > 0)
				name = res[0][1] + "::" + res[0][0];

			try {
				ns_image_server_image im = image_server_const.image_storage.get_region_movement_metadata_info(*id, "time_path_movement_image_analysis_quantification", sql);
				ns_acquire_for_scope<ifstream> i(image_server_const.image_storage.request_metadata_from_disk(im, false, &sql));
				i().close();
				i.release();
				im.save_to_db(im.id, &sql);
				sql << "UPDATE sample_region_image_info SET movement_image_analysis_quantification_id = " << im.id << " WHERE id = " << *id;
				sql.send_query();
				image_server.add_subtext_to_current_event(std::string("Fixed bad db record for region ") + name + ".  This is probably OK, but you may want to regenerate the region if further problems are encountered.", &sql);
			}
			catch (ns_ex & ex2) {
				unfixable_errors.push_back(ns_ex("Could not fix the error in region ") << name <<": " << ex.text() << "(" << ex2.text() << ")");
			}
		}
	}
	if (!unfixable_errors.empty()) {
		ns_ex ex;
		for (unsigned int i = 0; i < unfixable_errors.size(); i++)
			ex << unfixable_errors[i].text() << "\n";
		throw ex;
	}
}


//unsigned long cc(0); 
bool ns_experiment_storyboard::load_events_from_annotation_compiler(const ns_loading_type & loading_type,const ns_death_time_annotation_compiler & all_events,const bool use_absolute_time, const bool state_annotations_available_in_loaded_annotations,const unsigned long minimum_distance_to_juxtipose_neighbors,ns_sql & sql){
	first_time = ns_current_time();
	last_time = 0;

	std::vector<ns_experiment_storyboard_timepoint_element> animals;
	animals.reserve(50);

	//In all_events we've thrown together in a single list two types of annotations
	//	1) all the machine annotations that specify the animals that get put into storyboard
	//	2) by hand annotations generated by clicking on the storyboard.

	//So, the task is to identify machine annotations to build the storyboard
	//and attach the by-hand annotations to their corresponding machine annotations on the storyboard.
	std::vector<ns_experiment_storyboard_timepoint_element> machine_exclued_animals;
	std::vector<const ns_death_time_annotation * > state_annotations;

	unsigned long number_of_coincident_worms(0);
	unsigned long number_of_observed_deaths(0);
	unsigned long number_of_multi_stationary_event_animals(0);
	unsigned long cc(0);
	image_server.add_subtext_to_current_event("Aggregating Region Events.\n",&sql);
	ns_machine_analysis_data_loader region_state_annotations(true);
	unsigned long last_cc(-10);


	//use to confirm validity of each annotation
	build_worm_detection_id_lookup_table(sql);
	//problem_ids[region_info_id][region image id]
	std::map<ns_64_bit, vector<std::pair<ns_64_bit,std::string> > > problem_ids, all_ids;

	ns_death_time_annotation_compiler_region temporary_region_storage;

	bool may_need_to_load_state_events(subject_specification.delay_time_after_event > 0 || subject_specification.choose_images_from_time_of_last_death);


	for (ns_death_time_annotation_compiler::ns_region_list::const_iterator region_pointer = all_events.regions.begin(); region_pointer != all_events.regions.end(); region_pointer++) {

		const ns_death_time_annotation_compiler_region * r;
		if (!may_need_to_load_state_events)
			r = &(region_pointer->second);
		else {
			//make a copy using the default copy constructor
			//Thank god for STL containers, that will handle all the details!
			temporary_region_storage = region_pointer->second;
			r = &temporary_region_storage;
		}

		unsigned long cur_cc = (100 * cc) / all_events.regions.size();
		if (cur_cc - last_cc >10) {
			image_server_const.add_subtext_to_current_event(ns_to_string(cur_cc) + "%...",&sql);
			last_cc = cur_cc;
		}
		cc++;


		region_state_annotations.clear();
		bool region_state_events_loaded(false);


		for (ns_death_time_annotation_compiler_region::ns_location_list::const_iterator q = r->locations.begin(); q != r->locations.end(); q++) {

			number_of_observed_deaths++;

			//first we find the event time for each worm, that is, the time for
			//which we want to use its picture in the storyboard
			ns_dying_animal_description_const d(q->generate_dying_animal_description_const(false));

			ns_death_time_annotation event_to_place_on_storyboard;
			bool found_storyboard_event(false);


			ns_movement_event state_to_search(ns_stationary_worm_observed);
			bool animal_moving_after_last_observation(false);
			switch (subject_specification.event_to_mark) {

			case ns_movement_cessation:

				if (d.machine.death_annotation != 0) {
					if (!d.machine.death_annotation->is_censored()) {
						event_to_place_on_storyboard = *d.machine.death_annotation;
						state_to_search = ns_stationary_worm_observed;
						found_storyboard_event = true;
						break;
					}
					else
						animal_moving_after_last_observation = true;
					//NOTE The synatx here means that we will try to add a translation cessation event
					//if a movement cessation isn't present.

				}
			case ns_translation_cessation:
				if (d.machine.last_slow_movement_annotation != 0) {
					event_to_place_on_storyboard = *d.machine.last_slow_movement_annotation;
					state_to_search = ns_posture_changing_worm_observed;
					found_storyboard_event = true;
					break;
					//NOTE The synatx here means that we will try to add a fast moving cessation event
					//if a movement cessation isn't present.
				}
			case ns_fast_movement_cessation:
				if (d.machine.last_fast_movement_annotation != 0) {
					if (!d.machine.slow_moving_state_annotations.empty()) {
						event_to_place_on_storyboard = *d.machine.last_fast_movement_annotation;
						state_to_search = ns_slow_moving_worm_observed;
						found_storyboard_event = true;
					}
				}
				break;
			case ns_stationary_worm_disappearance:
				if (d.machine.stationary_worm_dissapearance != 0) {
					state_to_search = ns_stationary_worm_observed;
					event_to_place_on_storyboard = *d.machine.stationary_worm_dissapearance;
					found_storyboard_event = true;
				}
				break;
			case ns_worm_death_posture_relaxation_termination:
				if (d.machine.death_posture_relaxation_termination != 0) {
					event_to_place_on_storyboard = *d.machine.death_posture_relaxation_termination;
					state_to_search = ns_stationary_worm_observed;
					found_storyboard_event = true;
				}
				break;
			default: throw ns_ex("Unknown storyboard event type:") << (int)subject_specification.event_to_mark;
			}

			if (!found_storyboard_event || event_to_place_on_storyboard.annotation_source != ns_death_time_annotation::ns_lifespan_machine)
				continue;

			if (event_to_place_on_storyboard.time.fully_unbounded()) {
				image_server.add_subtext_to_current_event(ns_image_server_event("Found fully unbounded event time interval in ") << r->metadata.plate_name() << " (" << r->metadata.region_id << ")",&sql);
				continue;
			}
			//if (event_to_place_on_storyboard.time.period_end_was_not_observed)
			//	continue;

			//now we have the event to use as the storyboard event.
			ns_death_time_annotation event_whose_image_should_be_used(event_to_place_on_storyboard);



			//We need to find the image to use for the storyboard.
			//this is a movement state event.
			//when we're building the storyboard for the first time,
			//there are lots of possible state events and we choose the one closest to our desired time.
			//when we're loading the storyboard from metadata, there's only one and we choose it.
			if (event_to_place_on_storyboard.type != ns_movement_cessation && may_need_to_load_state_events)
				event_whose_image_should_be_used = event_to_place_on_storyboard;
			else {

				//some storyboard types require knowlege of events that occur after the event transition
				//(ie images of the worms a certain duration after it has died)
				//In this case we need to load the state annotations.
				//Because these take up a significant amount of memory, we only store the current region in memory,
				//and only load it from disk when necissary

				//these might already be loaded in by a previous operation, so we only go to disk if necissary
				if (!state_annotations_available_in_loaded_annotations) {
					if (!region_state_events_loaded) {
						region_state_annotations.load(ns_death_time_annotation_set::ns_movement_states, r->metadata.region_id, 0, 0, sql, true, ns_machine_analysis_region_data::ns_exclude_fast_moving_animals);

						for (unsigned long i = 0; i < region_state_annotations.samples.begin()->regions.begin()->death_time_annotation_set.size(); i++) {
							//we add this to the temporary storage being used 
							temporary_region_storage.add(region_state_annotations.samples.begin()->regions.begin()->death_time_annotation_set[i], false);
						}
						region_state_events_loaded = true;
					}
				}

				bool found_match(false);

				//find last event
				if (event_to_place_on_storyboard.type == ns_translation_cessation ||
					event_to_place_on_storyboard.type == ns_fast_movement_cessation) {
					unsigned long latest_time(0);
					for (vector<ns_death_time_annotation>::const_iterator p = q->annotations.begin(); p != q->annotations.end(); p++) {
						if (p->type != state_to_search)
							continue;
						if (p->time.best_estimate_event_time_for_possible_partially_unbounded_interval() > latest_time) {
							latest_time = p->time.best_estimate_event_time_for_possible_partially_unbounded_interval();
							event_whose_image_should_be_used = (*p);
							found_match = true;
						}
					}
				}
				else {
					//find event closest in time to the desired
					unsigned long ideal_time_for_image;
					if (subject_specification.choose_images_from_time_of_last_death) {
						if (time_of_last_death == 0)
							throw ns_ex("ns_experiment_storyboard::load_events_from_annotation_compiler()::time of death was not calculated");
						ideal_time_for_image = time_of_last_death;
					}
					else if (subject_specification.delay_time_after_event > 0) {
						if (event_to_place_on_storyboard.time.period_end_was_not_observed)
							ideal_time_for_image = event_to_place_on_storyboard.time.period_start + subject_specification.delay_time_after_event;
						else ideal_time_for_image = event_to_place_on_storyboard.time.period_end + subject_specification.delay_time_after_event;
					}
					else throw ns_ex("Code flow error");

					unsigned long lowest_diff(UINT_MAX);
					//if we're creating a storyboard for the first time,
					// we look for the state observation nearest to the desired time
					for (vector<ns_death_time_annotation>::const_iterator p = q->annotations.begin(); p != q->annotations.end(); p++) {
						if (p->time.fully_unbounded()) {
							cerr << "Found fully unbounded event: " << p->brief_description() << "\n";
							continue;
						}
						if (p->type != state_to_search) {
							continue;
						}
						unsigned long diff;
						unsigned long event_time;
						if (p->time.period_end_was_not_observed)
							event_time = p->time.period_start;
						else event_time = p->time.period_end;
						if (event_time >= ideal_time_for_image)
							diff = event_time - ideal_time_for_image;
						else diff = ideal_time_for_image - event_time;

						if (diff < lowest_diff) {
							found_match = true;
							lowest_diff = diff;
							event_whose_image_should_be_used = (*p);
						}
					}

				}
				if (!found_match)
					throw ns_ex("Could not find any state events for animal in ") << r->metadata.plate_name() << " (" << r->metadata.region_id << ")";

			}

			ns_experiment_storyboard_timepoint_element * annotation_subject(0);
			if (event_to_place_on_storyboard.excluded == ns_death_time_annotation::ns_machine_excluded) {
				machine_exclued_animals.resize(machine_exclued_animals.size() + 1);
				annotation_subject = &(*machine_exclued_animals.rbegin());
			}
			else {
				animals.resize(animals.size() + 1);
				annotation_subject = &(*animals.rbegin());
				//animals.rbegin()->annotation = *e;
			}
			annotation_subject->event_annotation = event_to_place_on_storyboard;
			annotation_subject->annotation_whose_image_should_be_used = event_whose_image_should_be_used;

			for (vector<ns_death_time_annotation>::const_iterator p = q->annotations.begin(); p != q->annotations.end(); p++) {
				if (!subject_specification.use_by_hand_annotations &&
					(p->annotation_source == ns_death_time_annotation::ns_posture_image ||
						p->annotation_source == ns_death_time_annotation::ns_region_image ||
						p->annotation_source == ns_death_time_annotation::ns_storyboard
						)) {
					//we keep by hand movement annotations saved to disk and never delete them, even if they aren't included in desired output.
					annotation_subject->by_hand_movement_annotations_for_element.push_back(ns_by_hand_movement_annotation(*p, true));
				}
			}
			ns_death_time_annotation * annotation_to_use_for_time(&event_to_place_on_storyboard);
			//we want to put animals who never stop moving all at the end of the storyboard.
			if (event_to_place_on_storyboard.type == ns_translation_cessation ||
				event_to_place_on_storyboard.type == ns_fast_movement_cessation)
				annotation_to_use_for_time = &event_whose_image_should_be_used;

			annotation_subject->event_annotation.clear_sticky_properties();
			if (annotation_to_use_for_time->time.period_end_was_not_observed) {
				annotation_subject->storyboard_absolute_time = annotation_to_use_for_time->time.period_start;
				annotation_subject->storyboard_time = annotation_to_use_for_time->time.period_start - (use_absolute_time ? 0 : (r->metadata.time_at_which_animals_had_zero_age));
			}
			else {
				annotation_subject->storyboard_absolute_time = annotation_to_use_for_time->time.period_end;
				annotation_subject->storyboard_time = annotation_to_use_for_time->time.period_end - (use_absolute_time ? 0 : (r->metadata.time_at_which_animals_had_zero_age));
			}

			annotation_subject->simplify_and_condense_by_hand_movement_annotations();

			//check subject matches to an existing worm detection result, which we'll need to generate images etc.
			std::map<ns_64_bit, map<ns_64_bit, ns_reg_info> >::iterator p(worm_detection_id_lookup.find(annotation_subject->annotation_whose_image_should_be_used.region_info_id)); 
		
			std::string problem_desc = "**E**: " + annotation_subject->annotation_whose_image_should_be_used.brief_description() + "\n" +
				"**C**: " + annotation_subject->event_annotation.brief_description();
			if (p == worm_detection_id_lookup.end()) {
				problem_ids[annotation_subject->event_annotation.region_info_id].push_back(std::pair<ns_64_bit, std::string>(0, problem_desc + " is not in a valid region."));
				continue;
			}
			map<ns_64_bit, ns_reg_info>::iterator q2(p->second.find(annotation_subject->annotation_whose_image_should_be_used.time.period_end));
			if (q2 == p->second.end()) {
				problem_ids[annotation_subject->event_annotation.region_info_id].push_back(std::pair<ns_64_bit, std::string>(0, problem_desc + " cannot be matched to an existing image"));
				continue;
			}
			if (q2->second.worm_detection_results_id == 0) {
				problem_ids[annotation_subject->event_annotation.region_info_id].push_back(std::pair<ns_64_bit, std::string>(q2->second.worm_detection_results_id,  problem_desc + " is matched to an image lacking a worm detection results id"));
			}
			all_ids[annotation_subject->event_annotation.region_info_id].push_back(std::pair<ns_64_bit, std::string>(q2->second.worm_detection_results_id,""));

		}
		//clear annotations to save memory
		if (may_need_to_load_state_events) {
			temporary_region_storage.clear();
		}
	}
	if (problem_ids.size() > 0) {
		ns_ex ex("ns_experiment_storyboard::draw()::Found ");
		ex << problem_ids.size() << " out of " << (all_ids.size()) << " regions contained images in which worms had been annotated but worm detection data had been deleted.\n";
		for (std::map<ns_64_bit, vector<std::pair<ns_64_bit, std::string> > >::iterator q = problem_ids.begin(); q != problem_ids.end(); q++) {
			ex << "Region " << q->first << " contains " << q->second.size() << " problem images\n";
		}
		for (std::map<ns_64_bit, vector<std::pair<ns_64_bit, std::string> > >::iterator q = problem_ids.begin(); q != problem_ids.end(); q++) {
			ex << "****Region " << q->first << " ( " << q->second.size() << ")\n";
			for (unsigned int i = 0; i < q->second.size(); i++) {
				ex << (q->second)[i].second << "\n";
				if ((q->second)[i].first != 0) {
					sql << "UPDATE sample_region_images SET problem=1 WHERE id = " << (q->second)[i].first;
					sql.send_query();
				}
			}
		}
		throw ex;
	}

	cerr << "\n";
	if (number_of_multi_stationary_event_animals > 0)
				throw ns_ex() << number_of_multi_stationary_event_animals << " were loaded from disk with multiple state observations\n";
	/*
	if (number_of_coincident_worms > 0)
			cerr << "Found a total of " << number_of_coincident_worms << " annotation events that had another event at the same location.  This probably results from coincident worms but might indicate an analysis error\n";			
	if (machine_exclued_animals.size() > 0){
		cerr << "Found " << machine_exclued_animals.size() << " animals that were excluded by the machine (likely for being low-temporal-resolution noise).  These were not included in the storyboard\n";
	}*/

	if (number_of_observed_deaths == 0){
		std::cerr << "No dead animals, or potentially dead animals, could be identified in the specificed group of plates.\n";
		return false;
	}
	if (animals.empty())
		throw ns_ex("ns_worm_storyboard::from_annotation_compiler()::No events provided!");
	const bool sort_by_time_only(false);

	//now we sort all the storyboard elements by time to lay them out on the storyboard.
	std::vector<ns_experiment_storyboard_timepoint_element *> animals_p(animals.size());
	for (unsigned int i = 0; i < animals.size(); i++)
		animals_p[i] = &animals[i];
	
	std::sort(animals_p.begin(),animals_p.end(),ns_annotation_orderer());
	bool include_close_objects_twice_once_by_neighbor_and_once_at_correct_time(true);
	if (sort_by_time_only){
		for (unsigned int i = 0; i < animals.size(); i++){
			animals[i].neighbor_group_id = i+1;
	  		animals[i].neighbor_group_size = 1;
		}
	}
	else{
		//Sometimes the machine makes errors that result in the same worm being marked as having died twice.
		//These can be picked up by looking for animals that die very close in location to one another
		//We juxtopose these on the storyboard.

		unsigned long cutoff_dist_sq(minimum_distance_to_juxtipose_neighbors*minimum_distance_to_juxtipose_neighbors);
		std::vector<ns_timepoint_element_neighbor_list> close_neighbors;
		close_neighbors.reserve(animals_p.size());
		//for each animal in the storyboard
		for (unsigned int i = 0; i < animals_p.size(); i++){
			double closest_d(DBL_MAX);
			unsigned long  closest_i(0);
			unsigned long found_group_id(0);
			//check inside existing groups to see if there's a match.
			for (unsigned int j = 0; j < close_neighbors.size(); j++){
				if (close_neighbors[j].region_id != animals_p[i]->event_annotation.region_info_id)
					continue;
				const unsigned long d ((close_neighbors[j].center_sum/(double)close_neighbors[j].elements.size() - 
					animals_p[i]->event_annotation.position).squared());
				if (d < closest_d){
					closest_d = d;
					closest_i = j;
					found_group_id = close_neighbors[j].group_id;
				}
			}

			const bool found_match(closest_d < cutoff_dist_sq);
			if (found_match){
				//update the "center" of the group of worms.  Later, we can divide out the number of animals to get the average
				//location.
				close_neighbors[closest_i].center_sum+=animals_p[i]->event_annotation.position;
				close_neighbors[closest_i].elements.push_back(animals_p[i]);
				if (animals_p[i]->storyboard_time < close_neighbors[closest_i].earliest_death)
					close_neighbors[closest_i].earliest_death = animals_p[i]->storyboard_time;
			}
			else{
				close_neighbors.resize(close_neighbors.size()+1);
				close_neighbors.rbegin()->center_sum = animals_p[i]->event_annotation.position;
				close_neighbors.rbegin()->elements.push_back(animals_p[i]);
				close_neighbors.rbegin()->earliest_death = animals_p[i]->storyboard_time;
				close_neighbors.rbegin()->region_id = animals_p[i]->event_annotation.region_info_id;
//	(*close_neighbors.rbegin()->elements.rbegin())->neighbor_group_id = group_id;
				close_neighbors.rbegin()->group_id = close_neighbors.size();
			}
		}
		const unsigned long close_neighbor_size(close_neighbors.size());

		//we've grouped all neighbors, but we also want the neighbors to appear at their propper location in the storyboard
		//so we add them here.  Note that the neighbors will all be juxtiposed at the time of the earliest event in the group, so we don't 
		//need to add an extra location for the earliest event.
		for (unsigned int i = 0; i < close_neighbor_size; i++){
			const unsigned group_id(close_neighbors[i].group_id);
			for (unsigned int j = 0; j < close_neighbors[i].elements.size(); j++){
					close_neighbors[i].elements[j]->neighbor_group_id = group_id;
					close_neighbors[i].elements[j]->neighbor_group_size = close_neighbors[i].elements.size();
			}
			if (include_close_objects_twice_once_by_neighbor_and_once_at_correct_time && close_neighbors[i].elements.size() > 1){
				for (unsigned int j = 0; j < close_neighbors[i].elements.size(); j++){
					if (close_neighbors[i].elements[j]->storyboard_time == close_neighbors[i].earliest_death){
						close_neighbors[i].elements[j]->neighbor_group_id_of_which_this_element_is_an_in_situ_duplicate = group_id;
					}
					else{
						close_neighbors.resize(close_neighbors.size()+1);	
						close_neighbors.rbegin()->center_sum = close_neighbors[i].elements[j]->event_annotation.position;
						close_neighbors.rbegin()->elements.push_back(close_neighbors[i].elements[j]);
						close_neighbors.rbegin()->earliest_death = close_neighbors[i].elements[j]->storyboard_time;
						close_neighbors.rbegin()->region_id = close_neighbors[i].elements[j]->event_annotation.region_info_id;
						close_neighbors.rbegin()->group_id = close_neighbors.size();
						close_neighbors.rbegin()->elements[0]->neighbor_group_id = close_neighbors.rbegin()->group_id;
						close_neighbors.rbegin()->elements[0]->neighbor_group_size = 1;
						close_neighbors.rbegin()->elements[0]->neighbor_group_id_of_which_this_element_is_an_in_situ_duplicate = group_id;
					}
				}
			}
		}

		cerr << "Sorted " << animals_p.size() << " into " << close_neighbors.size() << " neighbor groups\n";
		std::vector<ns_timepoint_element_neighbor_list *> close_neighbors_p(close_neighbors.size());
		for (unsigned int i = 0; i < close_neighbors.size(); i++)
			close_neighbors_p[i] = &close_neighbors[i];
		//sort neighbor groups by time
		std::sort(close_neighbors_p.begin(),close_neighbors_p.end(),ns_timepoint_element_neighbor_list_orderer());
		//restock the animal list in the correct order, giving each a unique neighbor group id
		animals_p.resize(0);
		for (unsigned int i = 0; i < close_neighbors_p.size(); i++){
			std::sort(close_neighbors_p[i]->elements.begin(),close_neighbors_p[i]->elements.end(),ns_annotation_orderer());
			for (unsigned int j = 0; j < close_neighbors_p[i]->elements.size(); j++){
				//each group is added with its members ordered in concecutive order
				animals_p.push_back(close_neighbors_p[i]->elements[j]);
			}
		}
	}
	
	first_time = (*animals_p.begin())->storyboard_time;
	unsigned long first_time_in_first_division((*animals_p.begin())->storyboard_time);
	last_time = (*animals_p.rbegin())->storyboard_time;
	for (unsigned int i = 0; i < animals_p.size(); i++){
		if (animals_p[i]->storyboard_time > last_time)
			last_time = animals_p[i]->storyboard_time;
		if (animals_p[i]->storyboard_time < first_time)
			first_time = animals_p[i]->storyboard_time;
	}
	
	divisions.resize(0);
	divisions.resize(1);
	divisions[0].time = first_time_in_first_division;

	//put a maximum of 25 animals per image in the storyboard.
	unsigned int cur_d(0);
	unsigned long number_of_events_per_division = 25;
	if (number_of_events_per_division > animals_p.size())
		number_of_events_per_division = animals_p.size();
	if (animals_p.size() < 50)
		number_of_events_per_division = animals_p.size()/2+1;

	for (unsigned int i = 0; i < animals_p.size();){

		//try not to have neighbors span pages of the storyboard
		const unsigned long current_neighbor_group_id = animals_p[i]->neighbor_group_id;
		const unsigned long number_in_neighbor_group(animals_p[i]->neighbor_group_size);
	
		//try to keep all members of a neighbor group in the same division
		//if we're going to wrap, just skip to the next division
		if (divisions.rbegin()->events.size() + number_in_neighbor_group > number_of_events_per_division){
			divisions.resize(divisions.size()+1);
			divisions.rbegin()->time = animals_p[i]->storyboard_time;
		}
		if (animals_p[i]->event_annotation.annotation_source == ns_death_time_annotation::ns_unknown)
			throw ns_ex("Found an annotation with unknown source");
		while(i < animals_p.size() && animals_p[i]->neighbor_group_id == current_neighbor_group_id){
			//sometimes groups may be larger than can be fit into an entire storyboard division!  At that point we need to split it accross
			//multiple divisions
			if (divisions.rbegin()->events.size() == number_of_events_per_division){
				divisions.resize(divisions.size()+1);
				divisions.rbegin()->time = animals_p[i]->storyboard_time;
			}
			divisions.rbegin()->events.push_back(*animals_p[i]);
			i++;
		}
	}

	cerr << animals_p.size() << " animals found in the experiment\n";
	
	calculate_worm_positions();
	subject_specification.use_absolute_time = use_absolute_time;
	return true;
}
void ns_experiment_storyboard::build_worm_detection_id_lookup_table(ns_sql & sql) {
	if (!worm_detection_id_lookup.empty())  //don't load multiple times unnecissarily
		return;
	sql << "SELECT r.worm_detection_results_id, r.capture_time, r.region_info_id, r.id FROM sample_region_images as r, sample_region_image_info as i ";
	if (subject().region_id != 0)
		sql << "WHERE r.region_info_id=" << subject().region_id << " AND i.id = " << subject().region_id;
	else if (subject().sample_id != 0)
		sql << "WHERE r.region_info_id = i.id AND i.sample_id = " << subject().sample_id;
	else if (subject().experiment_id != 0)
		sql << ", capture_samples as s "
		"WHERE r.region_info_id = i.id AND i.sample_id = s.id AND s.experiment_id = " << subject().experiment_id;
	else throw ns_ex("ns_experiment_storyboard::draw()::No subject specified!");
	sql << " AND r.capture_time <= i.last_timepoint_in_latest_movement_rebuild";
	ns_sql_result res;
	sql.get_rows(res);
	for (unsigned int i = 0; i < res.size(); i++) {
		worm_detection_id_lookup[ns_atoi64(res[i][2].c_str())][ns_atoi64(res[i][1].c_str())] = ns_reg_info(ns_atoi64(res[i][0].c_str()), ns_atoi64(res[i][3].c_str()));
	}
}
void ns_experiment_storyboard::prepare_to_draw(ns_sql & sql){
	
	//make sure ahead of time that none of the data used in worm movement analysis
	//has been deleted.
	{
		build_worm_detection_id_lookup_table(sql);

		std::map<ns_64_bit,vector<ns_64_bit> > problem_ids,all_ids;

		for (unsigned int i = 0; i < divisions.size(); i++){
		
			for (unsigned int j = 0; j < divisions[i].events.size(); j++){
				std::map<ns_64_bit, map<ns_64_bit,ns_reg_info> >::iterator p(worm_detection_id_lookup.find(divisions[i].events[j].event_annotation.region_info_id));
				if (p == worm_detection_id_lookup.end()) {
					problem_ids[divisions[i].events[j].event_annotation.region_info_id].push_back(0);
					continue;
				}
				map<ns_64_bit,ns_reg_info>::iterator q(p->second.find(divisions[i].events[j].annotation_whose_image_should_be_used.time.period_end));
				if (q == p->second.end()) {
					problem_ids[divisions[i].events[j].event_annotation.region_info_id].push_back(0);
					continue;
				}
				if (q->second.worm_detection_results_id == 0)
					problem_ids[divisions[i].events[j].event_annotation.region_info_id].push_back(q->second.region_image_id);
			
				all_ids[divisions[i].events[j].event_annotation.region_info_id].push_back(q->second.region_image_id);
		
			}
		}
		if (problem_ids.size() > 0){
			ns_ex ex("ns_experiment_storyboard::draw()::Found ");
			ex << problem_ids.size() << " out of " << (all_ids.size()) << " regions contained images in which worms had been annotated but worm detection data had been deleted.\n";
			for (std::map<ns_64_bit,vector<ns_64_bit> >::iterator q = problem_ids.begin(); q != problem_ids.end(); q++){
				ex << "Region " << q->first << " contains " << q->second.size() << " problem images\n";
			}

			for (std::map<ns_64_bit,vector<ns_64_bit> >::iterator p = problem_ids.begin(); p != problem_ids.end(); p++){
				for (unsigned int i = 0; i < p->second.size(); i++) {
					if ((p->second)[i] != 0){
						sql << "UPDATE sample_region_images SET problem=1 WHERE id = " << (p->second)[i];
						sql.send_query();
					}
				}
			}
		


			throw ex;
		}

	}
}
void ns_experiment_storyboard::draw(const unsigned long sub_image_id,ns_image_standard & im,bool use_color,ns_sql & sql) const{
	if (worm_detection_id_lookup.empty())
		throw ns_ex("ns_experiment_storyboard::draw()::Prepare to draw must be called!");
	im.prepare_to_recieve_image(ns_image_properties(worm_images_size[sub_image_id].y+label_margin_height(),worm_images_size[sub_image_id].x,use_color?3:1,72));
	if (use_color){
		for (unsigned int y = 0; y < im.properties().height; y++)
			for (unsigned int x = 0; x < 3*im.properties().width; x++)
				im[y][x] = 0;
	}
	else{
		for (unsigned int y = 0; y < im.properties().height; y++)
			for (unsigned int x = 0; x < im.properties().width; x++)
				im[y][x] = 0;
	}

	draw_label_margin(sub_image_id,use_color,im);


	for (unsigned int k = 0; k < divisions.size(); k++){
		if (divisions[k].sub_image_id != sub_image_id)
			continue;
		ns_experiment_storyboard_timepoint cur_division = divisions[k];
//std::cerr << "\nRendering sub-division " << (i+1) << " of " << divisions.size() << "...";
		//cerr << (100*i)/divisions.size() << "%...";
		cur_division.load_images(use_color,sql);
	
		for (unsigned int j = 0; j < cur_division.events.size(); j++){
			try{
				if (cur_division.events[j].image_image_size().x <
					cur_division.events[j].image.properties().width ||
					cur_division.events[j].image_image_size().y <
					cur_division.events[j].image.properties().height
				
				){
					throw ns_ex("There is a disagreement between the annotation (") 
						<< cur_division.events[j].image_image_size().x << ","
						<< cur_division.events[j].image_image_size().y
						<< ") and the actual size of the region image "
						<< cur_division.events[j].image.properties().width << ","
						<< cur_division.events[j].image.properties().height;
				}
				const ns_vector_2i p((cur_division.events[j].position_on_time_point + cur_division.position_on_storyboard));
				if(use_color){
					for (unsigned int y = 0; y < cur_division.events[j].image.properties().height; y++){
						for (unsigned int x = 0; x < 3* cur_division.events[j].image.properties().width; x++){
							if (y+p.y >= im.properties().height)
								throw ns_ex("Out of bounds y position encountered while generating storyboard:") << y+p.y << "/" << im.properties().height;
							if (x+3*p.x >= 3*im.properties().width)
								throw ns_ex("Out of bounds y position encountered while generating storyboard:") << x+p.x << "/" << im.properties().width;
							im[y+p.y][x+3*p.x] = cur_division.events[j].image[y][x];
						}
					}
				}
				else{
					for (unsigned int y = 0; y < cur_division.events[j].image.properties().height; y++){
						for (unsigned int x = 0; x < cur_division.events[j].image.properties().width; x++){
					//		if (y+p.y >= im.properties().height)
				//				throw ns_ex("YIKES");
				//			if (x+p.x >= im.properties().width)
				//				throw ns_ex("YIKES");
							im[y+p.y][x+p.x] = cur_division.events[j].image[y][x];
						}
					}
				}
			}
			catch (ns_ex & ex) {
				ns_experiment_storyboard_timepoint_element e(cur_division.events[j]);
				sql << "SELECT s.name,r.name FROM capture_samples as s, sample_region_image_info as r WHERE r.id = "
					<< e.event_annotation.region_info_id <<
					" AND s.id=r.sample_id";
				ns_sql_result res;
				sql.get_rows(res);
				std::string region_name;
				if (res.size() > 0) {
					region_name = res[0][0] + "::" + res[0][1];
				}

				throw ns_ex("A problem occurred involving worm #") << e.event_annotation.stationary_path_id.group_id << " in region " <<
					region_name << "(" <<
					e.event_annotation.region_info_id << ") at time (" <<
					e.annotation_whose_image_should_be_used.time.period_start << "," <<
					e.annotation_whose_image_should_be_used.time.period_end << ").  Details: "
					<< e.annotation_whose_image_should_be_used.description() << ": " << ex.text();
			}
		}
		cur_division.clear_images();
	}
}


void ns_experiment_storyboard_compiled_event_set::load(const ns_experiment_storyboard_spec & specification, ns_sql & sql) {
	//load machine annotations
	this->spec = specification;
	if (spec.event_to_mark == ns_no_movement_event)
		throw ns_ex("ns_experiment_storyboard::load_events_from_db()An event type must be specified.");
	vector<ns_64_bit> region_ids;
	vector<ns_64_bit> experiment_ids;
	map<ns_64_bit, ns_64_bit> region_current_detection_set_ids;
	//get all the regions requested
	if (spec.region_id != 0) {
		region_ids.push_back(spec.region_id);
		sql << "SELECT e.id, r.last_timepoint_in_latest_movement_rebuild, r.movement_image_analysis_quantification_id FROM experiments as e, capture_samples as s, sample_region_image_info as r WHERE r.id = "
			<< spec.region_id << " AND r.sample_id = s.id AND s.experiment_id = e.id";

		ns_sql_result res;
		sql.get_rows(res);
		if (res.size() == 0)
			throw ns_ex("Could not load experiment for region ") << spec.region_id;
		experiment_ids.push_back(ns_atoi64(res[0][0].c_str()));
		last_timepoint_in_storyboard = atol(res[0][1].c_str());
		number_of_regions_in_storyboard = 1;
		region_current_detection_set_ids[spec.region_id] = ns_atoi64(res[0][2].c_str());
	}
	else {
		ns_sql_result res;
		if (spec.sample_id != 0) {
			sql << "SELECT r.id,r.last_timepoint_in_latest_movement_rebuild, r.movement_image_analysis_quantification_id FROM sample_region_image_info as r, capture_samples as s  "
				"WHERE r.sample_id = s.id AND s.id = " << spec.sample_id << " AND r.censored = 0 AND r.excluded_from_analysis=0";
			if (spec.strain_to_use.strain.size() > 0)
				sql << " AND r.strain = '" << spec.strain_to_use.strain << "'";
			if (spec.strain_to_use.strain_condition_1.size() > 0)
				sql << " AND r.strain_condition_1 = '" << spec.strain_to_use.strain_condition_1 << "'";
			if (spec.strain_to_use.strain_condition_2.size() > 0)
				sql << " AND r.strain_condition_2 = '" << spec.strain_to_use.strain_condition_2 << "'";

			sql.get_rows(res);
			if (res.size() != 0)
				experiment_ids.push_back(ns_atoi64(res[0][1].c_str()));
		}
		else {
			if (spec.experiment_id == 0)
				throw ns_ex("No data requested!");
			experiment_ids.push_back(spec.experiment_id);
			sql << "SELECT r.id, r.last_timepoint_in_latest_movement_rebuild, r.movement_image_analysis_quantification_id FROM sample_region_image_info as r, capture_samples as s WHERE r.sample_id = s.id AND s.experiment_id = " << spec.experiment_id << " AND r.censored=0 AND latest_movement_rebuild_timestamp != 0 AND s.censored=0";
			if (spec.strain_to_use.strain.size() > 0)
				sql << " AND r.strain = '" << spec.strain_to_use.strain << "'";
			if (spec.strain_to_use.strain_condition_1.size() > 0)
				sql << " AND r.strain_condition_1 = '" << spec.strain_to_use.strain_condition_1 << "'";
			if (spec.strain_to_use.strain_condition_2.size() > 0)
				sql << " AND r.strain_condition_2 = '" << spec.strain_to_use.strain_condition_2 << "'";
			sql.get_rows(res);
		}
		region_ids.resize(res.size());
		number_of_regions_in_storyboard = res.size();
		last_timepoint_in_storyboard = 0;
		for (unsigned int i = 0; i < res.size(); i++) {
			region_ids[i] = ns_atoi64(res[i][0].c_str());
			unsigned long last_timepoint(atol(res[i][1].c_str()));
			if (last_timepoint > this->last_timepoint_in_storyboard)
				last_timepoint_in_storyboard = last_timepoint;
			region_current_detection_set_ids[region_ids[i]] = ns_atoi64(res[i][2].c_str());
		}
	}


	ns_machine_analysis_data_loader machine_annotations;
	machine_annotations.load(ns_death_time_annotation_set::ns_censoring_and_movement_transitions,
		spec.region_id, spec.sample_id, experiment_ids[0], sql, true, ns_machine_analysis_region_data::ns_exclude_fast_moving_animals);
	map<ns_64_bit, vector<string> > problems;
	map<ns_64_bit, ns_64_bit > total_events;
	for (unsigned int i = 0; i < machine_annotations.samples.size(); i++) {
		for (unsigned int j = 0; j < machine_annotations.samples[i].regions.size(); j++) {
			const ns_64_bit & region_id = machine_annotations.samples[i].regions[j].metadata.region_id;
			ns_64_bit current_movement_analysis_id = region_current_detection_set_ids[region_id];
			map<ns_64_bit, ns_64_bit >::iterator count = total_events.insert(pair<ns_64_bit, ns_64_bit >(region_id, 0)).first;
			count->second = 0;
			for (unsigned int k = 0; k < machine_annotations.samples[i].regions[j].death_time_annotation_set.size(); k++) {

				if (machine_annotations.samples[i].regions[j].death_time_annotation_set[k].stationary_path_id.detection_set_id != current_movement_analysis_id) {
					problems[machine_annotations.samples[i].regions[j].metadata.region_id].push_back(machine_annotations.samples[i].regions[j].death_time_annotation_set[k].brief_description());
				}
				count->second++;
			}


			all_events.add(machine_annotations.samples[i].regions[j].death_time_annotation_set);
			all_events.specifiy_region_metadata(machine_annotations.samples[i].regions[j].metadata.region_id,
				machine_annotations.samples[i].regions[j].metadata);
		}
	}
	//display useful diatgnostic info when the file on disk does not match the database spec.
	if (!problems.empty()) {

		ns_ex ex;
		for (map<ns_64_bit, vector<string> >::iterator p = problems.begin(); p != problems.end(); p++) {
			sql << "SELECT s.name,r.name FROM capture_samples as s, sample_region_image_info as r WHERE r.id = "
				<< p->first << " AND s.id=r.sample_id";
			ns_sql_result res;
			sql.get_rows(res);
			std::string region_name;
			if (res.size() > 0) {
				region_name = res[0][0] + "::" + res[0][1];
			}
			else region_name = ns_to_string(p->first);

			if (p->second.size() == total_events[p->first]) {
				ex << " All records on disk corresponding to region " << region_name << " are out of date.  Rerun movement analysis or censor the region to allow storyboard generation.\n";
			}
			else if (2 * p->second.size() > total_events[p->first]) {
				ex << " Most (" << ns_to_string(p->second.size()) + " / " + ns_to_string(total_events[p->first]) << ") of the records on disk corresponding to region " << region_name << " are out-of-date.  Rerun movement analysis or censor the region to allow storyboard generation.\n";
			}
			else image_server.add_subtext_to_current_event((std::string("Ignoring ") + ns_to_string(p->second.size()) + " of the records on disk corresponding to region " + region_name + "  are out-of-date.  If problems occur in storyboard generation,  Rerun movement analysis or censor this region.").c_str(), &sql);

			if (image_server.verbose_debug_output()) {
				for (map<ns_64_bit, vector<string> >::iterator p = problems.begin(); p != problems.end(); p++) {
					cout << "**Region " << p->first << "\n";
					for (unsigned int i = 0; i < p->second.size(); i++) {
						cout << p->second[i] << "\n";
					}
				}
			}
		}
		if (!ex.text().empty())
			throw ex;
	}

	//load by-hand annotations
	ns_hand_annotation_loader by_hand_annotations;
	if (spec.region_id != 0)
		by_hand_annotations.load_region_annotations(ns_death_time_annotation_set::ns_censoring_and_movement_transitions, spec.region_id, sql);
	else
		by_hand_annotations.load_experiment_annotations(ns_death_time_annotation_set::ns_censoring_and_movement_transitions, experiment_ids[0], sql);
	all_events.add(by_hand_annotations.annotations, ns_death_time_annotation_compiler::ns_do_not_create_regions_or_locations);
}
bool ns_experiment_storyboard_compiled_event_set::need_to_reload_for_new_spec(const ns_experiment_storyboard_spec & new_spec) {

	return !(new_spec.experiment_id == spec.experiment_id &&
		new_spec.region_id == spec.region_id &&
		new_spec.sample_id == spec.sample_id &&
		new_spec.strain_to_use.strain == spec.strain_to_use.strain &&
		new_spec.strain_to_use.strain_condition_1 == spec.strain_to_use.strain_condition_1 &&
		new_spec.strain_to_use.strain_condition_2 == spec.strain_to_use.strain_condition_2);
}

bool ns_experiment_storyboard::create_storyboard_metadata_from_machine_annotations(ns_experiment_storyboard_spec spec, const ns_experiment_storyboard_compiled_event_set & compiled_event_set,ns_sql & sql) {
	

	subject_specification = spec;

	last_timepoint_in_storyboard = compiled_event_set.last_timepoint_in_storyboard;
	number_of_regions_in_storyboard = compiled_event_set.number_of_regions_in_storyboard;


	//find the time of the last death, if that is the time at which we are pulling images.
	if (subject_specification.choose_images_from_time_of_last_death){
		time_of_last_death = 0;
		unsigned long number_of_events(0);
		for (ns_death_time_annotation_compiler::ns_region_list::const_iterator r = compiled_event_set.all_events.regions.begin(); r != compiled_event_set.all_events.regions.end();r++){
			for (ns_death_time_annotation_compiler_region::ns_location_list::const_iterator q = r->second.locations.begin(); q !=  r->second.locations.end();q++){
					number_of_events++;
			}
		}
		if (spec.choose_images_from_time_of_last_death){
			std::vector<unsigned long> death_times;
			death_times.reserve(number_of_events);	
			for (ns_death_time_annotation_compiler::ns_region_list::const_iterator r = compiled_event_set.all_events.regions.begin(); r != compiled_event_set.all_events.regions.end();r++){
				for (ns_death_time_annotation_compiler_region::ns_location_list::const_iterator q = r->second.locations.begin(); q !=  r->second.locations.end();q++){		
					if (q->properties.is_excluded() || q->properties.is_censored())
						continue;
					ns_dying_animal_description_const d(q->generate_dying_animal_description_const(false));
						if (d.machine.death_annotation != 0)
							death_times.push_back(d.machine.death_annotation->time.period_end);
				}
			}
			std::sort(death_times.begin(), death_times.end());
			if (death_times.empty()){
				image_server.register_server_event(ns_image_server::ns_register_in_central_db,ns_image_server_event("ns_experiment_storyboard::load_events_from_annotation_compiler()::No dead animals or potentially dead animals were detected"));
				time_of_last_death = 0;
				//throw ns_ex("");
			}
			else time_of_last_death = death_times[(unsigned long)(death_times.size()*.9)];
		}
	}
	

	return load_events_from_annotation_compiler(ns_creating_from_machine_annotations, compiled_event_set.all_events,spec.use_absolute_time,false,spec.minimum_distance_to_juxtipose_neighbors,sql);

}

void ns_experiment_storyboard::save_by_hand_annotations(ns_sql & sql,const ns_death_time_annotation_set & extra_annotations) const{
	const unsigned long cur_time = ns_current_time();

	//avoid writing duplicates
	map<ns_64_bit,map<ns_64_bit,bool> > worm_ids_written;

	//we make a list of all annotations sorted by region id
	std::map<ns_64_bit,ns_death_time_annotation_set > annotations;
	for (unsigned int i = 0; i < divisions.size(); i++){
		for (unsigned int j = 0; j < divisions[i].events.size(); j++){

			//only output the annotations once for each worm (even if it is included in multiple neighbor groups)
			map<ns_64_bit,map<ns_64_bit,bool> >::iterator pp = worm_ids_written.find(divisions[i].events[j].event_annotation.region_info_id);
			if (pp == worm_ids_written.end())
				worm_ids_written[divisions[i].events[j].event_annotation.region_info_id][divisions[i].events[j].event_annotation.stationary_path_id.group_id] = true;
			else{
				map<ns_64_bit,bool>::iterator q = pp->second.find(divisions[i].events[j].event_annotation.stationary_path_id.group_id);
				if (q == pp->second.end())
					pp->second[divisions[i].events[j].event_annotation.stationary_path_id.group_id] = true;
				else
					continue;
			}

			ns_64_bit region_id(divisions[i].events[j].event_annotation.region_info_id);
			if (subject().region_id != 0 && region_id != subject().region_id)
				continue;
			std::map<ns_64_bit,ns_death_time_annotation_set >::iterator p(annotations.find(region_id));
			if (p==annotations.end())
				p = annotations.insert(std::map<ns_64_bit,ns_death_time_annotation_set >::value_type(region_id,ns_death_time_annotation_set())).first;
			
			if (divisions[i].events[j].event_annotation.annotation_source == ns_death_time_annotation::ns_posture_image ||
				divisions[i].events[j].event_annotation.annotation_source == ns_death_time_annotation::ns_region_image ||
				divisions[i].events[j].event_annotation.annotation_source == ns_death_time_annotation::ns_storyboard){
					ns_death_time_annotation a(divisions[i].events[j].event_annotation);
					a.annotation_time = cur_time;
					p->second.add(a);
			}
			
			//include an annotation containing sticky properties such as censoring, exclusion, and flags.
			if (divisions[i].events[j].event_annotation.has_sticky_properties()){
				ns_death_time_annotation a(divisions[i].events[j].event_annotation);
				a.annotation_source = ns_death_time_annotation::ns_storyboard;
				a.annotation_source_details = "Worm Terminal::Storyboard Annotation";
				a.type = ns_no_movement_event;
				a.annotation_time = cur_time;
				p->second.add(a);
			}
			//add all by hand annotations, including movement event annotations
			for (unsigned int k = 0; k < divisions[i].events[j].by_hand_movement_annotations_for_element.size(); k++){
				
				ns_death_time_annotation a(divisions[i].events[j].by_hand_movement_annotations_for_element[k].annotation);
				a.stationary_path_id = divisions[i].events[j].event_annotation.stationary_path_id;
				p->second.add(a);
			}
			
		}
	}

	for(unsigned int i = 0; i < orphan_by_hand_annotations.size(); i++){
		const ns_64_bit region_id(orphan_by_hand_annotations[i].region_info_id);
		if (subject().region_id != 0 && region_id != subject().region_id)
			continue;
		std::map<ns_64_bit,ns_death_time_annotation_set >::iterator p(annotations.find(region_id));
		if (p==annotations.end())
			p = annotations.insert(std::map<ns_64_bit,ns_death_time_annotation_set >::value_type(region_id,ns_death_time_annotation_set())).first;
		p->second.add(orphan_by_hand_annotations[i]);	
	}

	for(unsigned int i = 0; i < extra_annotations.size(); i++){
		const ns_64_bit region_id(extra_annotations[i].region_info_id);
		if (subject().region_id != 0 && region_id != subject().region_id)
			continue;
		std::map<ns_64_bit,ns_death_time_annotation_set >::iterator p(annotations.find(region_id));
		if (p==annotations.end())
			p = annotations.insert(std::map<ns_64_bit,ns_death_time_annotation_set >::value_type(region_id,ns_death_time_annotation_set())).first;
		p->second.add(extra_annotations[i]);	
	}

	for (std::map<ns_64_bit,ns_death_time_annotation_set >::iterator p = annotations.begin(); p != annotations.end(); p++){
		if (p->second.events.size() == 0)
			continue;
		if (subject().region_id != 0 && p->first != subject().region_id)
			continue;
		try{
			ns_region_annotation_file_cache_type::iterator q = region_annotation_file_cache.find(p->first);
			if (q == region_annotation_file_cache.end()){
				ns_image_server_results_subject sub;
				sub.region_id = p->first;
				q = region_annotation_file_cache.insert(ns_region_annotation_file_cache_type::value_type(p->first,
					image_server.results_storage.hand_curated_death_times(sub,sql))).first;
			}
			ns_acquire_for_scope<std::ostream> out(q->second.output());
			if (out.is_null())
				throw ns_ex("Could not open output file:") << q->second.output_filename();
			p->second.write(out());
			out.release();
		}
		catch(ns_ex & ex){
			cerr << "Could not save all annotation data: " << ex.text() << "\n";
		}
	}
}
//int ff = 0;
void ns_experiment_storyboard::calculate_worm_positions(){
	worm_images_size.resize(0);
	worm_images_size.resize(1,ns_vector_2i(INTER_COLUMN_MARGIN,0));
	
	for (unsigned int i = 0; i < divisions.size(); i++){
		divisions[i].calculate_worm_positions();
		if (divisions[i].size.x >= MAX_SUB_IMAGE_WIDTH)
			throw ns_ex("ns_experiment_storyboard::calculate_worm_positions()::"
			"A division larger than the maximum sub-image width discovered!"
			) << divisions[i].size.x << "," << divisions[i].size.y << " (" << divisions[i].events.size() << " elements)";


		if (worm_images_size.rbegin()->x + divisions[i].size.x > MAX_SUB_IMAGE_WIDTH)
			worm_images_size.resize(worm_images_size.size()+1,ns_vector_2i(INTER_COLUMN_MARGIN,0));
		divisions[i].sub_image_id = worm_images_size.size()-1;

		divisions[i].position_on_storyboard.x = worm_images_size.rbegin()->x;

		if(divisions[i].size.y >  worm_images_size.rbegin()->y)
			worm_images_size.rbegin()->y = divisions[i].size.y;
		worm_images_size.rbegin()->x+=divisions[i].size.x+INTER_COLUMN_MARGIN;

	}

	//align everything with the bottom rather than the top of the image

	unsigned long worm_count(0);
	for (unsigned int i = 0; i < divisions.size(); i++){
		divisions[i].position_on_storyboard.y = worm_images_size[divisions[i].sub_image_id].y - divisions[i].size.y;
		worm_count+=divisions[i].events.size();
	}
	cerr << worm_count << " animals found, divided into " << divisions.size() << " divisions and " << worm_images_size.size() << " sub images.\n";

}


ns_division_count ns_experiment_storyboard::count_of_animals_in_division(const unsigned long number_of_divisions,const std::vector<unsigned long > & values,unsigned long start_index,unsigned long size){
	const unsigned long division_width((values[start_index+size-1]-values[start_index])/number_of_divisions);
	const unsigned long start_time(values[start_index]);
	ns_division_count largest_group;
	largest_group.group_number= 0;
	largest_group.group_population = 0;
	largest_group.start_index = 0;
	largest_group.size = 0;

	ns_division_count current_group;
	current_group.group_number = 0;
	current_group.group_population = 0;
	current_group.start_index = 0;
	current_group.size = 0;
	current_group.division_time_length=division_width;
	for (unsigned int i = 0; i < values.size(); i++){
		if (current_group.group_number*division_width+start_time < values[i]){
			if (largest_group.group_population < current_group.group_population)
				largest_group = current_group;
			current_group.group_number++;
			current_group.group_population=0;
			start_index = i;
		}
		current_group.group_population++;
		current_group.size++;
	}
	if (largest_group.group_population < current_group.group_population)
				largest_group = current_group;

	return largest_group;
}
unsigned long ns_experiment_storyboard::label_margin_height() const{
	unsigned long h(worm_images_size[0].y/6);
	return (h<40?h:40);
}
void ns_experiment_storyboard::draw_label_margin(const unsigned long sub_image_id,bool use_color,ns_image_standard & im) const{
	unsigned long h(label_margin_height());
	int c(use_color?3:1);

	for (unsigned int y = im.properties().height-(9*h)/10; y < im.properties().height-(4*h)/5; y++){
		for (unsigned int x = 0; x < c*im.properties().width; x++){
			im[y][x] = 255;
		}
	}
	ns_acquire_lock_for_scope font_lock(font_server.default_font_lock, __FILE__, __LINE__);
	ns_font &font(font_server.get_default_font());
	font.set_height(h/3);

	for (unsigned int i = 0; i < divisions.size(); i++){
		if (divisions[i].events.empty() || divisions[i].sub_image_id != sub_image_id)
			continue;
		if (use_color){
			for (unsigned int y = 0; y < h/5; y++){
				for (unsigned int x = 0; x < h/10; x++){
					im[im.properties().height-(4*h)/5][3*(divisions[i].position_on_storyboard.x+x)] = 0;
					im[im.properties().height-(4*h)/5][3*(divisions[i].position_on_storyboard.x+x)+1] = 0;
					im[im.properties().height-(4*h)/5][3*(divisions[i].position_on_storyboard.x+x)+2] = 0;
				}
			}
		}else{
			for (unsigned int y = 0; y < h/5; y++){
				for (unsigned int x = 0; x < h/10; x++){
					im[im.properties().height-(4*h)/5][divisions[i].position_on_storyboard.x+x] = 0;
				}
			}
		}
		std::string text;
		if (subject_specification.use_absolute_time)
			text = ns_format_time_string_for_human(divisions[i].time);
		else text = ns_to_string_short(divisions[i].time/60.0/60.0/24,2);
		if (use_color)
			font.draw_color(1+3*divisions[i].position_on_storyboard.x,im.properties().height-(4*h/10),ns_color_8(255,255,255),text,im);
		else font.draw_grayscale(1+divisions[i].position_on_storyboard.x,im.properties().height-(4*h/10),255,text,im);
	}
	font_lock.release();
}

std::string ns_experiment_storyboard::image_suffix(const ns_experiment_storyboard_spec & spec){

	std::string r;
	r+= ns_movement_event_to_label(spec.event_to_mark);
	r+= "_";
	
	if (spec.strain_to_use.strain.empty())
		r+= "all";
	else r+= spec.strain_to_use.strain;
	r+="_";
	if (spec.strain_to_use.strain_condition_1.empty())
		r+= "all";
	else r+= spec.strain_to_use.strain_condition_1;
	r+="_";
	if (spec.strain_to_use.strain_condition_2.empty())
		r+= "all";
	else r+= spec.strain_to_use.strain_condition_2;
	r+="_";
	r+=spec.use_by_hand_annotations?"machine":"hand";
	r+="_";
	r+=spec.use_absolute_time?"abs":"rel";
	r+="_delay=";
	r+=ns_to_string(spec.delay_time_after_event);
	r+="_";
	r+=spec.choose_images_from_time_of_last_death?"last":"actual";
	return r;
}



void  ns_experiment_storyboard_spec::add_to_xml(ns_xml_simple_writer & xml) const{
	xml.add_tag("ev",this->event_to_mark);
	xml.add_tag("e",this->experiment_id);
	xml.add_tag("r",this->region_id);
	xml.add_tag("s",this->sample_id);
	xml.add_tag("str",this->strain_to_use.strain);
	xml.add_tag("str_1",this->strain_to_use.strain_condition_1);
	xml.add_tag("str_2",this->strain_to_use.strain_condition_2);
	xml.add_tag("abs_t",this->use_absolute_time?"1":"0");
	xml.add_tag("by_hand",this->use_by_hand_annotations?"1":"0");
	xml.add_tag("delay",this->delay_time_after_event);
	xml.add_tag("choose_last",this->choose_images_from_time_of_last_death?"1":"0");
	xml.add_tag("mdjn",minimum_distance_to_juxtipose_neighbors);
}
void ns_experiment_storyboard_spec::from_xml_group(ns_xml_simple_object & obj){
	this->event_to_mark=(ns_movement_event)atol(obj.tag("ev").c_str());
	this->experiment_id = ns_atoi64(obj.tag("e").c_str());
	this->region_id= ns_atoi64(obj.tag("r").c_str());
	this->sample_id= ns_atoi64(obj.tag("s").c_str());
	this->strain_to_use.strain=obj.tag("str");
	this->strain_to_use.strain_condition_1=obj.tag("str_1");
	this->strain_to_use.strain_condition_2=obj.tag("str_2");
	this->use_absolute_time=obj.tag("abs_t")=="1";
	this->use_by_hand_annotations=obj.tag("by_hand")=="1";
	this->delay_time_after_event = atol(obj.tag("delay").c_str());
	this->choose_images_from_time_of_last_death = obj.tag("choose_last") == "1";
	this->minimum_distance_to_juxtipose_neighbors = atol(obj.tag("mdjn").c_str());
}


void ns_experiment_storyboard_timepoint_element::write_metadata(const ns_64_bit division_id,ns_xml_simple_writer & xml) const{
	xml.add_tag("p_t_x",position_on_time_point.x);
	xml.add_tag("p_t_y",position_on_time_point.y);
	xml.add_tag("a",event_annotation.to_string());
	xml.add_tag("ia",annotation_whose_image_should_be_used.to_string());
	xml.add_tag("d_id",division_id);
}
ns_64_bit  ns_experiment_storyboard_timepoint_element::from_xml_group(ns_xml_simple_object & o){
	position_on_time_point.x = atol(o.tag("p_t_x").c_str());
	position_on_time_point.y = atol(o.tag("p_t_y").c_str());
	string r = o.tag("a");
	event_annotation.from_string(r);
	r = o.tag("ia");
	annotation_whose_image_should_be_used.from_string(r);
	return ns_atoi64(o.tag("d_id").c_str());
}

bool ns_experiment_storyboard::read_metadata(std::istream & in,ns_sql & sql){
	ns_xml_simple_object_reader xml;
	xml.from_stream(in);
	unsigned long worm_images_size_in_record;
	vector<ns_experiment_storyboard_timepoint_element> elements;
	bool spec_found(false);
	for (unsigned int i = 0; i < xml.objects.size(); i++){
		if (xml.objects[i].name == "spec"){
			subject_specification.from_xml_group(xml.objects[i]);
			spec_found = true;
		}
		else if (xml.objects[i].name == "g"){
			first_time = atol(xml.objects[i].tag("ft").c_str());
			last_time = atol(xml.objects[i].tag("lt").c_str());
			worm_images_size_in_record = atol(xml.objects[i].tag("wis").c_str());
			worm_images_size.resize(worm_images_size_in_record,ns_vector_2i(0,0));
			time_of_last_death = atol(xml.objects[i].tag("ld").c_str());
		}
		else if (xml.objects[i].name == "wi"){
			ns_64_bit id(ns_atoi64(xml.objects[i].tag("i").c_str()));
			if (id >= worm_images_size.size())
				worm_images_size.resize(id+1,ns_vector_2i(0,0));
			worm_images_size[id].x = atol(xml.objects[i].tag("wis_x").c_str());
			worm_images_size[id].y = atol(xml.objects[i].tag("wis_y").c_str());
		}
		else if (xml.objects[i].name == "div"){
			//we rebuild these from the annotations.
		}
		else if (xml.objects[i].name == "e"){
			elements.resize(elements.size()+1);
			elements.rbegin()->from_xml_group(xml.objects[i]);
		}
		else throw ns_ex("ns_experiment_storyboard::read_metadata()::Unknown xml tag: ") << xml.objects[i].name;
	}
	if (!spec_found)
		throw ns_ex("Could not find storyboard specification in metadata file.");
	ns_64_bit experiment_id(0);
	if (subject_specification.experiment_id != 0)
		experiment_id =subject_specification.experiment_id;
	else if (subject_specification.sample_id != 0){
		sql << "SELECT experiment_id FROM capture_samples WHERE id = " << subject_specification.sample_id;
		ns_sql_result res;
		sql.get_rows(res);
		if(res.size() == 0)
			throw ns_ex("ns_experiment_storyboard::read_metadata()::Could not load experiment information for sample ") << subject_specification.sample_id;
		experiment_id = ns_atoi64(res[0][0].c_str());
	}
	else if (subject_specification.region_id != 0){
		sql << "SELECT s.experiment_id FROM sample_region_image_info as r, capture_samples as s WHERE r.id = " << subject_specification.region_id << " AND r.sample_id = s.id";
		ns_sql_result res;
		sql.get_rows(res);
		if(res.size() == 0)
			throw ns_ex("ns_experiment_storyboard::read_metadata()::Could not load experiment information for region ") << subject_specification.region_id;
		experiment_id = ns_atoi64(res[0][0].c_str());
	}

	if (worm_images_size_in_record != worm_images_size.size()) {
		throw ns_ex("Metadata file is not consistant in the number of sub images in the image.");
	}
	
	ns_death_time_annotation_compiler all_events;
	{
		
		ns_death_time_annotation_set events_specified_in_storyboard_file;
		for (unsigned int i = 0; i < elements.size(); i++){
			events_specified_in_storyboard_file.add(elements[i].event_annotation);
			events_specified_in_storyboard_file.add(elements[i].annotation_whose_image_should_be_used);
		}
		all_events.add(events_specified_in_storyboard_file);
		if (subject_specification.region_id != 0){
			for (ns_death_time_annotation_compiler::ns_region_list::iterator p = all_events.regions.begin(); p != all_events.regions.end(); p++){
				ns_hand_annotation_loader by_hand_annotations;
				by_hand_annotations.load_region_annotations(ns_death_time_annotation_set::ns_censoring_and_movement_transitions,p->first,sql);
				all_events.add(by_hand_annotations.annotations,ns_death_time_annotation_compiler::ns_do_not_create_regions_or_locations);
			}
		}
		else{
			ns_hand_annotation_loader by_hand_annotations;
			by_hand_annotations.load_experiment_annotations(ns_death_time_annotation_set::ns_censoring_and_movement_transitions,experiment_id,sql);
			all_events.add(by_hand_annotations.annotations,ns_death_time_annotation_compiler::ns_do_not_create_regions_or_locations);
		}
		for (ns_death_time_annotation_compiler::ns_region_list::iterator p = all_events.regions.begin(); p != all_events.regions.end(); ++p)
			if (p->second.metadata.region_id == 0)
				p->second.metadata.load_from_db(p->first,"",sql);
		
	}
	bool loaded = load_events_from_annotation_compiler(ns_loading_from_storyboard_file,all_events,subject_specification.use_absolute_time,true,subject_specification.minimum_distance_to_juxtipose_neighbors,sql);
	if (number_of_sub_images() != worm_images_size_in_record) {
		throw ns_ex("The events contained in the storyboard file (") << number_of_sub_images() << ") did not render into the expected number of sub images, specified in the metatadata stored on disk (" << worm_images_size_in_record << ")";
	}
	return loaded;
}

void ns_experiment_storyboard::write_metadata(std::ostream & o) const{
	//cout << " Writing metadata to disk containing " << number_of_images_in_storyboard() << " images.\n";
	ns_xml_simple_writer xml;
	xml.generate_whitespace();
	xml.add_header();
	xml.start_group("spec");
	subject_specification.add_to_xml(xml);
	xml.end_group();
	xml.start_group("g");
	xml.add_tag("ft",first_time);
	xml.add_tag("lt",last_time);
	xml.add_tag("wis",number_of_images_in_storyboard());
	xml.add_tag("ld",time_of_last_death);
	xml.end_group();
	for (unsigned int i = 0; i < worm_images_size.size(); i++){
		xml.start_group("wi");
		xml.add_tag("i",i);
		xml.add_tag("wis_x",worm_images_size[i].x);
		xml.add_tag("wis_y",worm_images_size[i].y);
		xml.end_group();
	}
	for (unsigned int i = 0; i < divisions.size(); i++){
		xml.start_group("div");
		xml.add_tag("i",i);
		xml.add_tag("ps_x",	divisions[i].position_on_storyboard.x);
		xml.add_tag("ps_y",	divisions[i].position_on_storyboard.y);
		xml.add_tag("s_x",	divisions[i].size.x);
		xml.add_tag("s_y",	divisions[i].size.y);
		xml.add_tag("t", divisions[i].time);
		xml.end_group();
	}
	
	for (unsigned int i = 0; i < divisions.size(); i++){
		for (unsigned int j = 0; j < divisions[i].events.size(); j++){
			xml.start_group("e");
			divisions[i].events[j].write_metadata(i,xml);
			xml.end_group();
		}
	}
	xml.add_footer();
	o << xml.result();
	
}

ns_ex ns_experiment_storyboard::compare(const ns_experiment_storyboard & s){
	ns_ex ex;
	if (s.divisions.size() != divisions.size())
		ex << "Storyboards have different numbers of divisions: " << divisions.size() << " vs " << s.divisions.size() << "\n";;
	unsigned long stop = divisions.size();
	if (s.divisions.size() < stop)
		stop = s.divisions.size();
	for (unsigned int i = 0; i < stop; i++){
		if (s.divisions[i].events.size() != divisions[i].events.size())
			ex << "Storyboards have different number of events in division " << i << ":" << divisions[i].events.size() << " vs " << s.divisions[i].events.size();
		else{
			for (unsigned int j = 0; j < divisions[i].events.size(); j++){
				if (divisions[i].events[j].event_annotation.time.period_start !=
					s.divisions[i].events[j].event_annotation.time.period_start)
					ex << "Storyboards have different event times for division " << i << " event " << j << ": " 
					   << divisions[i].events[j].event_annotation.time.period_start << " vs "
					   << s.divisions[i].events[j].event_annotation.time.period_start << "\n";
				if (divisions[i].events[j].event_annotation.time.period_end !=
					s.divisions[i].events[j].event_annotation.time.period_end)
					ex << "Storyboards have different event times for division " << i << " event " << j << ": " 
					   << divisions[i].events[j].event_annotation.time.period_end << " vs " 
					   << s.divisions[i].events[j].event_annotation.time.period_end << "\n";
				if (divisions[i].events[j].storyboard_absolute_time !=
					s.divisions[i].events[j].storyboard_absolute_time)
					ex << "Storyboards have different storyboard absolute times for division " << i << " event " << j << ": " 
					   << divisions[i].events[j].storyboard_absolute_time << " vs " 
					   << s.divisions[i].events[j].storyboard_absolute_time << "\n";
				if (divisions[i].events[j].event_image_size().x != s.divisions[i].events[j].event_image_size().x ||
					divisions[i].events[j].event_image_size().y != s.divisions[i].events[j].event_image_size().y)
					ex << "Storyboards have different sizes for division " << i << " event " << j << ": " 
					   << divisions[i].events[j].event_image_size() << " vs " 
					   << s.divisions[i].events[j].event_image_size() << "\n";

				if (divisions[i].events[j].annotation_whose_image_should_be_used.time.period_start !=
					s.divisions[i].events[j].annotation_whose_image_should_be_used.time.period_start)
					ex << "Storyboards have different image event times for division " << i << " event " << j << ": " 
					   << divisions[i].events[j].annotation_whose_image_should_be_used.time.period_start << " vs "
					   << s.divisions[i].events[j].annotation_whose_image_should_be_used.time.period_start << "\n";
				if (divisions[i].events[j].annotation_whose_image_should_be_used.time.period_end !=
					s.divisions[i].events[j].annotation_whose_image_should_be_used.time.period_end)
					ex << "Storyboards have different image event times for division " << i << " event " << j << ": " 
					   << divisions[i].events[j].annotation_whose_image_should_be_used.time.period_end << " vs " 
					   << s.divisions[i].events[j].annotation_whose_image_should_be_used.time.period_end << "\n";
				if (divisions[i].events[j].storyboard_absolute_time !=
					s.divisions[i].events[j].storyboard_absolute_time)
					ex << "Storyboards have different storyboard image absolute times for division " << i << " event " << j << ": " 
					   << divisions[i].events[j].storyboard_absolute_time << " vs " 
					   << s.divisions[i].events[j].storyboard_absolute_time << "\n";
				if (divisions[i].events[j].event_image_size().x != s.divisions[i].events[j].event_image_size().x ||
					divisions[i].events[j].event_image_size().y != s.divisions[i].events[j].event_image_size().y)
					ex << "Storyboards have different image sizes for division " << i << " event " << j << ": " 
					   << divisions[i].events[j].image_image_size() << " vs " 
					   << s.divisions[i].events[j].image_image_size() << "\n";

				if (divisions[i].events[j].position_on_time_point.x != s.divisions[i].events[j].position_on_time_point.x ||
					divisions[i].events[j].position_on_time_point.y != s.divisions[i].events[j].position_on_time_point.y)
					ex << "Storyboards have different poisitions for division " << i << " event " << j << ": " 
					   << divisions[i].events[j].position_on_time_point << " vs " 
					   << s.divisions[i].events[j].position_on_time_point << "\n";
			}
		}
	}

	return ex;
}

bool ns_experiment_storyboard_manager::load_metadata_from_db(const ns_experiment_storyboard_spec & spec, ns_experiment_storyboard & storyboard, ns_sql & sql){
	load_metadata_from_db(spec,sql);
	load_subimages_from_db(spec,sql);
	ns_acquire_for_scope<ifstream> i(image_server.image_storage.request_metadata_from_disk(xml_metadata_database_record,false,&sql));
	storyboard.read_metadata(i(),sql);
	i().close();
	i.release();
	if (this->sub_images.size() != storyboard.number_of_sub_images())
		throw ns_ex("ns_experiment_storyboard_manager::load_metadata_from_db()::The database (") << sub_images.size() 
		<< ") and metadata file (" << storyboard.number_of_sub_images() << ") disagree on how many sub-images exist for the storyboard.";
	//create_records_and_storage_for_subimages(storyboard.number_of_sub_images(),spec,sql,false);
	//sub_images.resize(storyboard.number_of_sub_images());
	return true;
}	

bool ns_experiment_storyboard_manager::load_image_from_db(const unsigned long image_id,const ns_experiment_storyboard_spec & spec, ns_image_standard & im,ns_sql & sql){
	if (image_id >= sub_images.size())
		throw ns_ex("ns_experiment_storyboard_manager::load_image_from_db()::Requesting invalid sub image");
	if (sub_images[image_id].id == 0)
		throw ns_ex("ns_experiment_storyboard_manager::load_image_from_db()::Metadata has not been loaded");
	ns_image_storage_source_handle<ns_8_bit> in(image_server.image_storage.request_from_storage(sub_images[image_id],&sql));
	in.input_stream().pump(im,1024);
	return true;
}
void ns_experiment_storyboard_manager::save_metadata_to_db(const ns_experiment_storyboard_spec & spec, const ns_experiment_storyboard & storyboard, const ns_image_type & image_type,ns_sql & sql){
	create_records_and_storage_for_subimages(storyboard.number_of_sub_images(),spec,sql,true);
	save_metadata_to_db(spec,sql);
	ns_acquire_for_scope<ofstream> o(image_server.image_storage.request_metadata_output(xml_metadata_database_record, image_type,false,&sql));
	storyboard.write_metadata(o());
	xml_metadata_database_record.save_to_db(xml_metadata_database_record.id,&sql);
	o().close();
	o.release();
}

void ns_experiment_storyboard_manager::save_image_to_db_no_error_handling(const unsigned long sub_image_id, const ns_experiment_storyboard_spec & spec, const ns_image_standard & im, ns_sql & sql) const {
	if (sub_image_id >= sub_images.size())
		throw ns_ex("ns_experiment_storyboard_manager::save_image_to_db()::Requesting invalid sub image");
	if (sub_images[sub_image_id].id == 0)
		throw ns_ex("ns_experiment_storyboard_manager::save_image_to_db()::Metadata has not been established");
	bool had_to_use_volatile;
	ns_image_server_image image_record = sub_images[sub_image_id];
	get_default_storage_base_filenames(sub_image_id, image_record, ns_tiff_lzw, spec, sql);
	///cout << "Writing " << image_record.filename << " to " << image_record.id  << "\n";
	image_record.save_to_db(image_record.id, &sql);
	ns_image_storage_reciever_handle<ns_8_bit> h(image_server.image_storage.request_storage(image_record, ns_tiff, 1.0, 1024, &sql, had_to_use_volatile, true, false));
	im.pump(h.output_stream(), 1024);
}


void ns_experiment_storyboard_manager::save_image_to_db(const unsigned long sub_image_id, const ns_experiment_storyboard_spec & spec, const ns_image_standard & im, ns_sql & sql){
	if (sub_image_id >= sub_images.size())
		throw ns_ex("ns_experiment_storyboard_manager::save_image_to_db()::Requesting invalid sub image");
	if (sub_images[sub_image_id].id == 0)
		throw ns_ex("ns_experiment_storyboard_manager::save_image_to_db()::Metadata has not been established");
	bool had_to_use_volatile;
	try {
		ns_image_storage_reciever_handle<ns_8_bit> h(image_server.image_storage.request_storage(sub_images[sub_image_id], ns_tiff, 1.0, 1024, &sql, had_to_use_volatile, true, false));
		im.pump(h.output_stream(), 1024);
	}
	catch (ns_ex & ex) {
		//this can fail if an unusual record previously existed in the database previous to this current round of storyboard generation.
		//before giving up, we first try to make a fresh record and delete the old one.

		sql << "DELETE FROM images WHERE id = " << sub_images[sub_image_id].id;
		sql.send_query();
		//get a new image record and save it to the db
		get_default_storage_base_filenames(sub_image_id, sub_images[sub_image_id], ns_tiff_lzw, spec, sql);
		//cout << "Re-Writing " << sub_images[sub_image_id].filename << " to " << sub_images[sub_image_id].id << "\n";
		sub_images[sub_image_id].save_to_db(sub_images[sub_image_id].id, &sql);
		//update the storyboard db record to point to the new image
		sql << "UPDATE animal_storyboard SET image_id = " << sub_images[sub_image_id].id << " WHERE " << generate_sql_query_where_clause_for_specification(spec)
			<< " AND storyboard_sub_image_number = " << sub_image_id;
		sql.send_query();
		//try to open the new image.
		ns_image_storage_reciever_handle<ns_8_bit> h(image_server.image_storage.request_storage(sub_images[sub_image_id], ns_tiff, 1.0, 1024, &sql, had_to_use_volatile, true, false));
		im.pump(h.output_stream(), 1024);
		//if this fails, it will throw the error.
	}
}

void ns_experiment_storyboard_manager::load_metadata_from_db(const ns_experiment_storyboard_spec & spec,ns_sql & sql){
	sql << "SELECT id, image_id, metadata_id, number_of_sub_images FROM animal_storyboard WHERE " << generate_sql_query_where_clause_for_specification(spec);
	ns_sql_result res;
	sql.get_rows(res);	
	if (res.size() == 0)
		throw ns_ex("Could not find storyboard in database.");
	ns_64_bit m_id(ns_atoi64(res[0][2].c_str()));
	try{
		for (unsigned int i = 0; i < res.size(); i++){
			if (atol(res[i][3].c_str()) != res.size())
				throw ns_ex("ns_experiment_storyboard_manager::load_metadata_from_db()::Inconsistant records of sub_image_count!");
			if (ns_atoi64(res[i][2].c_str()) != m_id)
				throw ns_ex("ns_experiment_storyboard_manager::load_metadata_from_db()::Inconsistant records of metadata id!");
		}
	}
	catch(ns_ex & ex){
		delete_metadata_from_db(spec,sql);
		throw ex;
	}
	xml_metadata_database_record.load_from_db(m_id,&sql);
}


void ns_experiment_storyboard_manager::delete_metadata_from_db(const ns_experiment_storyboard_spec & spec, ns_sql & sql){

	sql << "SELECT image_id,metadata_id FROM animal_storyboard WHERE " << generate_sql_query_where_clause_for_specification(spec);
	ns_sql_result res;
	sql.get_rows(res);
	if (res.size() == 0)
		return;
		
	
	for (unsigned int i = 0; i < res.size(); i++){
		sql << "DELETE FROM images WHERE id = " << res[i][0];
		sql.send_query();
	}
	sql << "DELETE FROM images WHERE id = " << res[0][1];
	sql.send_query();

	sql << "DELETE FROM animal_storyboard WHERE " << generate_sql_query_where_clause_for_specification(spec);
	sql.send_query();
}

void ns_experiment_storyboard_manager::save_metadata_to_db(const ns_experiment_storyboard_spec & spec,ns_sql & sql){
	get_default_storage_base_filenames(0, xml_metadata_database_record,ns_xml,spec,sql);
	xml_metadata_database_record.save_to_db(xml_metadata_database_record.id,&sql);
	sql << "UPDATE animal_storyboard SET metadata_id=" << xml_metadata_database_record.id << " WHERE " << generate_sql_query_where_clause_for_specification(spec);
	sql.send_query();
}
bool ns_experiment_storyboard_manager::load_subimages_from_db(const ns_experiment_storyboard_spec & spec,ns_sql & sql){

	sql << "SELECT id, image_id, metadata_id, number_of_sub_images,minimum_distance_to_juxtipose_neighbors,storyboard_sub_image_number FROM animal_storyboard WHERE " << generate_sql_query_where_clause_for_specification(spec);
	ns_sql_result res;
	sql.get_rows(res);
	sub_images.resize(0);		
	if (res.size() == 0)
		return false;
	sub_images.resize(res.size());
	//cout << "Loading " << res.size() << " sub images from disk";
	ns_64_bit m_id(ns_atoi64(res[0][2].c_str()));
	try{
		for (unsigned int i = 0; i < res.size(); i++){
			ns_64_bit sub_image_number = ns_atoi64(res[i][5].c_str());
			if (sub_image_number >= sub_images.size())
				throw ns_ex("An invalid storyboard sub image record was found in the database.");

			sub_images[sub_image_number].load_from_db(ns_atoi64(res[i][1].c_str()),&sql);
			if (ns_atoi64(res[i][3].c_str()) != res.size())
				throw ns_ex("ns_experiment_storyboard_manager::load_subimages_from_db()::Inconsistant records of sub_image_count!");
			
			if (ns_atoi64(res[i][2].c_str()) != m_id)
				throw ns_ex("ns_experiment_storyboard_manager::load_subimages_from_db()::Inconsistant records of metadata id!");

			if (ns_atoi64(res[i][4].c_str()) != spec.minimum_distance_to_juxtipose_neighbors)
				throw ns_ex("ns_experiment_storyboard_manager::load_subimages_from_db()::Inconsistant records of minimum_distance_to_juxtipose_neighbors!");
		}
	}
	catch(ns_ex & ex){
		sql << "DELETE FROM animal_storyboard WHERE " << generate_sql_query_where_clause_for_specification(spec);
		sql.send_query();
		throw ex;
	}
	return true;
}

 std::string ns_experiment_storyboard_manager::generate_sql_query_where_clause_for_specification(const ns_experiment_storyboard_spec & spec) {
	 ns_text_stream_t ex;
	 ex << "  region_id = " << spec.region_id
		 << " AND sample_id = " << spec.sample_id << " AND experiment_id = " << spec.experiment_id
		 << " AND using_by_hand_annotations = " << (spec.use_by_hand_annotations ? "1" : "0")
		 << " AND movement_event_used=" << (long)spec.event_to_mark
		 << " AND aligned_by_absolute_time = " << (spec.use_absolute_time ? "1" : "0")
		 << " AND images_chosen_from_time_of_last_death = " << (spec.choose_images_from_time_of_last_death ? "1" : "0")
		 << " AND image_delay_time_after_event = " << spec.delay_time_after_event;
	 return ex.text();
}


void ns_experiment_storyboard_manager::create_records_and_storage_for_subimages(const unsigned long number_of_subimages,const ns_experiment_storyboard_spec & spec,ns_sql & sql, const bool create_if_missing){
	sub_images.resize(number_of_subimages);
	for (unsigned int i = 0; i < sub_images.size(); i++){
		if (sub_images[i].id == 0){
			if (!create_if_missing) throw ns_ex("Information for sub_image " ) << i << " was not loaded.";
			sub_images[i].save_to_db(0,&sql);

		//	cout << "Creating database record containing " << number_of_sub_images() << " images.\n";
			sql << "INSERT INTO animal_storyboard SET region_id = " << spec.region_id
				<< ",sample_id = " << spec.sample_id << ", experiment_id = " << spec.experiment_id
				<< ",using_by_hand_annotations = " << (spec.use_by_hand_annotations ? "1" : "0")
				<< ",movement_event_used=" << (long)spec.event_to_mark
				<< ",aligned_by_absolute_time = " << spec.use_absolute_time
				<< ", images_chosen_from_time_of_last_death = " << (spec.choose_images_from_time_of_last_death ? "1" : "0")
				<< ", image_delay_time_after_event = " << spec.delay_time_after_event
				<< ", image_id = " << sub_images[i].id
				<< ", metadata_id = " << xml_metadata_database_record.id
				<< ", strain = ''"
				<< ", storyboard_sub_image_number = " << i
				<< ", minimum_distance_to_juxtipose_neighbors = " << spec.minimum_distance_to_juxtipose_neighbors
				<< ", number_of_sub_images = " << number_of_sub_images();
			sql.send_query();
		}
		get_default_storage_base_filenames(i,sub_images[i],ns_tiff_lzw,spec,sql);
		//cout << "Writing " << sub_images[i].filename << " to " << sub_images[i].id << "\n";
		sub_images[i].save_to_db(sub_images[i].id,&sql);
	}

	sql << "DELETE FROM animal_storyboard WHERE " << generate_sql_query_where_clause_for_specification(spec)
		<< " AND storyboard_sub_image_number >= " << sub_images.size();
	sql.send_query();
}


	
void ns_experiment_storyboard_manager::get_default_storage_base_filenames(const unsigned long subimage_id,ns_image_server_image & image, const ns_image_type & type,
	const ns_experiment_storyboard_spec & spec,ns_sql & sql) const{
	ns_file_location_specification s;
	if (spec.region_id != 0){
		s = image_server_const.image_storage.get_storyboard_path(0, spec.region_id, subimage_id, ns_experiment_storyboard::image_suffix(spec),type, sql,false);
	}
	else if (spec.experiment_id != 0){
		s = image_server_const.image_storage.get_storyboard_path(spec.experiment_id,0, subimage_id, ns_experiment_storyboard::image_suffix(spec), type, sql,false);
	}
	ns_64_bit id = image.id;
	image = image_server_const.image_storage.get_storage_for_specification(s);
	image.id = id;
	image.host_id = image_server.host_id();
}

