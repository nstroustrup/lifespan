#include "ns_mask_management.h"
#include "ns_sql.h"
#include "ns_processing_job_push_scheduler.h"
#include "ns_progress_reporter.h"


#include <iostream>
using namespace std;

void ns_bulk_experiment_mask_manager::process_mask_file(ns_image_standard & mask_vis){
	if (collage_info_manager.collage_info.size() == 0)
		throw ns_ex("No mask file loaded.");
	std::vector<unsigned int> starting_positions(collage_info_manager.collage_info.size());
//	ns_image_buffered_random_access_input_image<ns_8_bit,ns_image_storage_source<ns_8_bit> > decoded_image(1024*1024);
	ns_image_storage_source_handle<ns_8_bit> in(image_server.image_storage.request_from_local_cache(scratch_filenames[0]));
//	decoded_image.assign_buffer_source(in.input_stream());


	//determine the size of the merged visualization
	ns_image_properties prop(in.input_stream().properties());
	prop.width = 0;
	prop.height = 0;
	std::vector<ns_image_properties> output_sizes(collage_info_manager.collage_info.size());

	for (unsigned int i = 0; i < collage_info_manager.collage_info.size(); i++){
		output_sizes[i] = in.input_stream().properties();
		output_sizes[i].width = collage_info_manager.collage_info[i].dimentions.x;
		output_sizes[i].height =collage_info_manager.collage_info[i].dimentions.y;
		starting_positions[i] = prop.width;
		prop.width+=output_sizes[i].width;
		if (prop.height < output_sizes[i].height)
			prop.height = output_sizes[i].height;
	}
	unsigned int resize_factor = prop.width/1024;
	//cerr << "Resize factor : " << resize_factor << "\n";
	//if we need to resize, recalculate all the sizes
	if (resize_factor > 1){
		prop.width = 0;
		prop.height = 0;
		prop.resolution/=resize_factor;
		for (unsigned int i = 0; i < collage_info_manager.collage_info.size(); i++){
			output_sizes[i] = in.input_stream().properties();
			output_sizes[i].width  = collage_info_manager.collage_info[i].dimentions.x/resize_factor;
			output_sizes[i].height =collage_info_manager.collage_info[i].dimentions.y/resize_factor;
			output_sizes[i].resolution/=resize_factor;
//			cerr << "o[" << i << "]:" << output_sizes[i].width << "," << output_sizes[i].height << "\n";
			starting_positions[i] = prop.width;
			prop.width+=output_sizes[i].width;
			if (prop.height < output_sizes[i].height)
				prop.height = output_sizes[i].height;
		}
	}
	//cerr << "vis" << prop.width << "," << prop.height << "\n";

	prop.components = 3;
	mask_vis.prepare_to_recieve_image(prop);
	ns_image_standard tmp,tmp2;
	
	vector<string> problems;
	for (unsigned int i = 0;;){

//		cerr << "Processing mask " << i << "...";
	//	cerr << "Registering vis output\n";
		mask_analyzer.register_visualization_output(tmp);
	//	cerr << "Analyzing mask.\n";
		in.input_stream().pump(mask_analyzer,4096);
		cerr << "Sample " << i << " has " << mask_analyzer.mask_info().number_of_regions() << " regions.\n";
		if ( mask_analyzer.mask_info().number_of_regions() > 4)
			problems.push_back(string( "Sample ") + ns_to_string(i) + " has " + ns_to_string( mask_analyzer.mask_info().number_of_regions()) + " regions.");
		for (unsigned int j = 0; j < mask_analyzer.mask_info().size(); j++){
			if (mask_analyzer.mask_info()[j]->stats.pixel_count == 0) continue;
			cerr << "\t" << ns_vector_2i(mask_analyzer.mask_info()[j]->stats.x_min,mask_analyzer.mask_info()[j]->stats.y_min) << " -> ";
			cerr << "\t" << ns_vector_2i(mask_analyzer.mask_info()[j]->stats.x_max,mask_analyzer.mask_info()[j]->stats.y_max) << "\n";
		}
	//	decoded_image.clear();
		//cerr << "Resampling mask\n";
		tmp.resample(output_sizes[i],tmp2);
		for (unsigned int y = 0; y < tmp2.properties().height; y++)
			for (unsigned int x = 0; x < 3*tmp2.properties().width; x++)
			mask_vis[y][3*starting_positions[i]+x] = tmp2[y][x];
		for (unsigned int y = tmp2.properties().height; y < prop.height; y++)
			for (unsigned int x = 0; x < 3*tmp2.properties().width; x++)
				mask_vis[y][3*starting_positions[i]+x] = 0;
		tmp.clear();
		tmp2.clear();
		i++;
		if(i >= collage_info_manager.collage_info.size()) break;
		in = image_server.image_storage.request_from_local_cache(scratch_filenames[i]);
		//in.input_stream().pump(decoded_image,512);
	}
	for (unsigned int i = 0; i < problems.size(); i++){
		cerr << "Error: " << problems[i] << "\n";
	}
}

