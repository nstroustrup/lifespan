#include "ns_worm_training_set_image.h"
#include "ns_xml.h"
using namespace std;

void ns_worm_training_set_image::generate_check_box(const ns_color_8 & check_color, ns_image_standard & out){
	ns_image_properties prop;
	prop.height = 50;
	prop.width = 50;
	prop.components = 3;
	out.prepare_to_recieve_image(prop);
	for (unsigned int y = 0; y < prop.height; y++){
		for (unsigned int x = 0; x < 3*prop.width; x++)
			out[y][x] = 0;
	}
	for (unsigned int y = 0; y < prop.height; y++){
		for (unsigned int x = 0; x < 6; x++){
			out[y][x] = 255;
			out[y][3*(prop.width-2) +x] = 255;
		}
	}
	for (unsigned int y = 0; y < 2; y++){
		for (unsigned int x = 0; x < 3*prop.width; x++){
			out[y][x] = 255;
			out[prop.height-2+y][x] = 255;
		}
	}
	if (check_color.x != 0 || check_color.y != 0 || check_color.z != 0){
		for (unsigned int y = prop.height/3; y < (2*prop.height)/3; y++){
			for (unsigned int x = (prop.width/3); x < 2*(prop.width/3); x++){
				out[y][3*x] =   check_color.x;
				out[y][3*x+1] = check_color.y;
				out[y][3*x+2] = check_color.z;
			}
		}
	}
}

void ns_worm_training_set_image::generate_check_boxes(){
	check_boxes.resize((int)ns_object_hand_annotation_data::ns_number_of_object_hand_annotation_types);
	for (unsigned int i = 0; i < check_boxes.size(); i++)
		generate_check_box(ns_object_hand_annotation_data::annotation_color((ns_object_hand_annotation_data::ns_object_hand_annotation)i),check_boxes[i]);
	
}


struct ns_training_set_visualization_image_info{
	ns_vector_2i original_region_position_in_source_image;
	ns_vector_2i original_context_position_in_source_image;
	ns_vector_2i original_bitmap_size;
	ns_vector_2i original_context_image_size;
	ns_vector_2i bitmap_offset_in_context_image()const{return original_region_position_in_source_image-original_context_position_in_source_image;}

	ns_object_hand_annotation_data hand_annotation_data;
	
	unsigned long multiple_worm_cluster_group_id;
	unsigned long multiple_worm_cluster_solution_id;
	ns_vector_2i position;
	ns_vector_2i dimensions;

	unsigned long region_info_id;
	unsigned long time;
	static void write_header(ostream & o){
		o << "Identified as a worm by human,"
			 "Identified as a worm by machine,"
			 "Source Region Id,"
			 "Source Image Capture Time,"
			 "Identified as a mis-thresholded worm,"
			 "Identified as a mis-disambiguated multiple worm group,"
			 "Identified as a small larvae,"
			 "Region Position in Source Image X,"
			 "Region Position in Source Image Y,"
			 "Context Image Position in Source Image X,"
			 "Context Image Position in Source Image Y,"
			 "Region Size in Source Image X,"
			 "Region Size in Source Image Y,"
			 "Context Size in Source Image X,"
			 "Context Size in Source Image Y,"
			 "Multiple Worm Cluster Group ID,"
			 "Multiple Worm Cluster Solution ID,"
			 "Position in Visualization X,"
			 "Position in Visualization Y,"
			 "Size in Visualization X,"
			 "Size in Visualization Y";
	}

	void write_to_csv(ostream & o){
		o << (hand_annotation_data.identified_as_a_worm_by_human?"1":"0") << ","
		  << (hand_annotation_data.identified_as_a_worm_by_machine?"1":"0") << ","
		  << region_info_id << ","
		  << time << ","
		  << (hand_annotation_data.identified_as_a_mangled_worm?"1":"0") << ","
		  << (hand_annotation_data.identified_as_misdisambiguated_multiple_worms?"1":"0") << ","
		  << (hand_annotation_data.identified_as_small_larvae?"1":"0") << ","
		  << original_region_position_in_source_image.x << "," << original_region_position_in_source_image.y << "," 
		  << original_context_position_in_source_image.x << "," << original_context_position_in_source_image.y << "," 
		  << original_bitmap_size.x << "," << original_bitmap_size.y << ","
		  << original_context_image_size.x << "," << original_context_image_size.y << ","
		  << multiple_worm_cluster_group_id << ","
		  << multiple_worm_cluster_solution_id << ","
		  << position.x << "," << position.y << ","
		  << dimensions.x << "," << dimensions.y;
	};
};


struct ns_whole_image_statistic_specification_key{
	unsigned long capture_time;
	unsigned long region_id;
};
bool operator<(const ns_whole_image_statistic_specification_key & a, const ns_whole_image_statistic_specification_key & b){
	if (a.region_id != b.region_id)
		return a.region_id < b.region_id;
	return a.capture_time < b.capture_time;
}
struct ns_result_location_region_info{
	unsigned long image_id;
	unsigned long capture_time;
};

class ns_training_set_visualization_metadata_manager{
public:

	void from_collage(const ns_image_standard & im, const bool allow_malformed_metadata=false);

	std::string metadata_to_string(){
		ns_xml_simple_writer writer;
		writer.generate_whitespace(false);
		writer.add_header();
		if (whole_image_stats.empty())
			throw ns_ex("No whole image stats specified!");
		for (ns_whole_image_statistics_list::const_iterator p=whole_image_stats.begin(); p != whole_image_stats.end(); p++){

			writer.start_group("image");

			writer.add_tag("r_id",p->first.region_id);
			writer.add_tag("t",p->first.capture_time);

			writer.add_tag("i1",  p->second.whole_image_region_stats.absolute_intensity_stats.minimum_intensity);
			writer.add_tag("i2",  p->second.whole_image_region_stats.absolute_intensity_stats.maximum_intensity);
			writer.add_tag("i3",  p->second.whole_image_region_stats.absolute_intensity_stats.average_intensity );
			writer.add_tag("i4",  p->second.whole_image_region_stats.relative_intensity_stats.minimum_intensity);
			writer.add_tag("i5",  p->second.whole_image_region_stats.relative_intensity_stats.average_intensity );
			writer.add_tag("i6",  p->second.whole_image_region_stats.relative_intensity_stats.maximum_intensity );
				
			writer.add_tag("w1",  p->second.worm_region_specific_region_stats.absolute_intensity_stats.minimum_intensity);
			writer.add_tag("w2",  p->second.worm_region_specific_region_stats.absolute_intensity_stats.maximum_intensity);
			writer.add_tag("w3",  p->second.worm_region_specific_region_stats.absolute_intensity_stats.average_intensity );
			writer.add_tag("w4", p->second.worm_region_specific_region_stats.relative_intensity_stats.minimum_intensity);
			writer.add_tag("w5",  p->second.worm_region_specific_region_stats.relative_intensity_stats.average_intensity);
			writer.add_tag("w6",  p->second.worm_region_specific_region_stats.relative_intensity_stats.maximum_intensity );

			writer.end_group();
		}


		for (unsigned int i = 0; i < image_info.size(); i++){
			writer.start_group("w");
			writer.add_tag("g",image_info[i].multiple_worm_cluster_group_id);
			writer.add_tag("s",image_info[i].multiple_worm_cluster_solution_id);
			writer.add_tag("ox",image_info[i].original_region_position_in_source_image.x);
			writer.add_tag("oy",image_info[i].original_region_position_in_source_image.y);
			writer.add_tag("cx",image_info[i].original_context_position_in_source_image.x);
			writer.add_tag("cy",image_info[i].original_context_position_in_source_image.y);
			writer.add_tag("bx",image_info[i].original_bitmap_size.x);
			writer.add_tag("by",image_info[i].original_bitmap_size.y);
			writer.add_tag("xx",image_info[i].original_context_image_size.x);
			writer.add_tag("xy",image_info[i].original_context_image_size.y);
			writer.add_tag("px",image_info[i].position.x);
			writer.add_tag("py",image_info[i].position.y);
			writer.add_tag("dx",image_info[i].dimensions.x);
			writer.add_tag("dy",image_info[i].dimensions.y);
			writer.add_tag("r",image_info[i].region_info_id);
			writer.add_tag("t",image_info[i].time);
			writer.add_tag("i",image_info[i].hand_annotation_data.identified_as_a_worm_by_machine?"1":"0");
			writer.add_tag("h",image_info[i].hand_annotation_data.identified_as_a_worm_by_human?"1":"0");
			writer.add_tag("m",image_info[i].hand_annotation_data.identified_as_a_mangled_worm?"1":"0");
			writer.add_tag("d",image_info[i].hand_annotation_data.identified_as_misdisambiguated_multiple_worms?"1":"0");
			writer.add_tag("l",image_info[i].hand_annotation_data.identified_as_small_larvae?"1":"0");
			writer.end_group();
		}
		writer.add_footer();
		return writer.result();
	}


	
	void load_from_death_time_annotation_set(const ns_death_time_annotation_compiler_region & result, ns_sql & sql){
		//get the region_ids for all time points specified in the region
		sql << "SELECT id,capture_time FROM sample_region_images WHERE region_info_id = " << result.metadata.region_id << " ORDER BY capture_time ASC";
		ns_sql_result res;
		sql.get_rows(res);
		map<ns_64_bit,ns_64_bit> region_image_id_sorted_by_time;
		for (unsigned int i = 0; i < res.size(); i++)
			region_image_id_sorted_by_time[ns_atoi64(res[i][1].c_str())] = ns_atoi64(res[i][0].c_str());
		

		//for each event, identify the region_image in which it was annotated
		vector<vector<ns_result_location_region_info> > result_location_region_image_ids;
		result_location_region_image_ids.resize(result.locations.size());
		
		unsigned long total_worm_count(0);
		for (unsigned int i = 0; i < result.locations.size(); i++){
			result_location_region_image_ids[i].resize(result.locations[i].annotations.size());
			total_worm_count += result.locations[i].annotations.size();
			for (unsigned int j = 0; j < result.locations[i].annotations.size(); j++){
				if (result.locations[i].annotations[j].region_info_id != result.metadata.region_id)
					throw ns_ex("Invalid region id found!");
				result_location_region_image_ids[i][j].capture_time = result.locations[i].annotations[j].time.period_end;
				map<ns_64_bit,ns_64_bit>::iterator p(region_image_id_sorted_by_time.find(result.locations[i].annotations[j].time.period_end));
				if (p == region_image_id_sorted_by_time.end()){
					if (result.locations[i].annotations[j].excluded != ns_death_time_annotation::ns_not_excluded)
						result_location_region_image_ids[i][j].image_id = 0;
					else 
						throw ns_ex("Could not identify time point for annotation in database");
				}
				else result_location_region_image_ids[i][j].image_id = p->second;
			}
		}
		//for each training set image, identify the training set source image

		map<unsigned long, ns_image_server_image> training_set_source_images_sorted_by_region_image_id;
		for (unsigned int i = 0; i < result_location_region_image_ids.size(); ++i){
			for (unsigned int j = 0; j < result_location_region_image_ids[i].size(); ++j){
				if (result_location_region_image_ids[i][j].image_id == 0 ||
					training_set_source_images_sorted_by_region_image_id.find(result_location_region_image_ids[i][j].image_id) !=
					training_set_source_images_sorted_by_region_image_id.end()) continue;
				ns_image_server_captured_image_region region;
				region.load_from_db(result_location_region_image_ids[i][j].image_id,&sql);
				ns_image_server_image im(region.request_processed_image(ns_process_add_to_training_set,sql));
				training_set_source_images_sorted_by_region_image_id[result_location_region_image_ids[i][j].image_id] = im;
			}
		}
		//store images in a cache to reduce the network load a little.  This might actually make things slower due to disk swapping,
		//but we can't know without profiling so we take a guess it'll help
		ns_simple_image_cache cache(2048 * 1024);
		ns_image_cache_data_source cache_source;
		cache_source.handler = &image_server.image_storage;
		cache_source.sql = &sql;
		worms.reserve(total_worm_count);
		images.reserve(total_worm_count);
		images_with_boxes.reserve(total_worm_count);
		image_info.reserve(total_worm_count);
		for (unsigned int i = 0; i < result.locations.size(); i++){
			cerr << (int)(100*(float)i/(float)result.locations.size()) << "%";
			for (unsigned int j = 0; j < result.locations[i].annotations.size(); j++){
				ns_simple_image_cache::const_handle_t image;
				cache.get_for_read(
					training_set_source_images_sorted_by_region_image_id[result_location_region_image_ids[i][j].image_id],
					image,cache_source);
				ns_training_set_visualization_metadata_manager m;
				m.from_collage(image().image);
				image.release();
				ns_whole_image_statistic_specification_key key;
				key.capture_time = result.locations[i].annotations[j].time.period_end;
				key.region_id = result.locations[i].annotations[j].region_info_id;
				if (m.whole_image_stats.size() == 1)
					add_region_metadata(key.region_id,key.capture_time,m.whole_image_stats.begin()->second);
				else add_region_metadata(key.region_id,key.capture_time,m.whole_image_stats[key]);

				unsigned long min_w(0);
				unsigned long min_d(10000000);
				for (int w = 0; w < m.image_info.size(); w++){
					//event locations are stored as the center of the object
					//so we search for locations at the center.
					ns_vector_2i vis_pos(m.image_info[w].original_region_position_in_source_image+ m.image_info[w].original_bitmap_size/2),
									annotation_pos(result.locations[i].properties.position + result.locations[i].properties.size/2 /*+ result.locations[i].size/2*/);
					unsigned long d((vis_pos - annotation_pos).squared());
				//	cerr << vis_pos << " <-> " << annotation_pos << "\t" << sqrt((double)d) << "\n";
					if (d < min_d){
						min_w = w;
						min_d = d;
					}
				}
								
				if (min_d > 125*125){
					cerr << "Could not find an animal at specificed location: " << sqrt((double)min_d) << "\n";
					continue;
				}
				cerr << ".";
				//cerr << "Found a match " << sqrt((double)min_d) << " pixels from annotation.\n";
				int s = worms.size();
				worms.resize(s+1);
				images.resize(s+1);
				images_with_boxes.resize(s+1);
				image_info.resize(s+1);

				m.images[min_w].combined_image.pump(images[s].combined_image,1024);
	//			m.images_with_boxes[min_w].pump(images_with_boxes[min_w],1024);
				image_info[s] = m.image_info[min_w];
				image_info[s].multiple_worm_cluster_group_id = s;
				image_info[s].multiple_worm_cluster_solution_id = 0;
				image_info[s].position = ns_vector_2i(0,0);
				image_info[s].dimensions = ns_vector_2i(0,0);
				image_info[s].hand_annotation_data.identified_as_a_worm_by_human = (result.locations[i].properties.excluded != ns_death_time_annotation::ns_by_hand_excluded) && 
					(result.locations[i].annotations[j].excluded != ns_death_time_annotation::ns_by_hand_excluded);
				image_info[s].hand_annotation_data.identified_as_misdisambiguated_multiple_worms = result.locations[i].properties.disambiguation_type==ns_death_time_annotation::ns_part_of_a_mutliple_worm_disambiguation_cluster;
			
			}
		}
	}
	void load_from_mutually_exclusive_worm_groups(const ns_64_bit region_info_id, const unsigned long time,const std::vector< std::vector< std::vector<ns_detected_worm_info *> > > & groups){
		std::vector<ns_detected_worm_info *>::size_type s(0);
		//multiple worm clusters can expand to produce a *ton* of animals.
		//to prevent this we only output a small number of disambiguation solutions.
		const unsigned long max_number_of_solutions_to_output_for_one_worm_cluster(4);
		const unsigned long max_number_of_worms_in_a_solution_to_output(3);
		unsigned long worm_count(0);
		std::vector<std::vector<std::vector<unsigned char> > > output_worm(groups.size());
		for (unsigned int i = 0; i < groups.size(); i++){
			output_worm[i].resize(groups[i].size());
			bool outputted_a_worm(false);
			unsigned long number_of_solutions_to_output_in_this_cluster(0);
			for (unsigned int g = 0; g < groups[i].size(); g++){
				output_worm[i][g].resize(groups[i][g].size(),0);
				if (number_of_solutions_to_output_in_this_cluster > max_number_of_solutions_to_output_for_one_worm_cluster)
					continue;
				if (groups[i][g].size() > max_number_of_worms_in_a_solution_to_output)
					continue;
				number_of_solutions_to_output_in_this_cluster ++;
				for (unsigned int w = 0; w < groups[i][g].size(); w++){
					output_worm[i][g][w] = 1;
					outputted_a_worm = true;
					worm_count++;
				}
			}
		}
		worms.reserve(worm_count);
		image_info.reserve(worm_count);
		for (unsigned int i = 0; i < groups.size(); i++){
			for (unsigned int g = 0; g < groups[i].size(); g++){
				for (unsigned int w = 0; w < groups[i][g].size(); w++){
					if (output_worm[i][g][w] == 0)
						continue;
					s = worms.size();
					worms.resize(s+1,groups[i][g][w]);
					image_info.resize(s+1);
					image_info[s].multiple_worm_cluster_group_id = i;
					image_info[s].multiple_worm_cluster_solution_id = g;
					image_info[s].hand_annotation_data = groups[i][g][w]->hand_annotations;
					image_info[s].region_info_id = region_info_id;
					image_info[s].time = time;
					image_info[s].original_region_position_in_source_image = groups[i][g][w]->region_position_in_source_image;
					image_info[s].original_context_position_in_source_image = groups[i][g][w]->context_position_in_source_image;
					image_info[s].original_bitmap_size = groups[i][g][w]->region_size;
					image_info[s].original_context_image_size = groups[i][g][w]->context_image_size;

					ns_whole_image_statistic_specification_key key;
					key.region_id = image_info[s].region_info_id;
					key.capture_time = image_info[s].time;
					whole_image_stats[key] = groups[i][g][w]->whole_image_stats;

				}
			}
		}
		//generate visualization images
		images.resize(worms.size());
		unsigned int p(0);
		for (unsigned int i = 0; i < groups.size(); i++){
			for (unsigned int g = 0; g < groups[i].size(); g++){
				for (unsigned int w = 0; w < groups[i][g].size(); w++){
					if (output_worm[i][g][w] == 0)
						continue;
					groups[i][g][w]->generate_training_set_visualization(images[p].combined_image);
					p++;
				}
			}
		}
	}
	void generate_image_with_box(const ns_object_hand_annotation_data & data, const ns_image_standard & combined_image, ns_image_standard & out){
		ns_image_properties prop(combined_image.properties());
		const ns_image_standard & box = ns_worm_training_set_image::check_box(data.dominant_label());
		prop.width += box.properties().width;
		if(prop.height < box.properties().height)
			prop.height = box.properties().height;
		out.prepare_to_recieve_image(prop);
		for (unsigned int y = 0; y < prop.height; y++){
			for (unsigned int x = 0; x < prop.components*prop.width; x++)
				out[y][x] = 0;
		}
		for (unsigned int y = 0; y < box.properties().height; y++){
			for (unsigned int x = 0; x < prop.components*box.properties().width; x++)
				out[y][x] = box[y][x];
		}
		for (unsigned int y = 0; y < combined_image.properties().height; y++){
			for (unsigned int x = 0; x < prop.components*combined_image.properties().width; x++)
				out[y][x+prop.components*box.properties().width] = combined_image[y][x];
		}
	}