void ns_bulk_experiment_mask_manager::produce_mask_file(const ns_mask_type & mask_type, const unsigned int experiment_id, const std::string metadata_output_filename, ns_image_stream_file_sink<ns_8_bit> & reciever, ns_sql & sql, const unsigned long mask_time ) {

	bool output_region_label_mask(mask_type == ns_subregion_label_mask);
	ofstream metadata_output(metadata_output_filename.c_str());
	if (metadata_output.fail())
		throw ns_ex("Could not open metadata file ") << metadata_output_filename;

	const string subject_type = (output_region_label_mask) ? "region" : "sample";
	if (image_server.verbose_debug_output())
		cerr << "Loading sample info...\n";
	if (output_region_label_mask)
		 sql << "SELECT r.name,r.id,s.id,s.name FROM sample_region_image_info as r, capture_samples as s WHERE r.sample_id = s.id AND s.experiment_id=" << experiment_id << " AND r.censored = 0 AND s.censored=0 ORDER BY s.name ASC, r.name ASC";
	else sql << "SELECT name,id FROM capture_samples WHERE experiment_id=" << experiment_id << " AND censored = 0 ORDER BY name ASC";

	ns_sql_result subjects;
	sql.get_rows(subjects);
	if (subjects.size() == 0)
		throw ns_ex("The specified experiment contains no ") << subject_type;
	std::vector<ns_image_buffered_random_access_input_image<ns_8_bit, ns_image_storage_source<ns_8_bit> > > images(0, ns_image_buffered_random_access_input_image<ns_8_bit, ns_image_storage_source<ns_8_bit> >(1024));
	images.reserve(subjects.size());

	std::vector<ns_image_storage_source_handle<ns_8_bit> > sources;
	sources.reserve(subjects.size());
	bool start_verbosity = image_server.verbose_debug_output();
	if (start_verbosity)
		cerr << "Getting image ids...\n";
	image_server.image_storage.set_verbosity(ns_image_storage_handler::ns_verbose);

	int counter(0);
	for (ns_sql_result::iterator p = subjects.begin(); p != subjects.end();) {
		counter++;
		if (start_verbosity) cerr << counter << " ";
		try {
			//image_id==0  images have already been deleted.
			if(output_region_label_mask)
				sql << "SELECT image_id FROM sample_region_images WHERE region_info_id = " << (*p)[1] << " and problem=0 AND censored=0 AND image_id != 0";
			else 
				sql << "SELECT image_id FROM captured_images WHERE sample_id = " << (*p)[1] << " and currently_being_processed=0 AND problem=0 AND censored=0 AND image_id != 0";

			if (mask_time != 0)
				sql << " AND capture_time < " << mask_time;
			sql << " ORDER BY capture_time DESC";
			ns_sql_result im_id;
			sql.get_rows(im_id);
			if (im_id.size() < 2) {
				ns_ex ex("Could not find a sufficient number of captured images for ");
				ex << subject_type << " ";
				ex << (*p)[0] << "(" << (*p)[1] << ").";
				if (mask_time != 0)
					ex << "Note that the experiment has a mask time specified: " << ns_format_time_string_for_human(mask_time);
				cerr << " Miss! ";
				throw ex;
			}

			unsigned int s = (unsigned int)images.size();
			bool found_valid_image = false;
			//find an existing image to use as the mask
			for (unsigned int i = 1; i < im_id.size(); i++) { //skip first as it's often currently being written to disk
				ns_image_server_image im;
				im.load_from_db(atoi(im_id[i][0].c_str()), &sql);
				if (im.filename.find("TEMP") != im.filename.npos)
					continue;
				try {
					sources.resize(s + 1, image_server.image_storage.request_from_storage(im, &sql));
					found_valid_image = true;
					break;
				}
				catch (ns_ex & ex) {
					cerr << ex.text() << "\n";
					if (sources.size() == s + 1)
						sources.pop_back();
				}
			}
			if (!found_valid_image) {
				throw ns_ex("Could not find any good images for ") << subject_type << " " << (*p)[0] << "(" << (*p)[1] << ").";
			}
			if (start_verbosity) cerr << " hit. ";
			images.resize(s + 1, ns_image_buffered_random_access_input_image<ns_8_bit, ns_image_storage_source<ns_8_bit> >(100));
			images[s].assign_buffer_source(sources[s].input_stream());
			p++;
		}
		catch (ns_ex & ex) {
			cerr << "An error occurred in sample " << (*p)[0] << "(" << (*p)[1] << "): " << ex.text() << "\n";
			p = subjects.erase(p);
		}
		catch (std::exception & ex) {
			cerr << "An error occurred in sample " << (*p)[0] << "(" << (*p)[1] << "): " << ex.what() << "\n";
			p = subjects.erase(p);
		}
		catch (...) {
			cerr << "An unknown error occurred in sample " << (*p)[0] << "(" << (*p)[1] << ")\n";
			p = subjects.erase(p);
		}
	}
	if (subjects.size() == 0)
		throw ns_ex("No usable samples could be found in the experiment.");

	if (!start_verbosity)
		image_server.image_storage.set_verbosity(ns_image_storage_handler::ns_standard);

	//std::vector<ns_mask_collage_info> collage_info(images.size());
	collage_info_manager.collage_info.resize(images.size());
	if (images.size() == 0)
		throw ns_ex("No images loaded.");
	ns_image_properties prop(images[0].properties());
	prop.resolution /= resize_factor;
	prop.width = 0;
	prop.height = 0;
	unsigned long current_y(label_margin_buffer), current_x(0);
	ns_64_bit last_sample_id(0);
	if (start_verbosity) cerr << "Prepping images...\n";
	for (unsigned int i = 0; i < images.size(); i++) {

		if (start_verbosity) cerr << i << " ";
		if (images[i].properties().components != prop.components)
			throw ns_ex(subject_type) << subjects[i][0] << "(" << subjects[i][1] << ") has " << images[i].properties().components << " components, unlike other samples.";
		if (images[i].properties().resolution / resize_factor != prop.resolution)
			throw ns_ex(subject_type) << subjects[i][0] << "(" << subjects[i][1] << ") was taken at a resolution of " << images[i].properties().resolution << ",unlike other samples.";

		collage_info_manager.collage_info[i].dimentions.x = images[i].properties().width / resize_factor;
		collage_info_manager.collage_info[i].dimentions.y = images[i].properties().height / resize_factor;
		collage_info_manager.collage_info[i].experiment_id = experiment_id;
		collage_info_manager.collage_info[i].position.x = current_x;
		collage_info_manager.collage_info[i].position.y = current_y;
		if (output_region_label_mask) {
			collage_info_manager.collage_info[i].region_id = ns_atoi64(subjects[i][0].c_str());
			collage_info_manager.collage_info[i].sample_name = subjects[i][2];
			collage_info_manager.collage_info[i].sample_id = ns_atoi64(subjects[i][3].c_str());
		}
		else {
			collage_info_manager.collage_info[i].sample_name = subjects[i][0];
			collage_info_manager.collage_info[i].sample_id = ns_atoi64(subjects[i][1].c_str());
		}
		const unsigned long current_top(collage_info_manager.collage_info[i].dimentions.y + collage_info_manager.collage_info[i].position.y+image_margin);
		if (prop.height < current_top)
			prop.height = current_top;
		const unsigned long current_right(collage_info_manager.collage_info[i].dimentions.x + collage_info_manager.collage_info[i].position.x+image_margin);
		if (prop.width < current_right)
			prop.width = current_right;

		if (output_region_label_mask) {  //all sample regions in the same column, one column per sample
			ns_64_bit next_sample_id(0);
			if (i + 1 < subjects.size() ) next_sample_id = ns_atoi64(subjects[i + 1][3].c_str());

			if (next_sample_id == collage_info_manager.collage_info[i].sample_id)
				current_y = current_top;
			else {
				current_x = current_right+ image_margin;
				current_y = label_margin_buffer;
			}
		}
		else { //all sample regions in separate column
			current_x = current_right;
			current_y = label_margin_buffer;
		}
	}
	//cerr << "Prop height = " << prop.height << "\n";
	prop.height += ns_bulk_experiment_mask_manager::label_margin_buffer;

	//insert metadata into image
	prop.description = collage_info_manager.to_string();
	metadata_output << collage_info_manager.to_string();
	metadata_output.close();
	render_mask_file(prop, images, reciever);
}