	void generate_check_box_composits(bool run_worm_detection){
		
		//generate check box images
		images_with_boxes.resize(images.size());
		if (run_worm_detection && (images.size() != worms.size()))
			throw ns_ex("generate_check_box_composits::worms[] not specified");
		for (unsigned int i = 0; i < images.size(); i++){
			if (run_worm_detection)
				image_info[i].hand_annotation_data.identified_as_a_worm_by_machine = worms[i]->is_a_worm();
			generate_image_with_box(image_info[i].hand_annotation_data,images[i].combined_image,images_with_boxes[i]);
		}
	}

	void generate_collage(ns_image_standard & im){

		ns_image_standard collage;
		std::vector<ns_packed_collage_position> worm_positions = ns_make_packed_collage(images_with_boxes,im,0,1200);
		if (worm_positions.size() != images_with_boxes.size())
			throw ns_ex("ns_worm_training_set::to_image()::incorrect number of worm locations returned by make_packed_collage()") <<(unsigned long)worms.size() << "->" << (unsigned long) worm_positions.size();
		
		for (unsigned int i = 0; i < worm_positions.size(); i++){
			image_info[i].position = worm_positions[i].pos;
			image_info[i].dimensions = worm_positions[i].size;
		}

		im.set_description(this->metadata_to_string());
	}
	
	void add_region_metadata(const unsigned long region_id, const unsigned long capture_time, const ns_whole_image_statistic_specification & spec){
		ns_whole_image_statistic_specification_key key;
		key.region_id = region_id;
		key.capture_time = capture_time;
		whole_image_stats[key] = spec;
	}
	typedef std::map<ns_whole_image_statistic_specification_key,ns_whole_image_statistic_specification> ns_whole_image_statistics_list;
	ns_whole_image_statistics_list whole_image_stats;

	std::vector<ns_training_set_visualization_image_info> image_info;
	std::vector<ns_detected_worm_info *> worms;
	std::vector<ns_worm_context_image> images;
	std::vector<ns_image_standard> images_with_boxes;
	void from_string(const std::string & s, const bool allow_malformed_metadata=false){
		// ofstream foo("c:\\str.txt");
		// foo << s;
		// foo.close();
		ns_xml_simple_object_reader reader;
		reader.from_string(s);
		ns_whole_image_statistic_specification all_object_spec;
		//group_sizes.reserve(reader.objects.size()/2);
		image_info.reserve(reader.objects.size());
		ns_whole_image_statistic_specification all_object_image_spec;
		bool all_object_image_spec_found(false);
		for (unsigned int i = 0; i < reader.objects.size(); i++){
			if (reader.objects[i].name == "image"){
				ns_whole_image_statistic_specification * destination(0);
				if (!reader.objects[i].tag_specified("r_id")){
					if (all_object_image_spec_found)
						throw ns_ex("Multiple all image whole image statistics found!");
					all_object_image_spec_found = true;
					destination = &all_object_image_spec;
				}
				else{
					ns_whole_image_statistic_specification_key key;
					key.region_id = ns_atoi64(reader.objects[i].tag("r_id").c_str());
					key.capture_time = atol(reader.objects[i].tag("t").c_str());
					ns_whole_image_statistics_list::iterator p(whole_image_stats.find(key));
					if (p!=whole_image_stats.end())
						throw ns_ex("Multiple specifications found for region/time ") << key.region_id << "/"<<key.capture_time;
					destination = &whole_image_stats[key];
				}
					
				if (!reader.objects[i].assign_if_present("min_abs_i",destination->whole_image_region_stats.absolute_intensity_stats.minimum_intensity))	
					destination->whole_image_region_stats.absolute_intensity_stats.minimum_intensity= (unsigned char)atoi(reader.objects[i].tag("i1").c_str());

				if (!reader.objects[i].assign_if_present("max_abs_i",destination->whole_image_region_stats.absolute_intensity_stats.maximum_intensity))	
					destination->whole_image_region_stats.absolute_intensity_stats.maximum_intensity = (unsigned char)atoi(reader.objects[i].tag("i2").c_str());

				if (!reader.objects[i].assign_if_present("av_abs_i",destination->whole_image_region_stats.absolute_intensity_stats.average_intensity))	
					destination->whole_image_region_stats.absolute_intensity_stats.average_intensity = (unsigned char)atoi(reader.objects[i].tag("i3").c_str());

				if (!reader.objects[i].assign_if_present("min_rel_i",destination->whole_image_region_stats.relative_intensity_stats.minimum_intensity))	
					destination->whole_image_region_stats.relative_intensity_stats.minimum_intensity = (unsigned char)atoi(reader.objects[i].tag("i4").c_str());

				if (!reader.objects[i].assign_if_present("max_rel_i",destination->whole_image_region_stats.relative_intensity_stats.average_intensity))	
					destination->whole_image_region_stats.relative_intensity_stats.average_intensity = (unsigned char)atoi(reader.objects[i].tag("i5").c_str());

				if (!reader.objects[i].assign_if_present("av_rel_i", destination->whole_image_region_stats.relative_intensity_stats.maximum_intensity))	
					destination->whole_image_region_stats.relative_intensity_stats.maximum_intensity = (unsigned char)atoi(reader.objects[i].tag("i6").c_str());

				
				if (!reader.objects[i].assign_if_present("min_r_abs_i",destination->worm_region_specific_region_stats.absolute_intensity_stats.minimum_intensity))	
					destination->worm_region_specific_region_stats.absolute_intensity_stats.minimum_intensity= (unsigned char)atoi(reader.objects[i].tag("w1").c_str());

				if (!reader.objects[i].assign_if_present("max_r_abs_i",destination->worm_region_specific_region_stats.absolute_intensity_stats.maximum_intensity))	
					destination->worm_region_specific_region_stats.absolute_intensity_stats.maximum_intensity = (unsigned char)atoi(reader.objects[i].tag("w2").c_str());

				if (!reader.objects[i].assign_if_present("av_r_abs_i",destination->worm_region_specific_region_stats.absolute_intensity_stats.average_intensity))	
					destination->worm_region_specific_region_stats.absolute_intensity_stats.average_intensity = (unsigned char)atoi(reader.objects[i].tag("w3").c_str());

				if (!reader.objects[i].assign_if_present("min_r_rel_i",destination->worm_region_specific_region_stats.relative_intensity_stats.minimum_intensity))	
					destination->worm_region_specific_region_stats.relative_intensity_stats.minimum_intensity = (unsigned char)atoi(reader.objects[i].tag("w4").c_str());

				if (!reader.objects[i].assign_if_present("max_r_rel_i",destination->worm_region_specific_region_stats.relative_intensity_stats.average_intensity))	
					destination->worm_region_specific_region_stats.relative_intensity_stats.average_intensity = (unsigned char)atoi(reader.objects[i].tag("w5").c_str());

				if (!reader.objects[i].assign_if_present("av_r_rel_i",destination->worm_region_specific_region_stats.relative_intensity_stats.maximum_intensity))	
					destination->worm_region_specific_region_stats.relative_intensity_stats.maximum_intensity = (unsigned char)atoi(reader.objects[i].tag("w6").c_str());
					
			/*
				destination->whole_image_region_stats.absolute_intensity_stats.minimum_intensity= (unsigned char)atoi(reader.objects[i].tag("min_abs_i").c_str());
				destination->whole_image_region_stats.absolute_intensity_stats.maximum_intensity = (unsigned char)atoi(reader.objects[i].tag("max_abs_i").c_str());
				destination->whole_image_region_stats.absolute_intensity_stats.average_intensity = (unsigned char)atoi(reader.objects[i].tag("av_abs_i").c_str());
				destination->whole_image_region_stats.relative_intensity_stats.minimum_intensity = (unsigned char)atoi(reader.objects[i].tag("min_rel_i").c_str());
				destination->whole_image_region_stats.relative_intensity_stats.average_intensity = (unsigned char)atoi(reader.objects[i].tag("max_rel_i").c_str());
				destination->whole_image_region_stats.relative_intensity_stats.maximum_intensity = (unsigned char)atoi(reader.objects[i].tag("av_rel_i").c_str());
				
				destination->worm_region_specific_region_stats.absolute_intensity_stats.minimum_intensity= (unsigned char)atoi(reader.objects[i].tag("min_r_abs_i").c_str());
				destination->worm_region_specific_region_stats.absolute_intensity_stats.maximum_intensity = (unsigned char)atoi(reader.objects[i].tag("max_r_abs_i").c_str());
				destination->worm_region_specific_region_stats.absolute_intensity_stats.average_intensity = (unsigned char)atoi(reader.objects[i].tag("av_r_abs_i").c_str());
				destination->worm_region_specific_region_stats.relative_intensity_stats.minimum_intensity = (unsigned char)atoi(reader.objects[i].tag("min_r_rel_i").c_str());
				destination->worm_region_specific_region_stats.relative_intensity_stats.average_intensity = (unsigned char)atoi(reader.objects[i].tag("max_r_rel_i").c_str());
				destination->worm_region_specific_region_stats.relative_intensity_stats.maximum_intensity = (unsigned char)atoi(reader.objects[i].tag("av_r_rel_i").c_str());
				*/
			}
			
					
			if (reader.objects[i].name == "w"){
				unsigned long s = (unsigned long)image_info.size();
				image_info.resize(s+1);
				image_info[s].position.x = atol(reader.objects[i].tag("px").c_str());
				image_info[s].position.y = atol(reader.objects[i].tag("py").c_str());
				image_info[s].dimensions.x = atol(reader.objects[i].tag("dx").c_str());
				image_info[s].dimensions.y = atol(reader.objects[i].tag("dy").c_str());

				image_info[s].multiple_worm_cluster_group_id = ns_atoi64(reader.objects[i].tag("g").c_str());
				image_info[s].multiple_worm_cluster_solution_id = ns_atoi64(reader.objects[i].tag("s").c_str());
				image_info[s].original_region_position_in_source_image.x= atol(reader.objects[i].tag("ox").c_str());
				image_info[s].original_region_position_in_source_image.y = atol(reader.objects[i].tag("oy").c_str());
				try{
					image_info[s].original_context_position_in_source_image.x= atol(reader.objects[i].tag("cx").c_str());
					image_info[s].original_context_position_in_source_image.y = atol(reader.objects[i].tag("cy").c_str());
				}
				catch(ns_ex & ex){
					cerr << ex.text() << "\n";
				}
				
				image_info[s].original_bitmap_size.x = atol(reader.objects[i].tag("bx").c_str());
				image_info[s].original_bitmap_size.y = atol(reader.objects[i].tag("by").c_str());
				image_info[s].original_context_image_size.x = atol(reader.objects[i].tag("xx").c_str());
				image_info[s].original_context_image_size.y = atol(reader.objects[i].tag("xy").c_str());

				/*if (allow_malformed_metadata){
					
					
				//	image_info[s].original_bitmap_size.x = image_info[s].dimensions.x - 80;
				//	image_info[s].original_bitmap_size.y = image_info[s].dimensions.y - 80;

					if(reader.objects[i].tag_specified("fx"))
						image_info[s].bitmap_offset_in_context_image.x = atol(reader.objects[i].tag("fx").c_str());
					else image_info[s].bitmap_offset_in_context_image.x  = 40;

					if(reader.objects[i].tag_specified("fy"))
						image_info[s].bitmap_offset_in_context_image.y = atol(reader.objects[i].tag("fy").c_str());
					else image_info[s].bitmap_offset_in_context_image.y = 40;
				}
				else{
					
				
					image_info[s].bitmap_offset_in_context_image.x = atol(reader.objects[i].tag("fx").c_str());
					image_info[s].bitmap_offset_in_context_image.y = atol(reader.objects[i].tag("fy").c_str());
				}*/
			
				if(reader.objects[i].tag_specified("r"))
					image_info[s].region_info_id = ns_atoi64(reader.objects[i].tag("r").c_str());
				if(reader.objects[i].tag_specified("t"))
					image_info[s].time = atol(reader.objects[i].tag("t").c_str());
				if (reader.objects[i].tag_specified("m"))
					image_info[s].hand_annotation_data.identified_as_a_mangled_worm = reader.objects[i].tag("m")=="1";		
				if (reader.objects[i].tag_specified("h"))
					image_info[s].hand_annotation_data.identified_as_a_worm_by_human = reader.objects[i].tag("h")=="1";	
				if (reader.objects[i].tag_specified("i"))
					image_info[s].hand_annotation_data.identified_as_a_worm_by_machine = reader.objects[i].tag("i")=="1";	
				if (reader.objects[i].tag_specified("d"))
					image_info[s].hand_annotation_data.identified_as_misdisambiguated_multiple_worms = reader.objects[i].tag("d")=="1";	
				if (reader.objects[i].tag_specified("l"))
					image_info[s].hand_annotation_data.identified_as_small_larvae = reader.objects[i].tag("l")=="1";	
			}
		
		}
		if (all_object_image_spec_found){
			//if we've found one specification for the entire file, copy it over to the map.
			if (image_info.size() == 0)
				throw ns_ex("No objects loaded in file!");
			ns_whole_image_statistic_specification_key key;
			key.region_id = image_info[0].region_info_id;
			key.capture_time= image_info[0].time;
			for (unsigned int i = 1; i < image_info.size(); i++){
				if (image_info[i].time != key.capture_time || key.region_id != image_info[i].region_info_id)
					throw ns_ex("All objects image spec specified in collage containing worms from multiple sources!");
			}
			whole_image_stats[key] = all_object_image_spec;
		}
		else{
			//make sure all the whole image information is specified
			for (unsigned int i = 0; i < image_info.size(); i++){
				ns_whole_image_statistic_specification_key key;
				key.region_id = image_info[i].region_info_id;
				key.capture_time = image_info[i].time;
				ns_whole_image_statistics_list::iterator p(whole_image_stats.find(key));
				if (p==whole_image_stats.end())
					throw ns_ex("The whole image information for region/time ") << key.region_id << "/"<<key.capture_time << " is missing.";
			}
		}
	}

};