void ns_bulk_experiment_mask_manager::render_mask_file(const ns_image_properties & prop,std::vector<ns_image_buffered_random_access_input_image<ns_8_bit, ns_image_storage_source<ns_8_bit> > > & images, ns_image_stream_file_sink<ns_8_bit> & reciever) {


	const ns_8_bit background_color(160);

	bool start_verbosity = image_server.verbose_debug_output();
	if (start_verbosity) cerr << "\nFlushing...\n";
	ns_image_buffered_random_access_output_image<ns_8_bit> mask_file(chunk_max_size);
	mask_file.prepare_to_recieve_image(prop);
	mask_file.init_flush_recipient(reciever);

	//cerr << "Creating image with size (" << mask_file.properties().width << "," << mask_file.properties().height << ")\n";
			
	ns_image_properties label_prop(prop);
	label_prop.height = ns_bulk_experiment_mask_manager::label_margin_buffer;

	if (start_verbosity) cerr << "Drawing labels...\n";
	ns_acquire_lock_for_scope font_lock(font_server.default_font_lock, __FILE__, __LINE__);
	ns_font & font(font_server.get_default_font());
			
	font.set_height(label_margin_buffer/3);
	//cerr << "Font Size = " << label_margin_buffer/3 << "\n";
	ns_image_standard label_im;
	label_im.init(label_prop);	

	for (unsigned int y = 0; y < label_prop.height; y++)
		for (unsigned int x = 0; x <label_prop.width; x++)
			label_im[y][x]=background_color;
	for (unsigned int i = 0; i < images.size(); i++){
		font.draw_grayscale(collage_info_manager.collage_info[i].position.x+50,
							label_margin_buffer-label_margin_buffer/3,0,collage_info_manager.collage_info[i].sample_name,label_im);
	}

	font_lock.release();
	for (unsigned int y = 0; y < label_prop.height; y++)
		for (unsigned int x = 0; x < label_prop.width; x++)
			mask_file[y][x] = label_im[y][x];
					

	if (start_verbosity) cerr << "Drawing images...\n";
	mask_file.flush_buffer(label_prop.height,reciever);

	ns_progress_reporter pr(prop.height,10);
	for (unsigned int y = label_margin_buffer; y < prop.height;){
		cerr << (100*y)/prop.height << "%";
		//pr(y);
		int chunk_size = prop.height - y;
		if (chunk_size > chunk_max_size)
			chunk_size = chunk_max_size;
		for (unsigned int _y = 0; _y < chunk_size; _y++)
		  for (unsigned int x = 0; x < prop.width; x++)
		    mask_file[y+_y][x] = background_color;
		for(unsigned int i = 0; i < images.size(); i++){
		 
					
			if ((unsigned int)collage_info_manager.collage_info[i].position.y > y + (unsigned int)chunk_size ||
				(unsigned int)collage_info_manager.collage_info[i].position.y+images[i].properties().height/resize_factor < y){
			  //for (unsigned int _y = 0; _y < chunk_size; _y++)
			  //		for (unsigned int x = 0; x < images[i].properties().width/resize_factor; x++)
			  //			mask_file[y+_y][collage_info_manager.collage_info[i].position.x +x] = background_color;
				continue;
			}

			//find the first line of the current chunk (relative to y) in which data needs to be written for the current image
			unsigned long start_offset = 0;
			if (collage_info_manager.collage_info[i].position.y > y)
				start_offset = collage_info_manager.collage_info[i].position.y-y;
			//fill in the blank until that offset
			for (unsigned int _y = 0; _y < start_offset; _y++)
				for (unsigned int x = 0; x < images[i].properties().width/resize_factor; x++)
					mask_file[y+_y][x] = background_color;

			//find the last line of the current chunk in which data needs to be written for the current image
			int stop_offset = (images[i].properties().height/resize_factor+collage_info_manager.collage_info[i].position.y)-y;
			if (stop_offset > chunk_size)
				stop_offset = chunk_size;

		


//				cerr << "image: " << i << " lines " << y+start_offset << " to " << y+stop_offset << "\n";
//				cerr << "Collage position = " << collage_info_manager.collage_info[i].position.x << "," << collage_info_manager.collage_info[i].position.y << " to " <<
//					collage_info_manager.collage_info[i].position.x + images[i].properties().width << "," <<  collage_info_manager.collage_info[i].position.y + images[i].properties().height << "\n";

			for (int _y = start_offset; _y < stop_offset; _y++){
				for (unsigned int x = 0; x < images[i].properties().width/resize_factor; x++){
					if ((y+_y - collage_info_manager.collage_info[i].position.y)*resize_factor >= images[i].properties().height)
						throw ns_ex("Yikes!");
					if (collage_info_manager.collage_info[i].position.x + x > prop.width)
						throw ns_ex("Yikes!");
					ns_image_buffered_random_access_input_image<ns_8_bit,ns_image_storage_source<ns_8_bit> > & im(images[i]);
					unsigned long y_read((y+_y - collage_info_manager.collage_info[i].position.y)*resize_factor),
								x_read(resize_factor*x),
									y_write(y+_y),
									x_write(collage_info_manager.collage_info[i].position.x + x);
						ns_8_bit * read_line(im[y_read]);
						ns_8_bit val(read_line[x_read]);
						ns_8_bit * write_line(mask_file[y_write]);
						write_line[x_write] = val;
					//mask_file[y+_y][collage_info_manager.collage_info[i].position.x + x] = images[i][(y+_y - collage_info_manager.collage_info[i].position.y)*resize_factor][resize_factor*x];
				}
			}

			for (int _y = stop_offset; _y < (int)chunk_size; _y++)
				for (unsigned int x = 0; x < images[i].properties().width/resize_factor; x++)
					mask_file[y + _y][collage_info_manager.collage_info[i].position.x + x] = background_color;;

					
			for (unsigned int _y = 0; _y < y+(int)chunk_size; _y++){
				for (unsigned int x = 0; x < image_margin; x++){
				mask_file[y + _y][collage_info_manager.collage_info[i].position.x + images[i].properties().width/resize_factor + x] = background_color;
				}
			}
		}
		unsigned long flush_size(chunk_size);
		if (prop.height < y+chunk_size)
			flush_size =  prop.height - y;
					
	//	cerr << "flush_size: " << flush_size << "\n";;

		mask_file.flush_buffer(flush_size,reciever);
		y+=flush_size;
	}
	reciever.finish_recieving_image();
	pr(prop.height);
	
}