void ns_worm_training_set_image::generate(const ns_image_worm_detection_results & results, ns_image_standard & out){
	ns_training_set_visualization_metadata_manager manager;
	manager.load_from_mutually_exclusive_worm_groups(results.region_info_id,results.capture_time,results.mutually_exclusive_worm_groups);
	manager.generate_check_box_composits(true);
	manager.generate_collage(out);
}


void ns_worm_training_set_image::generate(const ns_death_time_annotation_compiler_region & result, ns_image_standard & out,ns_sql & sql){
	ns_training_set_visualization_metadata_manager manager;
	manager.load_from_death_time_annotation_set(result,sql);
	manager.generate_check_box_composits(false);
	manager.generate_collage(out);
}

void ns_transfer_annotations(const ns_image_standard & source_collage, const ns_image_standard & destination_collage, ns_image_standard & output_image){

	ns_annotated_training_set annotation_set;
	ns_worm_training_set_image::decode(source_collage,annotation_set,true);

	ns_training_set_visualization_metadata_manager images;
	images.from_collage(destination_collage);
	for (unsigned int i = 0; i < images.image_info.size(); ++i){
		if (images.image_info[i].original_bitmap_size.x*images.image_info[i].original_bitmap_size.y == 0)
			continue;
		double min_d(100000000);
		int min_j(0);
		for (unsigned int j = 0; j < annotation_set.objects.size(); ++j){	
			double d(((images.image_info[i].original_region_position_in_source_image + images.image_info[i].original_bitmap_size/2) - 
						(annotation_set.objects[j].object.region_position_in_source_image + annotation_set.objects[j].object.region_size/2)).mag());
	//		cerr << images.image_info[i].original_bitmap_position_in_source_image << ": " 
	//			<< annotation_set.objects[j].object.position_in_source_image << ":\t" 
	//			<< d << "\n";
			if (d < min_d){
				min_j = j;
				min_d = d;
			}
		}
		if (min_d > (images.image_info[i].original_bitmap_size.squared()*3)/4)
			throw ns_ex("Could not find match in correct metadata collage");
	/*	ns_image_standard im;
		vector<ns_image_standard *> c;
		c.push_back(&images.images[i].combined_image);
		ns_image_standard im2;
		annotation_set.objects[min_j].object.generate_training_set_visualization(im2);
		c.push_back(&im2);
		ns_make_collage(c,im,1024);
		ns_save_image(string("c:\\match_") + ns_to_string(i) + ".tif",im);*/
		images.image_info[i].hand_annotation_data = annotation_set.objects[min_j].hand_annotation_data;
	}
	images.generate_check_box_composits(false);
	images.generate_collage(output_image);
}

ns_annotated_training_set::~ns_annotated_training_set(){
	objects.clear();
}

template<class ns_component>
float ns_calculate_overlap(const ns_image_whole<ns_component> & a, const ns_image_whole<ns_component> & b){
	if (a.properties() != b.properties())
		throw ns_ex("ns_calculate_overlap()::Images do not have the same width and height") << 
		a.properties().width << "x" << a.properties().height << " vs " << b.properties().width << "x" << b.properties().height;

	unsigned long overlap(0);
	unsigned long area[2] = {0,0};
	for (unsigned int y = 0; y < a.properties().height; y++)
		for (unsigned int x = 0; x < a.properties().width; x++){
			overlap += a[y][x]&&b[y][x];
			area[0]+=a[y][x];
			area[1]+=b[y][x];
		}
	return (float)overlap/(float)(area[0]+area[1]);
}

#include "ns_image_easy_io.h"

void ns_worm_training_set_image::decode(const ns_image_standard & im,ns_annotated_training_set & t, const bool allow_malformed_metadata){

	//extract individual training images from collage
	ns_training_set_visualization_metadata_manager manager;
	manager.from_collage(im, allow_malformed_metadata);
	unsigned long checked(0);
	unsigned long unchecked(0);
	for (unsigned int i = 0; i < manager.image_info.size(); i++){
		if (manager.image_info[i].hand_annotation_data.identified_as_a_worm_by_human)
			checked++;
		else unchecked++;
	}
//	cerr << "checked: " << checked << ", unchecked: " << unchecked << "\n";
	t.worms.reserve(manager.images.size()/2);
	t.non_worms.reserve(manager.images.size());
	t.objects.resize(manager.images.size());
	
	//extract worm grayscale and bitmap information from the training images
	for (unsigned int i = 0; i < t.objects.size(); i++){
		if (allow_malformed_metadata && 
			(manager.image_info[i].original_bitmap_size.x == 0 || 
			manager.image_info[i].original_bitmap_size.y == 0)){
			cerr << "Problem: Object size: " << manager.image_info[i].original_bitmap_size << "\n";
			continue;
		}
	//	if (manager.image_info[i].original_bitmap_position_in_source_image == ns_vector_2i(2385,241)){
	//		ns_save_image("c:\\attmept_com.tif",manager.images[i].combined_image);
	//		cerr << "ok!";
	//	}
		if (manager.image_info[i].original_bitmap_size.x > manager.images[i].combined_image.properties().width || 
			manager.image_info[i].original_bitmap_size.y > manager.images[i].combined_image.properties().height){
			manager.image_info[i].original_bitmap_size =  ns_vector_2i(
															manager.images[i].combined_image.properties().width-80,
															manager.images[i].combined_image.properties().height-80);
		}
		t.objects[i].object.from_training_set_visualization(manager.images[i],
													manager.image_info[i].original_bitmap_size,
													manager.image_info[i].bitmap_offset_in_context_image(),manager.image_info[i].original_context_image_size);
		manager.images[i].deallocate_images();
		
		/*if (manager.image_info[i].original_bitmap_position_in_source_image == ns_vector_2i(2385,241)){
			//cerr <<"ok!";
			ns_save_image("c:\\attmept_ag.tif",t.objects[i].object.absolute_grayscale());
			ns_save_image("c:\\attmept_rg.tif",t.objects[i].object.relative_grayscale());
			ns_save_image("c:\\attmept_bmp.tif",t.objects[i].object.bitmap());
		}*/
		t.objects[i].hand_annotation_data = manager.image_info[i].hand_annotation_data;
		t.objects[i].region_info_id = manager.image_info[i].region_info_id;
		t.objects[i].capture_time = manager.image_info[i].time;
		t.objects[i].object.region_position_in_source_image = manager.image_info[i].original_region_position_in_source_image;
		t.objects[i].object.context_position_in_source_image = manager.image_info[i].original_context_position_in_source_image;
		t.objects[i].object.region_size = manager.image_info[i].original_bitmap_size;
		t.objects[i].object.context_image_size = manager.image_info[i].original_context_image_size;
		ns_whole_image_statistic_specification_key key;
		key.region_id = manager.image_info[i].region_info_id;
		key.capture_time = manager.image_info[i].time;
		ns_training_set_visualization_metadata_manager::ns_whole_image_statistics_list::iterator p = manager.whole_image_stats.find(key);
		if (p == manager.whole_image_stats.end())
			throw ns_ex("Could not find whole image stats for region/time") << key.region_id << "/" << key.capture_time;
		t.objects[i].whole_image_stats = p->second;
		
	}

	//build spines from bitmaps;
	ns_detected_object_manager object_manager;
	object_manager.objects.resize(t.objects.size(),0);
	for (unsigned int i = 0; i < t.objects.size(); i++){
		object_manager.objects[i] = new ns_detected_object();
		//we use the multiple worm bitmap
		t.objects[i].object.bitmap_of_worm_cluster().pump(object_manager.objects[i]->bitmap(),1024);
		//cerr << "Worm cluster has a size of " << t.objects[i].bitmap_of_worm_cluster().properties().width << "x" << t.objects[i].bitmap_of_worm_cluster().properties().height << "\n";
		object_manager.objects[i]->offset_in_source_image = t.objects[i].object.region_position_in_source_image;
	//	object_manager.objects[i]->offset_in_bitmap = t.objects[i].object.get_bitmap_offset_in_context_image();
		object_manager.objects[i]->size = t.objects[i].object.region_size;
		
	}
	object_manager.convert_bitmaps_into_node_graphs(im.properties().resolution,"");
	object_manager.calculate_segment_topologies_from_node_graphs(true);
	//unsigned int number_of_worms = ns_count_number_of_worms_in_detected_object_group(object_manager.objects);

	//reconstruct mutually-exclusive solution hierarchy
//	std::vector<std::vector<std::vector<ns_training_set_visualization_image_info * > > > metadata_mappings;
	std::vector<std::vector<std::vector<ns_detected_object * > > > mutually_exclusive_objects;
	t.mutually_exclusive_groups.reserve(manager.image_info.size());
	for (unsigned int i = 0; i < manager.image_info.size(); i++){
		if (manager.image_info[i].multiple_worm_cluster_group_id >= mutually_exclusive_objects.size()){

			mutually_exclusive_objects.resize (manager.image_info[i].multiple_worm_cluster_group_id+1);
			t.mutually_exclusive_groups.resize(manager.image_info[i].multiple_worm_cluster_group_id+1);
	//		metadata_mappings.resize		  (manager.image_info[i].multiple_worm_cluster_group_id+1);
		}
		if (manager.image_info[i].multiple_worm_cluster_solution_id >= mutually_exclusive_objects[manager.image_info[i].multiple_worm_cluster_group_id].size()){
			mutually_exclusive_objects [manager.image_info[i].multiple_worm_cluster_group_id].resize(manager.image_info[i].multiple_worm_cluster_solution_id +1);
			t.mutually_exclusive_groups[manager.image_info[i].multiple_worm_cluster_group_id].resize(manager.image_info[i].multiple_worm_cluster_solution_id +1);
//			metadata_mappings		   [manager.image_info[i].multiple_worm_cluster_group_id].resize(manager.image_info[i].multiple_worm_cluster_solution_id +1);
		}
		mutually_exclusive_objects [manager.image_info[i].multiple_worm_cluster_group_id][manager.image_info[i].multiple_worm_cluster_solution_id].push_back(object_manager.objects[i]);
		t.mutually_exclusive_groups[manager.image_info[i].multiple_worm_cluster_group_id][manager.image_info[i].multiple_worm_cluster_solution_id].push_back(&t.objects[i]);
	//	metadata_mappings		   [manager.image_info[i].multiple_worm_cluster_group_id][manager.image_info[i].multiple_worm_cluster_solution_id].push_back(&manager.image_info[i]);
	}
	
	/*	ns_save_image("c:\\not_found_ag.tif",t.mutually_exclusive_groups[3][0][0]->object.absolute_grayscale());
		ns_save_image("c:\\not_found_rg.tif",t.mutually_exclusive_groups[3][0][0]->object.relative_grayscale());
		ns_save_image("c:\\not_found_bmp.tif",t.mutually_exclusive_groups[3][0][0]->object.bitmap());*/
	t.objects.reserve(manager.image_info.size());
	//now we go through each group and recalculate the solutions for each.
	
	/*for (unsigned int g = 0; g < mutually_exclusive_objects.size(); g++){
		if (mutually_exclusive_objects[g].size() == 0)
			continue;
		ns_save_image(string("c:\\dbg_") + ns_to_string(g) + ".tif",mutually_exclusive_objects[g][0][0]->bitmap());
	}*/
	for (unsigned int g = 0; g < mutually_exclusive_objects.size(); g++){
		if (mutually_exclusive_objects[g].size() == 0)
			continue;

		ns_vector_2i group_object_size(mutually_exclusive_objects[g][0][0]->size);
		for (unsigned int j = 0; j < mutually_exclusive_objects[g].size(); j++){
			for (unsigned int k = 1; k < mutually_exclusive_objects[g][j].size(); k++){
				if (!(mutually_exclusive_objects[g][j][k]->size == group_object_size))
					throw ns_ex("Group collation error: objects are different sizes. ");
			}
		}
		//each member of the group originate from the same multiple-worm disambiguation.
		//Thus, we only need to re-solve this once and can use that solution for all group members.
		std::vector<ns_detected_object *> objs(1,mutually_exclusive_objects[g][0][0]);
		std::vector<ns_detected_worm_info> solutions(ns_count_number_of_worms_in_detected_object_group(objs));
		std::vector<std::vector<ns_detected_worm_info *> > tmp;
		ns_image_bitmap *bmp(new ns_image_bitmap),*ebmp(new ns_image_bitmap);
		try{
			mutually_exclusive_objects[g][0][0]->bitmap().pump(*bmp,1024);
			mutually_exclusive_objects[g][0][0]->edge_bitmap().pump(*ebmp,1024);
			
			unsigned int worms_found = ns_detected_worm_info::from_segment_cluster_solution(*mutually_exclusive_objects[g][0][0],
				solutions, 0, tmp ,t.mutually_exclusive_groups[g][0][0]->object.relative_grayscale(),t.mutually_exclusive_groups[g][0][0]->object.absolute_grayscale(),
					ns_detected_worm_info::ns_individual_worm_source_grayscale_images_provided,ns_detected_worm_info::ns_vis_none);
			for (unsigned int i = 0; i < solutions.size(); i++)
				solutions[i].whole_image_stats = t.mutually_exclusive_groups[g][0][0]->whole_image_stats;
			
			
			mutually_exclusive_objects[g][0][0]->accept_bitmap(bmp);
			bmp = 0;
			mutually_exclusive_objects[g][0][0]->accept_edge_bitmap(ebmp);
			ebmp = 0;
		}
		catch(...){
			ns_safe_delete(bmp);
			ns_safe_delete(ebmp);
			throw;
		}
		if (solutions.size() == 0){
		/*	ns_save_image("c:\\not_found_ag.tif",t.mutually_exclusive_groups[g][0][0]->object.absolute_grayscale());
			ns_save_image("c:\\not_found_rg.tif",t.mutually_exclusive_groups[g][0][0]->object.relative_grayscale());
			ns_save_image("c:\\not_found_bmp.tif",t.mutually_exclusive_groups[g][0][0]->object.bitmap());*/
			throw ns_ex("ns_worm_training_set_image::decode()::Could not find worm in imported image.  Position: ") <<  mutually_exclusive_objects[g][0][0]->offset_in_source_image.x << "x" <<  mutually_exclusive_objects[g][0][0]->offset_in_source_image.y;

		}
		//std::vector<char> solution_used(solutions.size(),0);
		for (unsigned int s = 0; s < t.mutually_exclusive_groups[g].size(); s++){
			for (unsigned int i = 0; i < t.mutually_exclusive_groups[g][s].size(); i++){
			
				unsigned long chosen_worm;
				if (solutions.size() == 1)
					chosen_worm = 0;
				else{
					chosen_worm = -1;
					//look for the detected worm with the most overlap with the one found in the training set image
					float max_overlap(0);
					unsigned long max_overlap_id(-1);
					for (unsigned int j = 0; j < solutions.size(); j++){
					

						float overlap = ns_calculate_overlap(mutually_exclusive_objects[g][s][i]->bitmap(),solutions[j].bitmap());
						if (overlap > max_overlap){
							max_overlap = overlap;
							max_overlap_id = j;
						}
					}
					if (max_overlap_id != -1){
					//	cerr << "Max Overlap = " << max_overlap << "\n";
						chosen_worm = max_overlap_id;
				//		solution_used[max_overlap_id] = 1;
					}
					else std::cerr << "ns_worm_training_set_image::decode()::No detected worms had any overlap w. bitmap!";
				}
				//we now have the correct multiple worm disambiguation solution.  We copy it over, being careful
				//to replace the original context image that we had to remove earlier.
			//	ns_vector_2i pos_in_ctx = t.mutually_exclusive_groups[g][s][i]->object.get_region_offset_in_context_image();
			//	ns_vector_2i size_in_ctx = t.mutually_exclusive_groups[g][s][i]->object.get_region_offset_in_context_image();
				ns_worm_context_image ctx;
				
				t.mutually_exclusive_groups[g][s][i]->object.context_image().absolute_grayscale.pump(ctx.absolute_grayscale,1024);
				t.mutually_exclusive_groups[g][s][i]->object.context_image().relative_grayscale.pump(ctx.relative_grayscale,1024);
				t.mutually_exclusive_groups[g][s][i]->object.context_image().combined_image.pump(ctx.combined_image,1024);
				
				t.mutually_exclusive_groups[g][s][i]->object = solutions[chosen_worm];

				t.mutually_exclusive_groups[g][s][i]->object.region_position_in_source_image =  t.mutually_exclusive_groups[g][s][i]->object.region_position_in_source_image;
				t.mutually_exclusive_groups[g][s][i]->object.context_position_in_source_image =  t.mutually_exclusive_groups[g][s][i]->object.context_position_in_source_image;
				
				t.mutually_exclusive_groups[g][s][i]->object.region_size =  t.mutually_exclusive_groups[g][s][i]->object.region_size;
				t.mutually_exclusive_groups[g][s][i]->object.context_image_size =  t.mutually_exclusive_groups[g][s][i]->object.context_image_size;
				

				ctx.absolute_grayscale.pump(t.mutually_exclusive_groups[g][s][i]->object.context_image().absolute_grayscale,1024);
				ctx.relative_grayscale.pump(t.mutually_exclusive_groups[g][s][i]->object.context_image().relative_grayscale,1024);
				ctx.combined_image.pump(t.mutually_exclusive_groups[g][s][i]->object.context_image().combined_image,1024);
	
				ns_whole_image_statistic_specification_key key;	
				key.region_id = t.mutually_exclusive_groups[g][s][i]->region_info_id;
				key.capture_time = t.mutually_exclusive_groups[g][s][i]->capture_time;
				ns_training_set_visualization_metadata_manager::ns_whole_image_statistics_list::iterator p = manager.whole_image_stats.find(key);
				if (p == manager.whole_image_stats.end())
					throw ns_ex("Could not find whole image stats for region/time") << key.region_id << "/" << key.capture_time;

				t.mutually_exclusive_groups[g][s][i]->whole_image_stats = p->second;
			}
		}
	}
	
	//spread sticky group properties
	for (unsigned int g = 0; g < t.mutually_exclusive_groups.size(); g++){
		ns_object_hand_annotation_data data;
		data.identified_as_misdisambiguated_multiple_worms = false;
		for (unsigned int s = 0; s < t.mutually_exclusive_groups[g].size(); s++){
			for (unsigned int i = 0; i < t.mutually_exclusive_groups[g][s].size(); i++){
				if (t.mutually_exclusive_groups[g][s][i]->object.hand_annotations.identified_as_misdisambiguated_multiple_worms)
					data.identified_as_misdisambiguated_multiple_worms = true;
			}
		}
		if (data.identified_as_misdisambiguated_multiple_worms){
			for (unsigned int s = 0; s < t.mutually_exclusive_groups[g].size(); s++){
				for (unsigned int i = 0; i < t.mutually_exclusive_groups[g][s].size(); i++){
					t.mutually_exclusive_groups[g][s][i]->object.hand_annotations.identified_as_misdisambiguated_multiple_worms = true;
				}
			}
		}
	}

	//set up propper  bindings
	for (unsigned int g = 0; g < t.mutually_exclusive_groups.size(); g++){
		for (unsigned int s = 0; s < t.mutually_exclusive_groups[g].size(); s++){
			for (unsigned int i = 0; i < t.mutually_exclusive_groups[g][s].size(); i++){
				//censor discarded solutions to multiple worm groups
				if ((t.mutually_exclusive_groups[g].size() > 1 || t.mutually_exclusive_groups[g][s].size() > 1)
						&& !t.mutually_exclusive_groups[g][s][i]->hand_annotation_data.identified_as_a_worm_by_human)
					 t.censored_worms.push_back(t.mutually_exclusive_groups[g][s][i]);
				else if (t.mutually_exclusive_groups[g][s][i]->hand_annotation_data.identified_as_a_mangled_worm
						||t.mutually_exclusive_groups[g][s][i]->hand_annotation_data.identified_as_misdisambiguated_multiple_worms
						|| t.mutually_exclusive_groups[g][s][i]->hand_annotation_data.identified_as_small_larvae)
					t.censored_worms.push_back(t.mutually_exclusive_groups[g][s][i]);
				else if (t.mutually_exclusive_groups[g][s][i]->hand_annotation_data.identified_as_a_worm_by_human)
					 t.worms.push_back(t.mutually_exclusive_groups[g][s][i]);
				else t.non_worms.push_back(t.mutually_exclusive_groups[g][s][i]);
			}
		}
	}

}