void ns_bulk_experiment_mask_manager::submit_subregion_label_masks_to_cluster(bool balk_on_overwrite) {
	ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__, __LINE__));
	for (unsigned int i = 0; i < collage_info_manager.collage_info.size(); i++) {
		if (collage_info_manager.collage_info[i].region_id == 0)
			throw ns_ex() << "Region id 0 found.\n";

		if (collage_info_manager.collage_info[i].region_id == 0)
			throw ns_ex("Attempting to submit an experiment plate reigon mask as a subregion label mask!");

		sql() << "SELECT " << ns_processing_step_db_column_name(ns_process_subregion_label_mask) << " FROM capture_sample_region_info WHERE id = " << collage_info_manager.collage_info[i].region_id;
		ns_sql_result res;
		sql().get_rows(res);
		if (res.size() == 0)
			throw ns_ex("Could not find region in db") << collage_info_manager.collage_info[i].region_id;
		if (ns_atoi64(res[0][0].c_str()) != 0) {
			if (balk_on_overwrite)
				throw ns_ex("ns_bulk_experiment_mask_manager::submit_masks_to_cluster::A label mask already exists for specified sample: ") << collage_info_manager.collage_info[i].sample_id << ". To continue anyway, select the menu item Config/Set Behavior/Overwrite existing sample masks";
		}
	}

	for (unsigned int i = 0; i < collage_info_manager.collage_info.size(); i++) {
		ns_image_server_captured_image_region region_image;
		region_image.region_info_id = collage_info_manager.collage_info[i].region_id;
		ns_image_server_image mask_image = region_image.create_storage_for_processed_image(ns_process_subregion_label_mask, ns_tiff, &sql()); 
	
		sql() << "INSERT INTO image_masks SET image_id = " << mask_image.id << ", processed='0',resize_factor=" << resize_factor;
		ns_64_bit mask_id = sql().send_query_get_id();

		bool had_to_use_local_storage;
		ns_image_storage_reciever_handle<ns_8_bit> image_storage = image_server.image_storage.request_storage(mask_image, ns_tiff, 1.0, 512, &sql(), had_to_use_local_storage, false, false);

		//ns_image_standard decoded_image;
		ns_image_storage_source_handle<ns_8_bit> in(image_server.image_storage.request_from_local_cache(scratch_filenames[i]));
		in.input_stream().pump(image_storage.output_stream(), 512);
		//decoded_image.pump(sender,512);
		//c.close();

		sql() << "INSERT INTO processing_jobs SET image_id=" << mask_image.id << ", mask_id=" << mask_id << ", "
			<< "op" << (unsigned int)ns_process_analyze_mask << " = 1, time_submitted=" << ns_current_time() << ", urgent=1";
		sql().send_query();
		sql().send_query("COMMIT");

		//		cerr << "\nDone.\n";
	}
}

void ns_bulk_experiment_mask_manager::submit_plate_region_masks_to_cluster(bool balk_on_overwrite) {
	bool ex_not_ok = false;
	ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__, __LINE__));
	for (unsigned int i = 0; i < collage_info_manager.collage_info.size(); i++) {
		try {
			if (collage_info_manager.collage_info[i].sample_id == 0) {
				cerr << "Sample id 0 found.\n";
				continue;
			}
			if (collage_info_manager.collage_info[i].region_id != 0)
				throw ns_ex("Attempting to submit a subregion label mask as an experiment plate reigon mask!");

			sql() << "SELECT mask_id FROM capture_samples WHERE id = " << collage_info_manager.collage_info[i].sample_id;
			ns_sql_result res;
			sql().get_rows(res);
			if (res.size() == 0)
				throw ns_ex("Could not find sample in db");

			if (ns_atoi64(res[0][0].c_str()) != 0) {
				if (balk_on_overwrite) {
					ex_not_ok = true;
					throw ns_ex("ns_bulk_experiment_mask_manager::submit_masks_to_cluster::A mask already exists for specified sample: ") << collage_info_manager.collage_info[i].sample_id << ". To continue anyway, select the menu item Config/Set Behavior/Overwrite existing sample masks";

				}
				else {
					//	sql() << "DELETE FROM image_masks WHERE id = " << res[0][0];
					//	sql().send_query();
					sql() << "UPDATE sample_region_image_info SET name = CONCAT(name,'_old'),censored=1 WHERE sample_id = " << collage_info_manager.collage_info[i].sample_id;
					sql().send_query();
					sql() << "UPDATE captured_images SET mask_applied=0 WHERE sample_id = " << collage_info_manager.collage_info[i].sample_id << " AND image_id != 0 AND mask_applied=0";
					sql().send_query();
				}
			}
			//Connecting to image server
			/*	cerr << "Connecting to image server...";
			ns_socket socket;
			ns_socket_connection c;
			try{
			c = socket.connect(ip_address,port);
			}
			catch(...){
			ex_not_ok = true;
			throw;
			}*/
			//Updating database
			//cerr << "\nUpdating database....";
			ns_64_bit image_id = image_server.make_record_for_new_sample_mask(collage_info_manager.collage_info[i].sample_id, sql());
			sql() << "INSERT INTO image_masks SET image_id = " << image_id << ", processed='0',resize_factor=" << resize_factor;
			ns_64_bit mask_id = sql().send_query_get_id();
			ns_image_server_image image;
			image.load_from_db(image_id, &sql());
			bool had_to_use_local_storage;
			ns_image_storage_reciever_handle<ns_8_bit> image_storage = image_server.image_storage.request_storage(image, ns_tiff, 1.0,512, &sql(), had_to_use_local_storage, false, false);
			
			//ns_image_standard decoded_image;
			ns_image_storage_source_handle<ns_8_bit> in(image_server.image_storage.request_from_local_cache(scratch_filenames[i]));
			in.input_stream().pump(image_storage.output_stream(), 512);
			//decoded_image.pump(sender,512);
			//c.close();


			sql() << "UPDATE capture_samples SET mask_id=" << mask_id << " WHERE id=" << collage_info_manager.collage_info[i].sample_id;
			sql().send_query();
			sql() << "INSERT INTO processing_jobs SET image_id=" << image_id << ", mask_id=" << mask_id << ", "
				<< "op" << (unsigned int)ns_process_analyze_mask << " = 1, time_submitted=" << ns_current_time() << ", urgent=1";
			sql().send_query();
			sql().send_query("COMMIT");

			//		cerr << "\nDone.\n";
		}
		catch (ns_ex & ex) {
			if (ex_not_ok)
				throw ex;
			else cerr << ex.text() << "\n";
		}
	}
	ns_image_server_push_job_scheduler::request_job_queue_discovery(sql());
	sql.release();

}