void ns_training_set_visualization_metadata_manager::from_collage(const ns_image_standard & in, const bool allow_malformed_metadata){

	//cerr << "in size: " << in.properties().width << "," << in.properties().height << "," << (int)in.properties().components << "\n";
	const ns_image_properties prop(in.properties());
	//get number of objects

	from_string(prop.description,allow_malformed_metadata);
	if (image_info.size() == 0)
		throw ns_ex("Could not identify any annotated worms in image");
	
	images.resize(image_info.size());

	//cerr << "Input positions:\n";
	//for (unsigned int i = 0 ; i < worm_positions.size(); i++)
	//	cerr << worm_positions[i].pos << ":" <<worm_positions[i].size << "," << worm_positions[i].original_bitmap_position << "\n";
	
	//ns_image_standard vis;
	//in.pump(vis,1024);

	ns_image_properties check_box_properties(ns_worm_training_set_image::check_box(ns_object_hand_annotation_data::ns_non_worm).properties());
	for (unsigned int i = 0; i < image_info.size(); i++){
		ns_image_properties new_prop(prop);
		new_prop.height = image_info[i].dimensions.y;
		if (image_info[i].dimensions.x <= (long)check_box_properties.width)
			throw ns_ex("Could not extract a check box from an image with width") << image_info[i].dimensions.x;
		new_prop.width = image_info[i].dimensions.x -check_box_properties.width;
		images[i].combined_image.prepare_to_recieve_image(new_prop);

		//see if the image is checked
		unsigned long pixels_in_checked_area[3] = {0,0,0};
		for (unsigned int y = 3; y < check_box_properties.height-2; y++)
			for (unsigned int x = 3; x < check_box_properties.width-2; x++){
				ns_vector_2i p(image_info[i].position.x + x,image_info[i].position.y + y);  //number_of_info_lines offset because these do not contain image info
				
				pixels_in_checked_area[0]+=(in[p.y][3*p.x] >= 200)?1:0;
				pixels_in_checked_area[1]+=(in[p.y][3*p.x+1] >= 200)?1:0;
				pixels_in_checked_area[2]+=(in[p.y][3*p.x+2] >= 200)?1:0;
			
			}
	//		cerr << pixels_in_checked_area << "\n";
		image_info[i].hand_annotation_data.identified_as_a_worm_by_human =	//white
							(pixels_in_checked_area[0] > 75 && 
							pixels_in_checked_area[1] > 75 && 
							pixels_in_checked_area[2] > 75);
		image_info[i].hand_annotation_data.identified_as_a_mangled_worm =	//red
							(pixels_in_checked_area[1] > 75 && 
							pixels_in_checked_area[0] < pixels_in_checked_area[1]/2 &&
							pixels_in_checked_area[2] < pixels_in_checked_area[1]/2);
		image_info[i].hand_annotation_data.identified_as_misdisambiguated_multiple_worms =	//blue
							(pixels_in_checked_area[2] > 75 && 
							pixels_in_checked_area[0] < pixels_in_checked_area[2]/2 &&
							pixels_in_checked_area[1] < pixels_in_checked_area[2]/2);
		image_info[i].hand_annotation_data.identified_as_small_larvae =						//yellow
							(pixels_in_checked_area[1] > 75 && 
							pixels_in_checked_area[2] > 75 &&
							pixels_in_checked_area[0] < pixels_in_checked_area[1]/2);

	//	if (marked_as_worm)
		//cerr << worm_positions[i].pos << ":" <<worm_positions[i].size << ",\t" << worm_positions[i].original_bitmap_position << " is a worm\n";
		//see if the image is a duplicate
		bool marked_as_duplicate(true);
		for (unsigned int y = 4; marked_as_duplicate && y < 8; y++)
			for (unsigned int x = 4; x < 8; x++){
				ns_vector_2i p(new_prop.components*(image_info[i].position.x + x),image_info[i].position.y + y );  //number_of_info_lines offset because these do not contain image info
		
				if (!(in[p.y][p.x] == 255 && in[p.y][p.x+1] == 0 && in[p.y][p.x+2] == 0)){
					marked_as_duplicate = false;
					break;
				}
			}

		//copy image
		for (unsigned int y = 0; y < new_prop.height; y++)
			for (unsigned int x = 0; x < new_prop.components*new_prop.width; x++){
				ns_vector_2i p(new_prop.components*(check_box_properties.width+image_info[i].position.x) + x,image_info[i].position.y + y);  //number_of_info_lines offset because these do not contain image info
				if ((unsigned long)p.x >= in.properties().components*in.properties().width)
					throw ns_ex() << "i" << i << "x:" <<p.x <<">" << in.properties().components*in.properties().width << "\n";
				if ((unsigned long)p.y >= in.properties().height)
					throw ns_ex() << "i" << i << "y:"<< p.y <<">" << in.properties().height << "\n";
				images[i].combined_image[y][x] = in[p.y][p.x];
			}

		//if (marked_as_duplicate){
			//cerr << "Marked as duplicate\n";
			//continue;
		//}

		//if (1 || marked_as_worm){
		//	for (unsigned int y = 0; y < 20; y++)
		//		for (unsigned int x = 0; x < 20; x++)
		//			vis[worm_positions[i].pos.y+y][3*(worm_positions[i].pos.x+x+i/2)+1] = 255;
		//}
	}
	//ns_save_image("c:\\tt\\vis.tif",vis);
	
}

std::vector<ns_image_standard> ns_worm_training_set_image::check_boxes;	