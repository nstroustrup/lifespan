#include "ns_worm_browser.h"
#include "ns_worm_training_set_image.h"
#include "ns_time_path_image_analyzer.h"
#include "ns_time_path_solver.h"
#include "ns_hand_annotation_loader.h"
#include "ns_machine_analysis_data_loader.h"
#include "ns_captured_image_statistics_set.h"
#include <set>
#include "ns_experiment_storyboard.h"
#include "ns_ex.h"
#include "ns_movement_visualization_generator.h"
#include "ns_time_path_image_analyzer.h"
#include "ns_hidden_markov_model_posture_analyzer.h"
#include "ns_fl_modal_dialogs.h"
#include <set>
#include "ns_processing_job_processor.h"
#include "hungarian.h"
#include "ns_analyze_movement_over_time.h"
using namespace std;

void ns_to_lower(std::string & s){
	for (unsigned int i = 0; i < s.size(); i++)
		s[i] = tolower(s[i]);
}
ns_sql & ns_worm_learner::get_sql_connection() {

	ns_acquire_lock_for_scope lock(persistant_sql_lock, __FILE__, __LINE__);
	if (persistant_sql_connection == 0) {
		persistant_sql_connection = image_server.new_sql_connection(__FILE__, __LINE__);
	}
	bool try_to_reestablish_connection = false;

	try {
		persistant_sql_connection->clear_query();
		persistant_sql_connection->check_connection();
		image_server.check_for_sql_database_access(persistant_sql_connection);
	}
	catch (ns_ex ex) {
		std::cerr << ex.text() << "\n";
		try_to_reestablish_connection = true;
	}
	

	if (try_to_reestablish_connection) {
		//if we've lost the connection, try to reconnect via conventional means
		image_server.reconnect_sql_connection(persistant_sql_connection);
		persistant_sql_connection->check_connection();
		lock.release();
		image_server.register_server_event(ns_image_server_event("Recovered from a lost MySQL connection."), persistant_sql_connection);

	}
	lock.release();
	return *persistant_sql_connection;
}

void ns_worm_learner::produce_experiment_mask_file(const std::string & filename){
	if (data_selector.current_experiment_id() == 0)
		throw ns_ex("ns_worm_learner::No experiment selected!");
	int long experiment_id = data_selector.current_experiment_id();
	ns_image_standard mask_file;

	ns_tiff_image_output_file<ns_8_bit> tiff_out;
	ns_image_stream_file_sink<ns_8_bit> file_sink(filename,tiff_out,1024, 1.0);

	cerr << "writing: " << filename  << "\n";	
	
	ns_sql & sql = get_sql_connection();
	sql << "SELECT mask_time FROM experiments WHERE id = " << experiment_id;
	unsigned long mask_time = sql.get_ulong_value();
	cerr << "Generating mask";
	if (mask_time != 0)
		cerr << " using images up until" << ns_format_time_string_for_human(mask_time);
	const std::string metadata_output_filename(ns_bulk_experiment_mask_manager::metadata_filename(filename));

	mask_manager.produce_mask_file(experiment_id, metadata_output_filename,file_sink, sql,mask_time);
}


void ns_worm_learner::decode_experiment_mask_file(const std::string & filename, const std::string & output_vis_filename){
	ns_tiff_image_input_file<ns_8_bit> tiff;
	tiff.open_file(filename);
	ns_image_stream_file_source<ns_8_bit> source(tiff);

	ns_image_buffered_random_access_input_image<ns_8_bit, ns_image_stream_file_source<ns_8_bit> > mask_file(1024);
	mask_file.assign_buffer_source(source);
	
	ifstream metadata_file(ns_bulk_experiment_mask_manager::metadata_filename(filename).c_str());
	ifstream * metadata_file_ref = 0;
	if (!metadata_file.fail())
		metadata_file_ref = &metadata_file;
	mask_manager.decode_mask_file(mask_file,metadata_file_ref);
	mask_file.clear();
	ns_image_standard vis;
	mask_manager.process_mask_file(vis);
	if (output_vis_filename.size() > 0)
	ns_save_image(output_vis_filename,vis);
	ns_image_properties prop(vis.properties());
	prop.width = 1200;
	prop.height =(unsigned long)(((1.0*prop.width)/vis.properties().width)*prop.height);
	
	ns_acquire_lock_for_scope lock(current_image_lock,__FILE__,__LINE__);
	vis.resample(prop,current_image);
	lock.release();
	vis.clear();
//	draw();
}
void ns_worm_learner::submit_experiment_mask_file_to_cluster(){
	mask_manager.submit_masks_to_cluster(!overwrite_existing_mask_when_submitting);
}



void ns_worm_learner::resize_image(){
	ns_resampler<ns_8_bit> rsp(75);
	ns_image_standard out;
	rsp.set_resize_factor(ns_vector_2d(0.5,0.5));
	ns_image_stream_binding<
		ns_resampler<ns_8_bit>,
		ns_image_standard >
		resampler(rsp, out,75);
	current_image.pump(resampler,75);
	out.pump(current_image,75);
	draw();
}

void ns_worm_learner::zhang_thinning(){
	ns_image_standard tmp;
	current_image.pump(tmp,1024);
	ns_zhang_thinning(tmp,current_image);
	draw_image(-1,-1,current_image);
}

void ns_worm_learner::compress_dark_noise(){

	ns_image_standard 
		out;
	ns_spatial_median_calculator<ns_8_bit,false> sp(128,65);

	ns_image_stream_binding<
		ns_spatial_median_calculator<ns_8_bit,false>,
		ns_image_standard >
		spatial_averager_bound(sp, out,128);

	current_image.pump(spatial_averager_bound,512);

	const unsigned int h(current_image.properties().height),
					   w(current_image.properties().width);

	for (unsigned int y = 0; y < h; y++)
		for (unsigned int x = 0; x < w; x++){
			if (ns_spatial_median_calculator_operation_calc<ns_8_bit,true>::run(current_image[y][x],out[y][x]) >=5 )
				out[y][x] = current_image[y][x];
		}
	out.pump(current_image,512);
	draw_image(-1,-1,current_image);
}
void ns_worm_learner::two_stage_threshold(const bool & make_vis){
	ns_image_standard out;
	ns_two_stage_difference_thresholder::run(current_image,out,0,ns_two_stage_difference_parameters(),make_vis);
		out.pump(current_image,512);
		 out.pump(thresholded_image,512);
	//if (!make_vis)
	//	stretch_levels();
	
//		else
		draw_image(-1,-1,current_image);
}
void ns_worm_learner::calculate_movement_threshold(const std::string & filename, const bool & visulization){
	ns_image_standard first,second;
	current_image.pump(first,512);
	load_file(filename, second);
	ns_movement_threshold<ns_threshold_two_stage>(first,second,current_image,visulization);
	draw_image(-1,-1,current_image);


}
void ns_worm_learner::set_svm_model_specification(ns_worm_detection_model_cache::const_handle_t & spec){
	model_specification = &spec;
	cout << "Using model specification " << spec().model_specification.model_name << "\n";
}
void ns_worm_learner::calculate_heatmap_overlay(){
	ns_image_standard tmp;
	ns_worm_multi_frame_interpolation::generate_static_mask_from_heatmap(current_image,tmp);
	tmp.pump(current_image,512);
	draw();
}

void ns_worm_learner::output_lots_of_worms(const std::string & path){
/*	unsigned long number_of_days(5);

	if (data_selector.current_experiment_id() == 0)
		throw ns_ex("ns_worm_learner::No experiment selected!");
	unsigned int experiment_id = data_selector.current_experiment_id();
	ns_sql * sql = image_server.new_sql_connection(__FILE__,__LINE__);
	*sql << "SELECT id,name FROM capture_samples WHERE experiment_id = " << experiment_id;
	try{
		ns_sql_result samples;
		sql->get_rows(samples);

		for (unsigned int s = 0; s < samples.size(); s++){
			std::string sample_name = samples[s][1];
			ns_sql_result regions;
			*sql << "SELECT sample_region_image_info.id, sample_region_image_info.name FROM sample_region_image_info "
				 << "WHERE sample_region_image_info.sample_id = " << samples[s][0];
			sql->get_rows(regions);	
			std::vector<ns_image_standard> graphs(regions.size());

			for (unsigned int i = 0; i < regions.size(); i++){
				ns_worm_multi_frame_interpolation mfi;
				mfi.load_all_region_worms(atol(regions[i][0].c_str()),*sql);
				mfi.output_worm_images_to_disk(path,std::string("im") + sample_name,*sql,number_of_days);
			}
		}
		delete sql;
	}
	catch(...){
		delete sql;
		throw;
	}*/
}


void ns_worm_learner::output_experiment_movement_graph_wrapper_files(ns_machine_analysis_region_data & r, const std::string & filename){
	/*ofstream o(filename.c_str());
	if (o.fail())
		throw ns_ex("make_experiment_summary()::Could not open") << filename << " for output.";
	for (unsigned int i = 0; i < r.size(); i++){
		o << "<table border=0 cellspacing=10><tr><td colspan=2>";
		o << "<B>" << r[i].sample_name << "::" << r[i].region_name << "</b></td></tr>";
		o << "<tr><td width=0>";
		o << "<object type=\"image/svg+xml\" data=\"" << r[i].area_graph_filename << "\" width=\"500\" height=\"300\"></object>";
		o << "</td>\n<td width = \"100%\">&nbsp</td></tr>\n";
		o << "<td><a href=\"" << r[i].raw_video_filename << "\">Brightfield</a<br>";
		o << "<a href=\"" << r[i].motion_video_filename << "\">Motion Capture</a";
		o << "</td>\n<td width = \"100%\">&nbsp</td></tr>\n";
		o << "</table><br><br>";
	}
	o << "Nicholas Stroustrup," << ns_format_time_string_for_human(ns_current_time()) << "<br>";
	o.close();*/
}
double ns_mean(const std::vector<double> & d){
	if (d.size() == 0) return 0;
	double sum(0);
	for (unsigned int i = 0; i < d.size(); i++)
		sum+=d[i];
	return sum/d.size();

}
double ns_variance(const double mean,const std::vector<double> & d){
	if (d.size() == 0) return 0;
	double sum(0);
	for (unsigned int i = 0; i < d.size(); i++)
		sum+=(d[i]-mean)*(d[i]-mean);
	return sum/d.size();
}
void ns_worm_learner::create_feature_time_series(const std::string & directory){
	ns_dir dir;
	dir.load_masked(directory,"csv",dir.files);
	if (dir.files.size() == 0)
		throw ns_ex("No csv files present in directory");
	//load headers
	std::vector<std::string> headers(1);
	ifstream in((directory + DIR_CHAR_STR + dir.files[0]).c_str());
		if (in.fail())
			throw ns_ex("Could not open") << directory + DIR_CHAR_STR + dir.files[0];
	unsigned int h = 0;
	while(true){
		char s(in.get());
		if (s==','){
			h++;
			headers.resize(h+1);
			continue;
		}
		if (s=='\n')
			break;
		headers[h]+=s;
	}
	in.close();
	//sort files by time;
	std::sort(dir.files.begin(),dir.files.end());
	//get times
	std::vector<unsigned long> times(dir.files.size());
	for (unsigned int i = 0; i < dir.files.size(); i++){
		unsigned eq = 0;
		std::string tmp;
		for (unsigned int j = 0; j < dir.files[i].size(); j++){
			if (dir.files[i][j] == '=')
				eq++;
			else if (eq == 4)
				tmp+=dir.files[i][j];
			else if (eq == 5){
				times[i] = atol(tmp.c_str());
				break;
			}
		}
	}
	cerr << "Loading...";
	std::vector<std::vector< std::vector<double> > > data(dir.files.size(),std::vector<std::vector<double> >(headers.size()));
	for (unsigned i = 0; i < dir.files.size(); i++){
		ifstream in((directory + DIR_CHAR_STR + dir.files[i]).c_str());
		cerr << (100*i)/dir.files.size() << "%...";
		if (in.fail())
			throw ns_ex("Could not open") << directory + DIR_CHAR_STR + dir.files[i];
		std::string tmp;
		getline(in,tmp);
		unsigned long c = 0;
		tmp.resize(0);
		while(true){
			char s(in.get());
			if (in.fail())
				break;
			if (s==',' || s=='\n')	{
				data[i][c].push_back(atof(tmp.c_str()));
				tmp.resize(0);
				c++;
				if (s=='\n')
					c=0;
			}
			else tmp+=s;
		}
		in.close();
	}
	cerr << "\n";
	cerr << "Writing...";
	std::string output_file = directory + DIR_CHAR_STR + "features_jmp.csv";
	ofstream out(output_file.c_str());
	out << "time,feature,mean,variance\n";
	if (out.fail()) throw ns_ex("Could not open ") << output_file;
	for (unsigned int i = 0; i < data.size(); i++){
		cerr << (100*i)/data.size() << "%...";
		for (unsigned int j = 0; j < data[i].size(); j++){
		double m(ns_mean(data[i][j]));
		out << times[i] << "," << headers[j] << "," << m << "," << ns_variance(m,data[i][j]) << "\n";
		}
	}
	out.close();
}

void ns_worm_learner::load_current_experiment_movement_results(const ns_death_time_annotation_set::ns_annotation_type_to_load & annotations_to_load, const unsigned long experiment_id){		
	if (data_selector.current_experiment_id() == 0)
		throw ns_ex("ns_worm_learner::No experiment selected!");
	
	ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__,__LINE__));
	//if we haven't loaded any data yet, load it
//	if (movement_results.experiment_id() != experiment_id || annotations_to_load != last_annotation_type_loaded){
		movement_results.clear();
		
		movement_results.load(annotations_to_load,0,0,experiment_id,sql());
		last_annotation_type_loaded = annotations_to_load;
		movement_data_is_strictly_decreasing_ = false;
//	}
	//otherwise, refresh the metadat for the experiments in case it has changed.
/*	else{
		for (unsigned int i = 0; i < movement_results.samples.size(); i++){
			for (unsigned int j = 0; j < movement_results.samples[i].regions.size(); j++)
				movement_results.samples[i].regions[j].metadata.load_from_db(movement_results.samples[i].regions[j].metadata.region_id,"",sql());
		
		}
	}*/
	sql.release();
}

inline double ns_calc_gaussian_neighboorhood(const ns_image_standard & im, const long khr, const long x, const long y, double (*gk)[50]){
	double lp(0);
	for (long y_ = 0; y_ <= khr; y_++)
		for (long x_ = 0; x_ <= khr; x_++)
			lp+=im[y+y_][x+x_]*gk[y_][x_];
	for (long y_ = 1; y_ <= khr; y_++)
		for (long x_ = 0; x_ <= khr; x_++)
			lp+=im[y-y_][x+x_]*gk[y_][x_];			
	for (long y_ = 0; y_ <= khr; y_++)
		for (long x_ = 1; x_ <= khr; x_++)
			lp+=im[y+y_][x-x_]*gk[y_][x_];		
	for (long y_ = 1; y_ <= khr; y_++)
		for (long x_ = 1; x_ <= khr; x_++)
			lp+=im[y-y_][x-x_]*gk[y_][x_];
	return lp-im[y][x];  //we don't count the center point
}
inline double ns_calc_val_gaussian_neighboorhood(const ns_8_bit &val, const long khr,  double (*gk)[50]){
	double lp(0);
	for (long y_ = 0; y_ <= khr; y_++)
		for (long x_ = 0; x_ <= khr; x_++)
			lp+=val*gk[y_][x_];
	for (long y_ = 1; y_ <= khr; y_++)
		for (long x_ = 0; x_ <= khr; x_++)
			lp+=val*gk[y_][x_];			
	for (long y_ = 0; y_ <= khr; y_++)
		for (long x_ = 1; x_ <= khr; x_++)
			lp+=val*gk[y_][x_];		
	for (long y_ = 1; y_ <= khr; y_++)
		for (long x_ = 1; x_ <= khr; x_++)
			lp+=val*gk[y_][x_];
	return lp-val;  //we don't count the center point
}

void ns_worm_learner::sharpen(){
	ns_image_standard &im(current_image);
	long khr(8);
	long k_area((2*khr+1)*(2*khr+1));
	ns_image_whole<ns_32_bit> val;
	val.prepare_to_recieve_image(im.properties());
	unsigned long max_val(0);
	//unsigned long area= (2*khh+1)*(2*khh+1)-1
	for (long y = khr; y < (long)im.properties().height-khr; y++)
		for (long  x = khr; x < (long)im.properties().width-khr; x++){
			long lp(-(k_area+1)*im[y][x]);
			for (long y_ = -khr; y_ <= khr; y_++)     //khr*khr
				for (long x_ = -khr; x_ <= khr; x_++)
					lp+=im[y+y_][x+x_];

			val[y][x] = -lp*(-lp>0);
			if (val[y][x] > max_val)
				max_val = val[y][x];
		}
	for (unsigned  long y = 0; y < im.properties().height; y++)
		for (unsigned  long x = 0; x < im.properties().width; x++)
			im[y][x] = (255*val[y][x])/max_val;

	draw();



	/*//generate gaussian kernal for specified radius
	long r = 2*khr+1;
	double g_std(1);
	double gk[50][50];
	for (int y = 0; y <= khr; y++)
		for (int x = 0; x <= khr; x++)
			gk[y][x] = exp(-.5*((x*x + y*y)/(g_std*g_std)))/sqrt(2*3.14159265359*g_std);

	double max_gk(ns_calc_val_gaussian_neighboorhood(1,khr,gk));
	//unsigned long area= (2*khh+1)*(2*khh+1)-1
	for (long y = khr; y < (long)im.properties().height-khr; y++)
		for (long  x = khr; x < (long)im.properties().width-khr; x++){
			double lp(ns_calc_gaussian_neighboorhood(im,khr,x,y,gk));

			double m =  im[y][x] -ksharp*lp;
			out[y][x] = (ns_8_bit)m;
		}
		out.pump(current_image,1024);
	draw();*/
}

void ns_worm_learner::concatenate_time_series(const std::string & path,const float resize_factor){
	
	std::vector<std::string> subdirs;
	std::vector<ns_dir> dirs(subdirs.size());
	unsigned long min_size(0);
	{
		ns_dir dir;
		dir.load(path);
		if (dir.dirs.size() < 2)
			throw ns_ex("No subdirectories");
		for (unsigned int i = 0; i < dir.dirs.size(); i++)
			if (dir.dirs[i] != "." && dir.dirs[i] != ".." && dir.dirs[i] != "out")
				subdirs.push_back(dir.dirs[i]);
		dirs.resize(subdirs.size());
		for (unsigned int i = 0; i < dirs.size(); i++)
			dirs[i].load(path + DIR_CHAR_STR + subdirs[i]);
		min_size = (unsigned long)dirs[0].files.size();
		for (unsigned int i = 0; i < dirs.size(); i++)
			if (dirs[i].files.size() < min_size)
				min_size = (unsigned long)dirs[i].files.size();
	}
	if (min_size == 0)
		throw ns_ex("At least one subdirectory is empty.");

	for (unsigned int i = 0; i < min_size; i++){
		cerr << "Frame " << i << "/" << min_size << "\n";
		std::vector<ns_image_standard> ims(subdirs.size());
		for (unsigned int j = 0; j < subdirs.size(); j++){
			ns_load_image(path + DIR_CHAR_STR + subdirs[j] + DIR_CHAR_STR + dirs[j].files[i],ims[j]);
			ns_image_properties p(ims[j].properties());
			p.height = p.height*resize_factor;
			p.width= p.width*resize_factor;
			p.resolution /= resize_factor;
			ns_image_standard tmp;
			ims[j].resample(p,tmp);
			tmp.pump(ims[j],1024);
		}
		ns_image_properties prop(ims[0].properties());
		std::vector<unsigned long> im_lefts(ims.size());
		im_lefts[0] = 0;
		for (unsigned int j = 1; j < ims.size(); j++){
			im_lefts[j] = prop.width;
			prop.width += ims[j].properties().width;
			if (prop.height < ims[j].properties().height)
				prop.height = ims[j].properties().height;
		}

		std::string out_dir = path + DIR_CHAR_STR + "out";
		ns_dir::create_directory_recursive(out_dir);
		ns_image_standard out;
		out.prepare_to_recieve_image(prop);
		for (unsigned int j = 0; j < ims.size(); j++){
			for (unsigned int y = 0; y < ims[j].properties().height; y++){
				for (unsigned int x = 0; x < ims[j].properties().components*ims[j].properties().width; x++){
					out[y][ims[j].properties().components*im_lefts[j]+x] = ims[j][y][x];
				}
			}
			for (unsigned int y = ims[j].properties().height; y < prop.height; y++){
				for (unsigned int x = 0; x < ims[j].properties().components*ims[j].properties().width; x++){
					out[y][ims[j].properties().components*im_lefts[j]+x] = 0;
				}
		
			}
		}
		std::string out_fname = out_dir + DIR_CHAR_STR + dirs[0].files[i];
		cerr << "Writing to " << out_fname << "\n";
		ns_save_image(out_fname,out);
	}
}


class ns_sample_region_error_data{
public:
	ns_sample_region_error_data():is_censored(false){}
	ns_region_metadata metadata;
	string error;
	bool is_censored;
};
class ns_experiment_error_data{
public:
	vector<ns_sample_region_error_data> regions;
	bool has_an_error_specified() const{
		for (unsigned int i = 0; i < regions.size(); i++) 
			if (regions[i].is_censored) return true;
		return false;
	}
};
class ns_device_history{
public:
	ns_device_history(){}
	ns_device_history(unsigned long id, string & name_):device_id(id),device_name(name_){}
	static void write_header(ostream & o){
		o << "Device,Experiment,Sample,Region,Problem,Error\n";
	}
	void write(ostream & o) const{
		for (unsigned int i = 0; i < history.size(); i++){
			o << history[i]->metadata.device << ","
				<<history[i]->metadata.experiment_name<< ","
				<<history[i]->metadata.sample_name<< ","
				<<history[i]->metadata.region_name<< ","
				<< (history[i]->is_censored?"1":"0") << ","
				<<history[i]->error
				<< "\n";
		}
	}
	unsigned long device_id;
	string device_name;
	vector<ns_sample_region_error_data *> history;
};

void ns_worm_learner::generate_scanner_report(unsigned long first_experiment_time, unsigned long last_experiment_time){
	
	ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__,__LINE__));
	map<string,ns_device_history> devices;
	vector<ns_experiment_error_data> experiments;
	
	//get devices
	
	{
		sql() << "SELECT name,id FROM devices";
		ns_sql_result res;
		sql().get_rows(res);
		for (unsigned int i = 0; i < res.size(); i++) 
			devices[res[i][0]] = ns_device_history(atol(res[i][1].c_str()),res[i][0]);
	}

	//get experiments
	{
		ns_sql_result res;
		sql() << "SELECT id, name FROM experiments WHERE first_time_point >= " << first_experiment_time << " AND first_time_point <= " << last_experiment_time;
		sql().get_rows(res);
		
		experiments.resize(res.size());
		for (unsigned int i = 0; i < res.size(); i++){
			sql() << "SELECT id,name, censored,description,device_name FROM capture_samples WHERE experiment_id = " << res[i][0];
			ns_sql_result res2;
			sql().get_rows(res2);
			for (unsigned int j = 0; j < res2.size(); j++){	
				sql() << "SELECT id,name, censored, details,strain,strain_condition_1,strain_condition_2 FROM sample_region_image_info WHERE sample_id = " << res2[j][0];
				ns_sql_result res3;
				sql().get_rows(res3);
				unsigned long s = experiments[i].regions.size();
				experiments[i].regions.resize(s + res3.size());
				for (unsigned int k = 0; k < res3.size(); k++){
					experiments[i].regions[s+k].metadata.experiment_name = res[i][1];
					experiments[i].regions[s+k].metadata.sample_name = res2[j][1];
					experiments[i].regions[s+k].metadata.sample_id = atol(res2[j][0].c_str());
					experiments[i].regions[s+k].metadata.device = res2[j][4];
					experiments[i].regions[s+k].metadata.region_name = res3[k][1];
					experiments[i].regions[s+k].metadata.details = res2[k][3];
					experiments[i].regions[s+k].metadata.strain_condition_1= res2[k][5];
					experiments[i].regions[s+k].metadata.strain_condition_2= res2[k][6];
					if (res2[j][2] != "0"){
						experiments[i].regions[s+k].is_censored = true;
						experiments[i].regions[s+k].error = res2[j][3];
					}
					else if (res3[k][2] != "0"){
						experiments[i].regions[s+k].is_censored = true;
						experiments[i].regions[s+k].error = res3[k][3];
					}
				}
			}
		}
	}

	for (unsigned int i = 0; i < experiments.size(); i++){
		for (unsigned int j =0; j < experiments[i].regions.size(); j++)
			devices[experiments[i].regions[j].metadata.device].history.push_back(&(experiments[i].regions[j]));
	}
	ns_image_server_results_subject sub;
	sub.device_name = "all_devices";
	sub.start_time = first_experiment_time;
	sub.stop_time = last_experiment_time;
	ns_image_server_results_file f(image_server.results_storage.device_history(sub,sql()));
	sql.release();
	ns_acquire_for_scope<ostream> o(f.output());
	ns_device_history::write_header(o());
	for (map<string,ns_device_history>::const_iterator p = devices.begin(); p != devices.end(); p++)
		p->second.write(o());
	o.release();
}


void ns_worm_learner::export_experiment_data(const unsigned long experiment_id){
	
	ns_sql & sql(get_sql_connection());
	ns_image_server_results_subject sub;
	sub.experiment_id = experiment_id;
	std::string dir(image_server.results_storage.db_export_directory(sub,sql));
	cout << "Writing experiment metadata to " << dir << "\n";
	ns_write_experimental_data_in_database_to_file(experiment_id,image_server.results_storage.db_export_directory(sub,sql),sql);
	cout << "Compressing data...";
	ns_zip_experimental_data(dir,true);
	cout << "\nDone.\n";
}

bool ns_worm_learner::import_experiment_data(const std::string & database_name,const std::string & directory, const bool reuse_database){
	ns_sql & sql(get_sql_connection());
	return ns_update_db_using_experimental_data_from_file(database_name,reuse_database,directory,sql);
}

void ns_worm_learner::analyze_time_path(const unsigned long region_id){
	ns_sql & sql(get_sql_connection());

	ns_time_path_solver_parameters solver_parameters(ns_time_path_solver_parameters::default_parameters(region_id,sql,true));
	ns_time_path_solution time_path_solution;
	ns_time_path_solver tp_solver;
	tp_solver.load(region_id,sql);
	tp_solver.solve(solver_parameters,time_path_solution,&sql);
	time_path_solution.fill_gaps_and_add_path_prefixes(ns_time_path_solution::default_length_of_fast_moving_prefix());
	time_path_solution.save_to_db(region_id,sql);
				
	ns_image_server_results_subject results_subject;
	results_subject.region_id = region_id;
			

	ns_acquire_for_scope<ostream> position_3d_file_output(
	image_server.results_storage.animal_position_timeseries_3d(
		results_subject,sql,ns_image_server_results_storage::ns_3d_plot
	).output()
	);
	time_path_solution.output_visualization_csv(position_3d_file_output());
	position_3d_file_output.release();
	cout << time_path_solution.paths.size() << " stationary animals detected.\n";
	cout << "Path lengths:\n";
	for (unsigned int i = 0; i < time_path_solution.paths.size(); i++){
		cout << time_path_solution.paths[i].stationary_elements.size() << ", ";
	}
	cout << "\n";

}

void ns_worm_learner::generate_morphology_statistics(const ns_64_bit & experiment_id) {


	ns_sql & sql(get_sql_connection());
	ns_image_server_results_subject sub;
	sub.experiment_id = experiment_id;
	ns_image_server_results_file outf(image_server.results_storage.worm_morphology_timeseries(sub, sql, true));

	ns_acquire_for_scope<ostream> o(outf.output());

	sql << "SELECT r.id FROM sample_region_image_info as r, capture_samples as s WHERE r.sample_id = s.id AND s.experiment_id = " << experiment_id;
	ns_sql_result res;
	sql.get_rows(res);
	bool write_header = true;
	int r(0);
	for (unsigned i = 0; i < res.size(); i++) {
		int r1 = (100 * i) / res.size();
		if (r1 - r > 5) {
			cout << r1 << "%...";
			r = r1;
		}
		ns_image_server_results_subject sub;
		sub.region_id = ns_atoi64(res[i][0].c_str());
		ns_image_server_results_file f(image_server.results_storage.worm_morphology_timeseries(sub, sql, false));
		ns_acquire_for_scope<istream> in(f.input());
		if (in.is_null())
			continue;
		char tmp(0);
		//only write header once
		while (!in().fail() && tmp != '\n') {
			tmp = in().get();
			if (write_header)
				o() << tmp;
		}
		write_header = false;
		//write out entire file
		o() << in().rdbuf();
	}
}

void ns_worm_learner::output_region_statistics(const unsigned long experiment_id, const unsigned long experiment_group_id){

	ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__,__LINE__));
	vector<ns_64_bit> experiment_ids;
	vector<string> experiment_names;
	const bool multi_experiment(experiment_id == 0);
	if (!multi_experiment){
		experiment_ids.push_back(experiment_id);
	}
	else{
		if (experiment_group_id == 0)
			throw ns_ex("ns_worm_learner::output_region_statistics()::No experiment group id provided!");
		
		sql() << "SELECT id,name FROM experiments WHERE group_id = " << experiment_group_id << " AND hidden = 0 ";
		ns_sql_result res;
		sql().get_rows(res);
		experiment_ids.resize(res.size());
		experiment_names.resize(res.size());
		for (unsigned int i = 0; i < res.size(); i++){
			experiment_ids[i] = ns_atoi64(res[i][0].c_str());
			experiment_names[i] = res[i][1];
		}
	}
	
	ns_image_server_results_subject sub;
	sub.experiment_id = experiment_id;
	ns_image_server_results_file f(image_server.results_storage.capture_region_image_statistics(sub,sql(),multi_experiment));
	
	ns_acquire_for_scope<ostream> o(f.output());
	ns_capture_sample_region_data::output_region_data_in_jmp_format_header("",o());
	
	for (unsigned int i = 0; i < experiment_ids.size(); i++){
		if (experiment_names.size() > 0)
			cout << "Processing " << experiment_names[i] << "...\n";
		ns_capture_sample_region_statistics_set set;
		set.load_whole_experiment(experiment_ids[i],sql(),false);

		for (unsigned int j = 0; j < set.regions.size(); j++)
			set.regions[j].output_region_data_in_jmp_format(o());

	}
	
	o.release();
	sql.release();
}


void ns_worm_learner::output_device_timing_data(const unsigned long experiment_id,const unsigned long experiment_group_id){
	bool multiple_experiments_requested=(experiment_id==0);
	bool experiment_group_id_requested=(experiment_group_id!=0);
	ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__,__LINE__));

	std::vector<unsigned long> experiment_ids;
	std::vector<std::string> experiment_names;
	if (multiple_experiments_requested){
			sql() << "SELECT id FROM experiments WHERE hidden = 0";
		if (experiment_group_id_requested)
			sql() << " AND group_id = " << experiment_group_id;
		ns_sql_result res;
		sql().get_rows(res);
		experiment_ids.resize(res.size());
		for (unsigned int i = 0; i < res.size(); i++){
			experiment_ids[i] = atol(res[i][0].c_str());
		}
	}
	else{
		experiment_ids.push_back(experiment_id);
	}

	ns_image_server_results_subject sub;
	if (!multiple_experiments_requested)
		sub.experiment_id = experiment_id;
	
	ns_image_server_results_file f(image_server.results_storage.capture_sample_image_statistics(sub,sql(),multiple_experiments_requested));

	ns_acquire_for_scope<ostream> o(f.output());


	std::string filename;
	
	ns_sql_result res;
	ns_capture_sample_image_statistics::output_jmp_header(o());
	for (unsigned int i = 0 ; i < experiment_ids.size(); i++){
		ns_capture_sample_statistics_set set;
		set.load_whole_experiment(experiment_ids[i],sql());
		
		for (unsigned int j = 0; j < set.samples.size(); j++){
			set.samples[j].output_jmp_format(o());
		}
	}
	o.release();
	sql.release();
}


//void ns_worm_learner::analyze_time_path(const unsigned long region_id){
	/*
	ns_sql * sql = image_server.new_sql_connection(__FILE__,__LINE__);
	try{
		
	/*	ns_time_path_image_movement_analyzer a;
		 ns_time_path_solution s;

		//1096
		//worm_a::4, 1105 is a great example of very closly juxtiposed stationary objects

		//1056 ->group index 10 (file name 11) has a good head moving worm
		 long group_to_process = -1;//17;
		{
			
			ns_time_path_solver tp_solver;
			tp_solver.load(region_id,*sql);
			unsigned long t(ns_current_time());
			tp_solver.solve(s);
			a.analyze(region_id,s,*sql);
		}
*/


		/*ofstream out("c:\\region_ex.csv");
		ns_segmented_time_path::output_visualization_header(out);
		for (unsigned int i = 0; i < a.size(); i++){
	//		if (group_to_process != -1 && i != group_to_process) continue;
			for (unsigned int j = 0; j < a[i].size(); j++){
				a[i].segmented_path(j).output_visualization_csv(out,4);
			}
		}

		out.close();
		
		for (unsigned int i = 0; i < a.size(); i++){
			
//			if (group_to_process != -1 && i != group_to_process) continue;
			std::string f("c:\\region_ex_");
			f+=ns_to_string(i+1);
			std::string f2(f),f3(f);
			f += ".csv";
			f2+="_c.csv";
			f3 += "_stat.csv";
			ofstream out2(f.c_str()),
					 out3(f2.c_str()),
					 out4(f3.c_str());
			ns_segmented_time_path::output_visualization_header(out2);
			ns_aligned_time_path::output_visualization_header(out3);
			for (unsigned int j = 0; j < a[i].size(); j++){
				a[i].segmented_path(j).output_statistics(out4);
				a[i].segmented_path(j).output_visualization_csv(out2,1);
				a[i].aligned_path(j).output_visualization_csv(out3,4,true);
			}
		}
		//exit(0);

		ns_movement_summarizer summarizer;
		try{
		//	summarizer.load_from_db(region_id,*sql);
		}
		catch(ns_ex & ex){
			cerr << ex.text() << "\n";
		}
		summarizer.load_from_db(region_id,*sql);
		summarizer.load_time_path_image_analysis_from_db(region_id,*sql);
	//	summarizer.build_movement_files(region_id,*sql,ns_movement_data_source_type::ns_time_path_image_analysis_data);
	//	summarizer.save_to_db(region_id,*sql,ns_movement_data_source_type::ns_time_path_image_analysis_data);
	//	summarizer.load_metadata(region_id,*sql);

		ns_graph graph;
		summarizer.create_area_graph_for_capture_time(-1,summarizer.image_data(),graph,"Movement Summary",false);
		graph.draw(current_image);
		draw();

		delete sql;
	}
	catch(...){
		delete sql;
		throw;
	}
	*/
//}

void ns_worm_learner::generate_survival_curve_from_hand_annotations(){
	if (!data_selector.experiment_selected())
		throw ns_ex("No experiment selected.");

	ns_death_time_annotation_compiler compiler;
	ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__,__LINE__));
	ns_hand_annotation_loader loader;
	//loader.load_region_annotations(ns_death_time_annotation_set::ns_censoring_and_movement_transitions,13899,sql());
	loader.load_experiment_annotations(ns_death_time_annotation_set::ns_censoring_and_movement_transitions,data_selector.current_experiment_id(),sql());
	ns_lifespan_experiment_set set;
	loader.annotations.generate_survival_curve_set(set,ns_death_time_annotation::ns_only_by_hand_annotations,false,false);

	//ns_device_temperature_normalization_data regression_data;
	//regression_data.produce_identity();
	//set.compute_device_normalization_regression(regression_data,ns_lifespan_experiment_set::ns_ignore_censoring_data,ns_lifespan_experiment_set::ns_exclude_tails);

	ns_image_server_results_subject results_subject;
	results_subject.experiment_id = data_selector.current_experiment_id();

	ns_acquire_for_scope<ostream> survival_jmp_file(image_server.results_storage.survival_data(results_subject,"by_hand","by_hand_interval_annotation_jmp",".csv",sql()).output());
	set.output_JMP_file(ns_death_time_annotation::ns_only_by_hand_annotations,ns_lifespan_experiment_set::ns_output_event_intervals,ns_lifespan_experiment_set::ns_days,survival_jmp_file(),ns_lifespan_experiment_set::ns_simple);
	survival_jmp_file.release();
	ns_acquire_for_scope<ostream> survival_jmp_file2(image_server.results_storage.survival_data(results_subject,"by_hand","by_hand_event_annotation_jmp",".csv",sql()).output());
	set.output_JMP_file(ns_death_time_annotation::ns_only_by_hand_annotations,ns_lifespan_experiment_set::ns_output_single_event_times,ns_lifespan_experiment_set::ns_days,survival_jmp_file2(),ns_lifespan_experiment_set::ns_simple);
	survival_jmp_file2.release();
	sql.release();
}


void ns_worm_learner::generate_training_set_from_by_hand_annotation(){
	if (!data_selector.experiment_selected())
		throw ns_ex("No experiment selected.");
	
	ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__,__LINE__));
	ns_hand_annotation_loader loader;
	loader.load_experiment_annotations(ns_death_time_annotation_set::ns_censoring_and_movement_transitions,data_selector.current_experiment_id(),sql());

	ns_death_time_annotation_compiler & death_time_annotation_compiler(loader.annotations);

	load_current_experiment_movement_results(ns_death_time_annotation_set::ns_censoring_and_movement_transitions,data_selector.current_experiment_id());


	for (unsigned int i = 0; i < movement_results.samples.size(); i++){
		for (unsigned int j = 0; j < movement_results.samples[i].regions.size(); j++){
				death_time_annotation_compiler.add(movement_results.samples[i].regions[j].death_time_annotation_set);
				death_time_annotation_compiler.specifiy_region_metadata(movement_results.samples[i].regions[j].metadata.region_id,
																	 movement_results.samples[i].regions[j].metadata);
				break;
		}
	}
	
	ns_image_server_results_subject results_subject;
	results_subject.experiment_id = data_selector.current_experiment_id();

	ns_image_standard im;
	unsigned long i(1);
	for (ns_death_time_annotation_compiler::ns_region_list::iterator p = death_time_annotation_compiler.regions.begin(); p != death_time_annotation_compiler.regions.end(); ++p){
		try{
			ns_worm_training_set_image::generate(p->second,im,sql());
		
			results_subject.sample_name = p->second.metadata.sample_name;
			results_subject.sample_id =  0;
			results_subject.region_name = p->second.metadata.region_name;
			results_subject.region_id =  p->second.metadata.region_id;
			ns_image_storage_reciever_handle<ns_8_bit> out(image_server.results_storage.machine_learning_training_set_image(results_subject,1024,sql()));
			cerr << "\nWriting " << p->second.metadata.sample_name << "::" << p->second.metadata.region_name << ": (" << i << "/" << death_time_annotation_compiler.regions.size() << ")\n";
			im.pump(out.output_stream(),1024);
		}
		catch(ns_ex & ex){
			cerr << "Could not process region " << p->second.metadata.sample_name << "::" << p->second.metadata.region_name << ": " << ex.text() << "\n";
		}
	}
	sql.release();
}


void ns_worm_learner::generate_worm_storyboard(const std::string & filename,const bool use_by_hand_timings,const ns_movement_event & event_to_mark,
											   unsigned long region_id, unsigned long sample_id, unsigned long experiment_id,ns_sql & sql, const ns_region_metadata & strain_to_use,bool use_absolute_time){

	
	/*ns_experiment_storyboard board;
	board.load_events_from_db(use_by_hand_timings,event_to_mark,region_id,sample_id,experiment_id,sql,strain_to_use,use_absolute_time);
	
	ns_image_standard im;
	board.draw(im,sql);
	ns_save_image(filename,im);*/

}

void ns_worm_learner::generate_detailed_animal_data_file(){
	
}
template<class T>
class ns_draw_element{
public:
	ns_draw_element(){}
	ns_draw_element(const T & t){element = t;}
	T element;
	bool excluded;
};
template<class T, class metadata_t>
class ns_draw_element_group{
public:
	typedef std::vector<ns_draw_element<T> > ns_element_list;
	ns_element_list elements;
	unsigned long unexcluded_count;
	metadata_t metadata;
};
template<class T, class metadata_t>
struct ns_death_time_compiler_random_picker{
	typedef std::vector<ns_draw_element_group<T, metadata_t> > ns_element_group_list;
	typedef std::pair<T,metadata_t> return_type;
	ns_element_group_list element_groups;
		
	const return_type get_random_event_with_replacement(){
		typename ns_element_group_list::iterator element_group;
		typename ns_draw_element_group<T,metadata_t>::ns_element_list::iterator element;

		get_random_region_and_location(element_group,element);
		//region_sizes[&region->second]--;
		//location->properties.excluded = ns_death_time_annotation::ns_excluded;
		//total_count--;
		return return_type(element->element,element_group->metadata);
	}

	const return_type get_random_event_without_replacement(){
		typename ns_element_group_list::iterator element_group;
		typename ns_draw_element_group<T,metadata_t>::ns_element_list::iterator element;
		get_random_region_and_location(element_group,element);
		element_group->unexcluded_count--;
		element->excluded = true;
		total_count--;
		return return_type(element->element,element_group->metadata);
	}

	void initialize_for_picking(){
		total_count = 0;
		for (typename ns_element_group_list::iterator p = element_groups.begin(); p != element_groups.end(); p++){
			
		for (typename ns_draw_element_group<T,metadata_t>::ns_element_list::iterator q = p->elements.begin(); q != p->elements.end(); q++){
				q->excluded = false;
			}
			p->unexcluded_count = p->elements.size();
			total_count += p->elements.size();
		}
	}
	unsigned long number_unchosen(){return total_count;}
	void add(const T & val,const metadata_t & m,const unsigned long group_id){
		if (group_id >= element_groups.size())
			element_groups.resize(group_id+1);
		element_groups[group_id].elements.push_back(ns_draw_element<T>(val));
		element_groups[group_id].metadata = m;
	}
private:
	void get_random_region_and_location(typename ns_element_group_list::iterator & group,typename ns_draw_element_group<T, metadata_t>::ns_element_list::iterator & element){
		if (total_count == 0)
			throw ns_ex("No annotations in set!");
		unsigned long r((unsigned long)((rand()/(double)RAND_MAX)*(total_count-1)));
		unsigned long c(0);
		for (group = element_groups.begin(); group != element_groups.end(); group++){
			
			if (c + group->unexcluded_count > r){
				const unsigned long location_number(r-c);
				
				unsigned long f(0);
				for (unsigned int i = 0; i < group->elements.size(); i++){
					if (!group->elements[i].excluded){
						if (f==location_number){
							element = group->elements.begin()+location_number;
							return;
						}
						f++;
					}
				}
				throw ns_ex("Hit end of location list!");
			}
			c+=group->unexcluded_count;
		}

		throw ns_ex("Hit the end of the region list!");
	}
	
	unsigned long total_count;
};

typedef ns_death_time_compiler_random_picker<ns_dying_animal_description, ns_region_metadata> ns_random_picker_type;
typedef std::map<std::string,ns_random_picker_type > ns_compiler_random_picker_list;

struct ns_lifespan_experiment_set_multiworm_simulation{
	ns_lifespan_experiment_set minimums,
							   means,
							   maximums;
	void specify_analysis_type_and_technique(const std::string & analysis_type){
		for (unsigned int i = 0; i < minimums.curves.size(); i++){
			minimums.curves[i].metadata.analysis_type = analysis_type;
			minimums.curves[i].metadata.technique = "Simulated Minimum";
		}
		for (unsigned int i = 0; i < maximums.curves.size(); i++){
			maximums.curves[i].metadata.analysis_type = analysis_type;
			maximums.curves[i].metadata.technique = "Simulated Maximum";
		}
		for (unsigned int i = 0; i < means.curves.size(); i++){
			means.curves[i].metadata.analysis_type = analysis_type;
			means.curves[i].metadata.technique = "Simulated Mean";
		}
	};
	void output_JMP_file(const ns_lifespan_experiment_set::ns_time_handing_behavior & time_handling_behavior, const ns_lifespan_experiment_set::ns_time_units & u,std::ostream & o,const ns_lifespan_experiment_set::ns_output_file_type & detailed, const bool output_header){
		minimums.output_JMP_file(ns_death_time_annotation::ns_only_machine_annotations,time_handling_behavior,u,o,detailed,output_header);
		maximums.output_JMP_file(ns_death_time_annotation::ns_only_machine_annotations,time_handling_behavior,u,o,detailed,output_header);
		means.output_JMP_file(ns_death_time_annotation::ns_only_machine_annotations,time_handling_behavior,u,o,detailed,output_header);
	}
	void clear(){
		minimums.clear();
		maximums.clear();
		means.clear();
	}
};
struct ns_death_time_annotation_compiler_multiworm_simulation{
	ns_death_time_annotation_compiler minimums,
									  means,
									  maximums;
	void generate_survival(ns_lifespan_experiment_set_multiworm_simulation & s){
		minimums.generate_survival_curve_set(s.minimums,ns_death_time_annotation::ns_machine_annotations_if_no_by_hand,false,false);
		maximums.generate_survival_curve_set(s.maximums,ns_death_time_annotation::ns_machine_annotations_if_no_by_hand,false,false);
		means.generate_survival_curve_set(s.means,ns_death_time_annotation::ns_machine_annotations_if_no_by_hand,false,false);
	}
	void clear(){
		maximums.clear();
		minimums.clear();
		means.clear();
	}
};


void ns_handle_pair(const ns_random_picker_type::return_type & d1, const ns_random_picker_type::return_type & d2,bool use_waiting_time_cropping, unsigned long wait_time,const unsigned long rround,
	ns_death_time_annotation_compiler_multiworm_simulation & worms){
	
	const ns_random_picker_type::return_type * mmax, * mmin;
				
	if (d1.first.machine.death_annotation->time.period_end > d2.first.machine.death_annotation->time.period_end){
		mmax = &d1;
		mmin = &d2;
	}
	else{
		mmax = &d2;
		mmin = &d1;
	}

	if (mmin->first.machine.stationary_worm_dissapearance->time.period_end < mmax->first.machine.last_fast_movement_annotation->time.period_end){
		ns_death_time_annotation a(*mmin->first.machine.death_annotation),
									b(*mmax->first.machine.death_annotation);
		a.loglikelihood = b.loglikelihood = rround; 

		worms.maximums.add(a,mmin->second);
		worms.minimums.add(a,mmin->second);
		worms.means.add(a,mmin->second);
		worms.maximums.add(b,mmin->second);
		worms.minimums.add(b,mmin->second);
		worms.means.add(b,mmin->second);
		return;
	}
				
	unsigned long mean_death_time(d1.first.machine.death_annotation->time.period_end/2.0 + d2.first.machine.death_annotation->time.period_end/2.0);
	bool cropped_by_wait(false);
	if (use_waiting_time_cropping){
		if (mmin->first.machine.death_annotation->time.period_end + wait_time <
			mmax->first.machine.last_fast_movement_annotation->time.period_end){
			cropped_by_wait = true;
			mmax = mmin;
			mean_death_time = mmin->first.machine.death_annotation->time.period_end;
		}
	}
	ns_death_time_annotation mmin_a(*mmin->first.machine.death_annotation),
								mmax_a(*mmax->first.machine.death_annotation),
								mmean_a(*d1.first.machine.death_annotation);

	mmean_a.time.period_end = mmean_a.time.period_end = mean_death_time;
				
	//encoding to allow extraction of this information from compiler later
	mmax_a.number_of_worms_at_location_marked_by_machine = mmax_a.number_of_worms_at_location_marked_by_hand = 2;
	mmin_a.number_of_worms_at_location_marked_by_machine = mmin_a.number_of_worms_at_location_marked_by_hand  = 2;
	mmean_a.number_of_worms_at_location_marked_by_machine = mmean_a.number_of_worms_at_location_marked_by_hand = 2;	
	mmax_a.annotation_source_details = mmin_a.annotation_source_details = mmean_a.annotation_source_details = 
		cropped_by_wait?"Cropped by permanance time":"Not Cropped";
				
	mmax_a.loglikelihood = mmin_a.loglikelihood = mmean_a.loglikelihood = rround; //carry this through for posterity, so we can look
																		//at variation
	worms.maximums.add(mmax_a,mmax->second);
	worms.minimums.add(mmin_a,mmin->second);
	worms.means.add(mmean_a,d1.second);
}
void ns_worm_learner::simulate_multiple_worm_clumps(const bool use_waiting_time_cropping,const bool require_nearly_slow_moving){
	ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__,__LINE__));
	load_current_experiment_movement_results(ns_death_time_annotation_set::ns_censoring_and_movement_transitions,data_selector.current_experiment_id());
	ns_image_server::ns_posture_analysis_model_cache::const_handle_t handle;
	image_server.get_posture_analysis_model_for_region(
		movement_results.samples.begin()->regions.begin()->metadata.region_id, handle, sql());
	unsigned long wait_time = handle().model_specification.threshold_parameters.permanance_time_required_in_seconds;
	handle.release();
	wait_time=24*60*60;

	ns_compiler_random_picker_list random_pickers_by_strain;
	
	ns_death_time_annotation_compiler single_worms,
									  two_worms,
									  three_worms,
									  four_plus_worms;
	ns_death_time_annotation_compiler_multiworm_simulation
								simulated_single_worms,
								simulated_double_worms,
								simulated_triple_worms;


	
	ns_lifespan_experiment_set_multiworm_simulation
								simulated_singles,
							   simulated_doubles,
							   simulated_triples;
	unsigned death_dissapearance_time (60*60*24);
	if (!require_nearly_slow_moving)
		death_dissapearance_time*=100; 
	map<unsigned long,ns_death_time_annotation_compiler> source_annotation_compilers;
	unsigned long k(0);
	for (unsigned int i = 0; i < movement_results.samples.size(); i++){
		for (unsigned int j = 0; j < movement_results.samples[i].regions.size(); j++){
			//	death_time_annotation_compiler.add(movement_results.samples[i].regions[j].death_time_annotation_set);
			//	death_time_annotation_compiler.specifiy_region_metadata(movement_results.samples[i].regions[j].metadata.region_id,
			//														 movement_results.samples[i].regions[j].metadata);
				ns_random_picker_type & picker(random_pickers_by_strain[movement_results.samples[i].regions[j].metadata.plate_type_summary()]);
				ns_death_time_annotation_compiler &compiler(source_annotation_compilers[k]);
				k++;
				compiler.add(movement_results.samples[i].regions[j].death_time_annotation_set,movement_results.samples[i].regions[j].metadata);

				ns_hand_annotation_loader loader;
				loader.load_region_annotations(ns_death_time_annotation_set::ns_censoring_and_movement_transitions,movement_results.samples[i].regions[j].metadata.region_id,sql());
				compiler.add(loader.annotations);
				unsigned long region_count_id(0);
				for (ns_death_time_annotation_compiler::ns_region_list::iterator p = compiler.regions.begin(); p != compiler.regions.end(); p++){
					for (ns_death_time_annotation_compiler_region::ns_location_list::iterator q = p->second.locations.begin(); q != p->second.locations.end(); q++){
						if (q->properties.excluded != ns_death_time_annotation::ns_not_excluded || q->properties.flag.event_should_be_excluded())
							continue;
						ns_dying_animal_description d(q->generate_dying_animal_description(false));
						if (d.machine.death_annotation == 0 || d.machine.last_fast_movement_annotation == 0 || d.machine.last_slow_movement_annotation == 0)
							continue;
						if (d.machine.last_fast_movement_annotation->time.period_end == d.machine.death_annotation->time.period_start)
							continue;
						if (d.machine.stationary_worm_dissapearance == 0 ){
							q->annotations.add(*d.machine.death_annotation);
							q->annotations.events.rbegin()->type = ns_stationary_worm_disappearance;
							q->annotations.events.rbegin()->time.period_end = q->annotations.events.rbegin()->time.period_start
								= d.machine.death_annotation->time.period_end + death_dissapearance_time; 
							d = q->generate_dying_animal_description(false);
						}
						else if (d.machine.stationary_worm_dissapearance->time.period_end > d.machine.death_annotation->time.period_end + death_dissapearance_time){
							d.machine.stationary_worm_dissapearance->time.period_end = d.machine.stationary_worm_dissapearance->time.period_start 
								= d.machine.death_annotation->time.period_end + death_dissapearance_time; 
						
						}
						q->properties.transfer_sticky_properties(*d.machine.death_annotation);
						q->properties.transfer_sticky_properties(*d.machine.last_fast_movement_annotation);
						q->properties.transfer_sticky_properties(*d.machine.last_slow_movement_annotation);
						q->properties.transfer_sticky_properties(*d.machine.stationary_worm_dissapearance);
						if (d.machine.death_annotation->number_of_worms() < 2){
							single_worms.add(*d.machine.death_annotation,movement_results.samples[i].regions[j].metadata);
							picker.add(d,movement_results.samples[i].regions[j].metadata, region_count_id);
						}
						else if (d.machine.death_annotation->number_of_worms_at_location_marked_by_hand == 2){
							two_worms.add(*d.machine.death_annotation,movement_results.samples[i].regions[j].metadata);
						}
						else if (d.machine.death_annotation->number_of_worms_at_location_marked_by_hand == 3){
							three_worms.add(*d.machine.death_annotation,movement_results.samples[i].regions[j].metadata);
						}
						else 
							four_plus_worms.add(*d.machine.death_annotation,movement_results.samples[i].regions[j].metadata);
					}
					region_count_id++;
				}
		}
	}

	
	ns_image_server_results_subject results_subject;
	results_subject.experiment_id = data_selector.current_experiment_id();
	string fn("simulated_worm_clusters");

	if(use_waiting_time_cropping)
		fn+="=with_permanance_time_crop";
	else 
		fn+="=without_permanance_time_crop";
	if (require_nearly_slow_moving)
		fn+="=require_nearly_slow_moving";
	else fn+="=do_not_require_nearly_slow_moving";

	ns_acquire_for_scope<ostream> o(image_server.results_storage.survival_data(results_subject,"",fn,"csv",sql()).output());
	ns_lifespan_experiment_set singles,
							   twos,
							   threes,
							   four_pluses;

	single_worms.generate_survival_curve_set(singles,ns_death_time_annotation::ns_machine_annotations_if_no_by_hand,false,false);
	two_worms.generate_survival_curve_set(twos,ns_death_time_annotation::ns_machine_annotations_if_no_by_hand,false,false);
	three_worms.generate_survival_curve_set(threes,ns_death_time_annotation::ns_machine_annotations_if_no_by_hand,false,false);
	four_plus_worms.generate_survival_curve_set(four_pluses,ns_death_time_annotation::ns_machine_annotations_if_no_by_hand,false,false);
	for (unsigned int i = 0; i < singles.curves.size(); i++){
		singles.curves[i].metadata.analysis_type = "1 Worm Clusters";
		singles.curves[i].metadata.technique = "Measured";
	}
	for (unsigned int i = 0; i < twos.curves.size(); i++){
		twos.curves[i].metadata.analysis_type = "2 Worm Clusters";
		twos.curves[i].metadata.technique = "Measured";
	}
	for (unsigned int i = 0; i < threes.curves.size(); i++){
		threes.curves[i].metadata.analysis_type = "3 Worm Clusters";
		threes.curves[i].metadata.technique = "Measured";
	}
	for (unsigned int i = 0; i < four_pluses.curves.size(); i++){
		four_pluses.curves[i].metadata.analysis_type = "4+ Worm Clusters";
		four_pluses.curves[i].metadata.technique = "Unchanged";
	}
	
	singles.output_JMP_file(ns_death_time_annotation::ns_only_machine_annotations,ns_lifespan_experiment_set::ns_output_event_intervals,ns_lifespan_experiment_set::ns_days,o(),ns_lifespan_experiment_set::ns_detailed_compact,true);
	twos.output_JMP_file(ns_death_time_annotation::ns_only_machine_annotations,ns_lifespan_experiment_set::ns_output_event_intervals,ns_lifespan_experiment_set::ns_days,o(),ns_lifespan_experiment_set::ns_detailed_compact,false);
	threes.output_JMP_file(ns_death_time_annotation::ns_only_machine_annotations,ns_lifespan_experiment_set::ns_output_event_intervals,ns_lifespan_experiment_set::ns_days,o(),ns_lifespan_experiment_set::ns_detailed_compact,false);
	//four_pluses.output_JMP_file(ns_lifespan_experiment_set::ns_days,o(),true,false);
	
		const unsigned long number_of_repeats(25);

	for (ns_compiler_random_picker_list::iterator p =  random_pickers_by_strain.begin(); p != random_pickers_by_strain.end(); p++){
		cerr << "\nStrain " << p->first << "...\n";
		p->second.initialize_for_picking();
		cerr << "Simulating Singles...\n";
		const unsigned long number_of_doubles(p->second.number_unchosen()/2);
		for (unsigned int r = 0; r < number_of_repeats; r++){
			cerr << r/(double)number_of_repeats << "...";
			for (unsigned int i = 0; i < number_of_doubles; i++){
				const ns_random_picker_type::return_type d1(p->second.get_random_event_without_replacement()),
														   d2(p->second.get_random_event_without_replacement());
				
				ns_death_time_annotation a(*d1.first.machine.death_annotation);
				//encoding to allow extraction of this information from compiler later
				a.number_of_worms_at_location_marked_by_hand = a.number_of_worms_at_location_marked_by_machine = 1;
				a.loglikelihood = r/3;
				//simulated_single_worms.maximums.add(a,p->second.compiler.regions[a.region_info_id].metadata);
				//simulated_single_worms.minimums.add(b,p->second.compiler.regions[a.region_info_id].metadata);
				int rr(r%3);
				if (rr == 0)
					simulated_single_worms.means.add(a,d1.second);
				else if (rr == 1)
					simulated_single_worms.maximums.add(a,d1.second);
				else simulated_single_worms.minimums.add(a,d1.second);
			}
			
			simulated_single_worms.generate_survival(simulated_singles);
			simulated_singles.specify_analysis_type_and_technique( "1 Worm Clusters");
			simulated_singles.output_JMP_file(ns_lifespan_experiment_set::ns_output_event_intervals,ns_lifespan_experiment_set::ns_days,o(),ns_lifespan_experiment_set::ns_detailed_compact,false);
			simulated_single_worms.clear();
			simulated_singles.clear();
			p->second.initialize_for_picking();
		}
		cerr << "\nSimulating Doubles...\n";
		
	//	const unsigned long number_of_doubles(p->second.number_unchosen()/2);
	//	const unsigned long number_of_repeats(25);
		for (unsigned int r = 0; r < number_of_repeats; r++){
			cerr << r/(double)number_of_repeats << "...";
			for (unsigned int i = 0; i < number_of_doubles; i++){
				const ns_random_picker_type::return_type d1(p->second.get_random_event_without_replacement()),
														   d2(p->second.get_random_event_without_replacement());
				ns_handle_pair(d1,d2,use_waiting_time_cropping,wait_time,r,simulated_double_worms);
				/*
				const ns_random_picker_type::return_type * mmax, * mmin;
				
				if (d1.first.death_annotation->time.period_end > d2.first.death_annotation->time.period_end){
					mmax = &d1;
					mmin = &d2;
				}
				else{
					mmax = &d2;
					mmin = &d1;
				}

				if (mmin->first.stationary_worm_dissapearance < mmax->first.last_fast_movement_annotation->time.period_end){
					ns_death_time_annotation a(mmin->first.death_annotation),
											 b(mmax->first.death_annotation);
					a.loglikelihood = b.loglikelihood = r; 

					simulated_triple_worms.maximums.add(a,mmin->second);
					simulated_triple_worms.minimums.add(a,mmin->second);
					simulated_triple_worms.means.add(a,mmin->second);
					simulated_triple_worms.maximums.add(b,mmin->second);
					simulated_triple_worms.minimums.add(b,mmin->second);
					simulated_triple_worms.means.add(b,mmin->second);
					continue;
				}
				
				unsigned long mean_death_time(d1.first.death_annotation->time.period_end/2.0 + d2.first.death_annotation->time.period_end/2.0);
				bool cropped_by_wait(false);
				if (use_waiting_time_cropping){
					if (mmin->first.death_annotation->time.period_end + wait_time <
						mmax->first.last_fast_movement_annotation->time.period_end){
						cropped_by_wait = true;
						mmax = mmin;
						mean_death_time = mmin->first.death_annotation->time.period_end;
					}
				}
				ns_death_time_annotation mmin_a(*mmin->first.death_annotation),
											mmax_a(*mmax->first.death_annotation),
											mmean_a(*d1.first.death_annotation);

				mmean_a.timeperiod_start = mmean_a.time.period_end = mean_death_time;
				
				//encoding to allow extraction of this information from compiler later
				mmax_a.number_of_worms_at_location_marked_by_machine = mmax_a.number_of_worms_at_location_marked_by_hand = 2;
				mmin_a.number_of_worms_at_location_marked_by_machine = mmin_a.number_of_worms_at_location_marked_by_hand  = 2;
				mmean_a.number_of_worms_at_location_marked_by_machine = mmean_a.number_of_worms_at_location_marked_by_hand = 2;	
				mmax_a.annotation_source_details = mmin_a.annotation_source_details = mmean_a.annotation_source_details = 
					cropped_by_wait?"Cropped by permanance time":"Not Cropped";
				
				mmax_a.loglikelihood = mmin_a.loglikelihood = mmean_a.loglikelihood = r; //carry this through for posterity, so we can look
																				   //at variation
				simulated_double_worms.maximums.add(mmax_a,mmax->second);
				simulated_double_worms.minimums.add(mmin_a,mmin->second);
				simulated_double_worms.means.add(mmean_a,d1.second);
				*/
			}
			
			simulated_double_worms.generate_survival(simulated_doubles);
			simulated_doubles.specify_analysis_type_and_technique( "2 Worm Clusters");
			simulated_doubles.output_JMP_file(ns_lifespan_experiment_set::ns_output_event_intervals,ns_lifespan_experiment_set::ns_days,o(),ns_lifespan_experiment_set::ns_detailed_compact,false);
			simulated_double_worms.clear();
			simulated_doubles.clear();
			p->second.initialize_for_picking();
		}

		//reset random picker and redo for triples
		const unsigned long number_of_triples(p->second.number_unchosen()/3);
		cerr << "\nSimulating Triples...\n";
		for (unsigned int r = 0; r < number_of_repeats; r++){
			cerr << r/(double)number_of_repeats << "...";
			for (unsigned int i = 0; i < number_of_triples; i++){
				
				const ns_random_picker_type::return_type  d1(p->second.get_random_event_without_replacement()),
														   d2(p->second.get_random_event_without_replacement()),
														   d3(p->second.get_random_event_without_replacement());
				ns_death_time_annotation a(*d1.first.machine.death_annotation),
										 b(*d2.first.machine.death_annotation),
										 c(*d3.first.machine.death_annotation);

				//encoding to allow extraction of this information from compiler later
				

				const ns_random_picker_type::return_type * mmax, * mmin, *mmiddle;
				if (d1.first.machine.death_annotation->time.period_end> d2.first.machine.death_annotation->time.period_end){
					mmax = &d1;
					mmiddle = &d2;
				}
				else{
					mmax = &d2;
					mmiddle = &d1;
				}
				if (mmax->first.machine.death_annotation->time.period_end > d3.first.machine.death_annotation->time.period_end){
					if (mmiddle->first.machine.death_annotation->time.period_end > d3.first.machine.death_annotation->time.period_end){
						mmin = &d3;
					}
					else{
						mmin = mmiddle;
						mmiddle = &d3;
					}
				}
				else{
					mmin = mmiddle;
					mmiddle = mmax;
					mmax = &d3;
				}
				if (mmin->first.machine.stationary_worm_dissapearance->time.period_end < mmiddle->first.machine.last_fast_movement_annotation->time.period_end){
					ns_death_time_annotation a(*mmin->first.machine.death_annotation);
					a.number_of_worms_at_location_marked_by_machine = 1;
					a.loglikelihood = r; 
					simulated_triple_worms.maximums.add(a,mmin->second);
					simulated_triple_worms.minimums.add(a,mmin->second);
					simulated_triple_worms.means.add(a,mmin->second);
					ns_handle_pair(*mmiddle,*mmax,use_waiting_time_cropping,wait_time,r,simulated_triple_worms);
					continue;
				}
				if (mmiddle->first.machine.stationary_worm_dissapearance->time.period_end < mmax->first.machine.last_fast_movement_annotation->time.period_end){
					ns_death_time_annotation a(*mmax->first.machine.death_annotation);
					a.number_of_worms_at_location_marked_by_machine = 1;
					a.loglikelihood = r; 
					simulated_triple_worms.maximums.add(a,mmax->second);
					simulated_triple_worms.minimums.add(a,mmax->second);
					simulated_triple_worms.means.add(a,mmax->second);
					ns_handle_pair(*mmiddle,*mmin,use_waiting_time_cropping,wait_time,r,simulated_triple_worms);
					continue;
				}
				
				unsigned long mean_death_time = (unsigned long)(d1.first.machine.death_annotation->time.period_end/3.0
																				+d2.first.machine.death_annotation->time.period_end/3.0
																				+d3.first.machine.death_annotation->time.period_end/3.0);				
				int cropped_by_wait;
				if (use_waiting_time_cropping){
					if (mmin->first.machine.death_annotation->time.period_end + wait_time <
						mmiddle->first.machine.last_fast_movement_annotation->time.period_end){
						cropped_by_wait = 0;
						mmax = mmin;
						mmiddle = mmin;
						mean_death_time = mmin->first.machine.death_annotation->time.period_end;
					}
					else if (mmiddle->first.machine.death_annotation->time.period_end + wait_time <
						mmax->first.machine.last_fast_movement_annotation->time.period_end){
						cropped_by_wait = 1;
						mmin = mmiddle;
						mmax = mmiddle;
						mean_death_time = mmiddle->first.machine.death_annotation->time.period_end;
					}
					else cropped_by_wait = 2;
				}
				else cropped_by_wait = 2;

				ns_death_time_annotation mmax_a(*mmax->first.machine.death_annotation),
					mmin_a(*mmin->first.machine.death_annotation),
					mmean_a(*d1.first.machine.death_annotation);

				mmax_a.number_of_worms_at_location_marked_by_machine = mmax_a.number_of_worms_at_location_marked_by_hand = 3;
				mmin_a.number_of_worms_at_location_marked_by_machine = mmin_a.number_of_worms_at_location_marked_by_hand  = 3;
				mmean_a.number_of_worms_at_location_marked_by_machine = mmean_a.number_of_worms_at_location_marked_by_hand = 3;	

				mmax_a.loglikelihood = mmin_a.loglikelihood = mmean_a.loglikelihood = r; //carry this through for posterity, so we can look
																				   //at variation

				mmean_a.time.period_end = mmean_a.time.period_end = mean_death_time;

				
				switch(cropped_by_wait){
					case 0: mmax_a.annotation_source_details = mmin_a.annotation_source_details = mmean_a.annotation_source_details = 
						"Cropped at first by permanance time";break;
					case 1: mmax_a.annotation_source_details = mmin_a.annotation_source_details = mmean_a.annotation_source_details = 
						"Cropped at second by permanance time";break;
					case 2: mmax_a.annotation_source_details = mmin_a.annotation_source_details = mmean_a.annotation_source_details = 
						"Not Cropped";break;
					default: throw ns_ex("YIKES");
				}

				simulated_triple_worms.maximums.add(mmax_a,mmax->second);
				simulated_triple_worms.minimums.add(mmin_a,mmin->second);
				simulated_triple_worms.means.add(mmean_a,d1.second);
			}
			simulated_triple_worms.generate_survival(simulated_triples);
			simulated_triples.specify_analysis_type_and_technique( "3 Worm Clusters");
			simulated_triples.output_JMP_file(ns_lifespan_experiment_set::ns_output_event_intervals,ns_lifespan_experiment_set::ns_days,o(),ns_lifespan_experiment_set::ns_detailed_compact,false);
			simulated_triple_worms.clear();
			simulated_triples.clear();
			p->second.initialize_for_picking();
		}
	}
	
	o.release();
}

struct ns_machine_by_hand_comp {
	ns_death_time_annotation_compiler death_time_annotation_compiler;
};
void ns_worm_learner::compare_machine_and_by_hand_annotations(){
	if (!data_selector.experiment_selected())
		throw ns_ex("No experiment selected.");
	
	ns_sql & sql(get_sql_connection());

	load_current_experiment_movement_results(ns_death_time_annotation_set::ns_censoring_and_movement_transitions,data_selector.current_experiment_id());

	map<std::string, ns_machine_by_hand_comp> per_strain_analysis;

	

	ns_death_time_annotation_compiler death_time_annotation_compiler;
	for (unsigned int i = 0; i < movement_results.samples.size(); i++){
		for (unsigned int j = 0; j < movement_results.samples[i].regions.size(); j++){
			//all strains
			death_time_annotation_compiler.add(movement_results.samples[i].regions[j].death_time_annotation_set);
			death_time_annotation_compiler.specifiy_region_metadata(movement_results.samples[i].regions[j].metadata.region_id,
				movement_results.samples[i].regions[j].metadata);
			
			//per strain analysis
			map<std::string, ns_machine_by_hand_comp>::iterator p = per_strain_analysis.find(movement_results.samples[i].regions[j].metadata.plate_type_summary());
			if (p == per_strain_analysis.end()) {
				p = per_strain_analysis.insert(per_strain_analysis.begin(), std::pair<std::string, ns_machine_by_hand_comp>(movement_results.samples[i].regions[j].metadata.plate_type_summary(), ns_machine_by_hand_comp()));
				p->second.death_time_annotation_compiler.specifiy_region_metadata(movement_results.samples[i].regions[j].metadata.region_id,
					movement_results.samples[i].regions[j].metadata);
			}
			p->second.death_time_annotation_compiler.add(movement_results.samples[i].regions[j].death_time_annotation_set);
		}
	}	
	ns_hand_annotation_loader loader;
	loader.load_experiment_annotations(ns_death_time_annotation_set::ns_censoring_and_movement_transitions,data_selector.current_experiment_id(),sql);
	//all strains
	death_time_annotation_compiler.add(loader.annotations);
	//per strain
	for (map<std::string, ns_machine_by_hand_comp>::iterator p = per_strain_analysis.begin(); p != per_strain_analysis.end(); p++) {
		p->second.death_time_annotation_compiler.add(loader.annotations, ns_death_time_annotation_compiler::ns_do_not_create_regions);
	}

	//write all strain data to disk
	ns_image_server_results_subject results_subject;
	results_subject.experiment_id = data_selector.current_experiment_id();
	ns_acquire_for_scope<ostream> animal_data_file(image_server.results_storage.animal_event_data(results_subject,"automated_inspected_comparison",sql).output());
	double overall_msqerr(0);
	ns_64_bit overall_count(0);
	death_time_annotation_compiler.generate_animal_event_method_comparison(animal_data_file(), overall_msqerr, overall_count);
	overall_msqerr /= overall_count;
	overall_msqerr /= (60 * 24 * 60 * 24);
	animal_data_file.release();

	std::string results_text("===Comparrison between Storyboard By-Hand Annotations and Fully-Automated Machine Results===\n");
	results_text += "Calculated at " + ns_format_time_string_for_human(ns_current_time()) + "\n\n";
	for (map<std::string, ns_machine_by_hand_comp>::iterator p = per_strain_analysis.begin(); p != per_strain_analysis.end(); p++) {
		results_text += "**For plates of type " + p->first + " **\n";
		double local_msqerr(0);
		ns_64_bit local_count(0);
		ofstream tmp;
		p->second.death_time_annotation_compiler.generate_animal_event_method_comparison(tmp, local_msqerr, local_count);

		local_msqerr /= local_count;
		local_msqerr /= (60*24*60*24);

		results_text += "The machine differed from by-hand annotations by\n" + ns_to_string_short(sqrt(local_msqerr),3) + " days on average (mean squared error :" + ns_to_string_short(local_msqerr,3) + " days squared)\n";
		
		bool enough_worms = local_count < 50;
		if (enough_worms)
			results_text += "Only " + ns_to_string(local_count) + " individuals were annotated by hand.  It is recommended that you annotate more individuals of this type and re-run this analysis, to produce a more reliable parameter set.\n";
		else results_text += ns_to_string(local_count) + " individuals were annotated by hand to produce these estimates.\n";
		results_text += "\n";
	}

	results_text += "**For all plates in this experiment **\n";
	results_text += "The machine differed from by-hand annotations by\n" + ns_to_string_short(sqrt(overall_msqerr),3) + " days on average (mean squared error :" + ns_to_string_short(overall_msqerr,3) + " square days\n";
	ns_text_dialog td;
	td.grid_text.push_back(results_text);
	td.title = "Results";
	td.w = 1000;
	td.h = 400;
	ns_run_in_main_thread_custom_wait<ns_text_dialog> b(&td);
}


void ns_worm_learner::generate_scanner_lifespan_statistics(bool use_by_hand_censoring,const vector<unsigned long> & experiment_ids, const std::string & output_filename){
	ns_device_lifespan_statistics_compiler compiler;

	ofstream o(output_filename.c_str());
	if (o.fail())
		throw ns_ex("Could not open ") << output_filename;
	
	ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__,__LINE__));

	for (unsigned int e = 0; e < experiment_ids.size(); e++){

		
		ns_image_server_results_subject results_subject;
		results_subject.experiment_id = experiment_ids[e];
		ns_image_server_results_file results_file(image_server.results_storage.survival_data(results_subject,"summary","machine_summary_xml","xml",sql()));
		
		ns_lifespan_experiment_set set;

		//attempt to load cached info from disk
		ns_acquire_for_scope<istream> in(results_file.input());
		if (0 && !in.is_null()){
			//	load_from_disk

		}
		else{
			//load from database 
			load_current_experiment_movement_results(ns_death_time_annotation_set::ns_censoring_and_movement_transitions,experiment_ids[e]);
			results_subject.experiment_id = experiment_ids[e];
			results_subject.experiment_name = movement_results.experiment_name();

			ns_death_time_annotation_compiler survival_curve_compiler;
			for (unsigned int i = 0; i < movement_results.samples.size(); i++){
			
				results_subject.sample_name = movement_results.samples[i].name();
				results_subject.sample_id = movement_results.samples[i].id();

				cerr << "Sample " <<movement_results.samples[i].name() << " has " << movement_results.samples[i].regions.size() << " regions\n";
				for (unsigned int j = 0; j < movement_results.samples[i].regions.size(); j++){
					results_subject.region_name = movement_results.samples[i].regions[j].metadata.region_name;
					results_subject.region_id = movement_results.samples[i].regions[j].metadata.region_id;
					survival_curve_compiler.add(movement_results.samples[i].regions[j].death_time_annotation_set,movement_results.samples[i].regions[j].metadata);
				
				}
			}
			if (use_by_hand_censoring){
				ns_hand_annotation_loader loader;
				loader.load_experiment_annotations(ns_death_time_annotation_set::ns_censoring_data,experiment_ids[e],sql());
				survival_curve_compiler.add(loader.annotations);
			}

			ns_acquire_for_scope<ostream> out(results_file.output());
			if (out.is_null())
				throw ns_ex("Could not open file ") << results_file.output_filename() << " for output";
			survival_curve_compiler.generate_survival_curve_set(set,ns_death_time_annotation::ns_machine_annotations_if_no_by_hand,false,false);

			//cache for later
//			set.output_xml_summary_file(out());

			
			out.release();
		}
		
//		for (unsigned int i = 0; i < set.curves.size(); i++)
//			compiler.add_data(set.curves[i].produce_summary());	


	}
	sql.release();

	compiler.save_to_disk(o);
	o.close();

};
struct ns_parameter_set_optimization_record {
	ns_parameter_set_optimization_record(std::vector<double> & thresholds, vector<unsigned long> & hold_times) :total_count(0),best_parameter_count(0),new_parameters_results(thresholds.size(), hold_times.size()), current_parameter_results(1,1){}
	ns_parameter_optimization_results new_parameters_results;
	ns_parameter_optimization_results current_parameter_results;
	ns_threshold_movement_posture_analyzer_parameters best;
	double best_mean_squared_error;
	ns_64_bit total_count;
	unsigned long best_parameter_count;
	void find_best_parameter_set( std::vector<double> & thresholds, vector<unsigned long> & hold_times, bool posture_not_expansion) {
		double best_weighted_mean_squared_error = DBL_MAX;
		const ns_64_bit total_count_expected = new_parameters_results.number_valid_worms;

		for (unsigned int i = 0; i < thresholds.size(); i++) {
			for (unsigned int j = 0; j < hold_times.size(); j++) {
				if (new_parameters_results.counts[i][j] == 0)
					continue;
				total_count += new_parameters_results.counts[i][j];
				//penalize for parameter combinations that do not converge
				const double cur_msq = (new_parameters_results.death_total_mean_square_error_in_hours[i][j] / (double)new_parameters_results.counts[i][j]);
				double weighted_msq = cur_msq + 5*cur_msq*(total_count_expected - new_parameters_results.counts[i][j]);
			//	cout << cur_msq << " " << total_count_expected - new_parameters_results.counts[i][j] << " " << weighted_msq << "\n";
				if (new_parameters_results.counts[i][j] > 0 && weighted_msq < best_weighted_mean_squared_error) {
					if (posture_not_expansion) {
						best.stationary_cutoff = thresholds[i];
						best.permanance_time_required_in_seconds = hold_times[j];
					}
					else {
						best.death_time_expansion_cutoff= thresholds[i];
						best.death_time_expansion_time_kernel_in_seconds = hold_times[j];
					}
					best_parameter_count = new_parameters_results.counts[i][j];
					best_mean_squared_error = cur_msq;
					best_weighted_mean_squared_error = weighted_msq;
				}
			}
		}
	}
	std::string filename;
};


void ns_worm_learner::output_movement_analysis_optimization_data(int software_version_number, const ns_parameter_set_range & range, bool run_posture,bool run_expansion){
	ns_sql & sql(get_sql_connection());

	unsigned int experiment_id = data_selector.current_experiment_id();

	//posture analysis
	std::vector<double> posture_analysis_thresholds,
		expansion_analysis_thresholds;
	vector<unsigned long> posture_analysis_hold_times,
		expansion_analysis_hold_times;

	//old movement scores
	if (software_version_number == 1) {
		const double min_thresh(.0005);
		const double max_thresh(.5);
		const long number_of_thresholds(60);
		const double log_dt(((log(max_thresh) - log(min_thresh)) / number_of_thresholds));
		posture_analysis_thresholds.resize(number_of_thresholds);

		double cur = min_thresh;
		for (unsigned long i = 0; i < number_of_thresholds; i++) {
			posture_analysis_thresholds[i] = exp(log(min_thresh) + log_dt*i);
		}
	}
	else {
		//new movement scores
		const double min_thresh(.1);
		const double max_thresh(20000);
		const long number_of_thresholds(20);
		const double log_dt(((log(max_thresh) - log(min_thresh)) / number_of_thresholds));
		posture_analysis_thresholds.resize(number_of_thresholds);

		double cur = min_thresh;
		for (unsigned long i = 0; i < number_of_thresholds; i++) {
			posture_analysis_thresholds[i] = exp(log(min_thresh) + log_dt*i);
		}

	}
	
	
	//generate optimization training set 
	const unsigned long near_zero(30);
	const unsigned long thresh_num(20);
	bool by_hand_range(false);
	if (range == ns_v2) {

		posture_analysis_hold_times.reserve(40);
		posture_analysis_hold_times.push_back(0);
		for (unsigned int i = 0; i < 15; i++)
			posture_analysis_hold_times.push_back(i * 30 * 60);
		for (unsigned int i = 0; i < 6; i++)
			posture_analysis_hold_times.push_back((i) * 2 * 60 * 60);
		int p(posture_analysis_hold_times.size());
		for (unsigned int i = 6; i < 14; i++)
			posture_analysis_hold_times.push_back(posture_analysis_hold_times[p-1] +(i - 6) * 6 * 60 * 60);
	}
	if (range == ns_thermotolerance){
		
		posture_analysis_hold_times.reserve(16);
		posture_analysis_hold_times.push_back(0);
		for (unsigned int i = 0; i < 15; i++)
			posture_analysis_hold_times.push_back(i*45*60);
	}
	else if (range ==  ns_quiecent){

		posture_analysis_hold_times.reserve(21);
		for (unsigned int i = 0; i < 6; i++)
			posture_analysis_hold_times.push_back((i)*2*60*60);

		int p(posture_analysis_hold_times.size());
		for (unsigned int i = 6; i < 14; i++)
			posture_analysis_hold_times.push_back(p+(i-6)*6*60*60);
	}
	else{
		
		
		posture_analysis_hold_times.reserve(21);
		posture_analysis_hold_times.push_back(0);
		posture_analysis_hold_times.push_back(60*30);
		posture_analysis_hold_times.push_back(60*60);
		for (unsigned int i = 0; i < 11; i++)
			posture_analysis_hold_times.push_back((i+1)*2*60*60);
		for (unsigned int i = 0; i < 6; i++)
			posture_analysis_hold_times.push_back((i+5)*6*60*60);
	}


	for (unsigned int i = 0; i < 16; i++) 
		expansion_analysis_thresholds.push_back(( i ) * 25);
	
	for (unsigned int i = 0; i < 16; i++) 
		expansion_analysis_hold_times.push_back((i) * 30 * 60);
	


	ns_image_server_results_subject sub;
	sub.experiment_id = experiment_id;
	ns_acquire_for_scope<ostream> posture_analysis_optimization_output, expansion_analysis_optimization_output;
	if (run_posture) {
		posture_analysis_optimization_output.attach(image_server.results_storage.time_path_image_analysis_quantification(sub, "posture_analysis_optimization_stats", true, sql).output());
		ns_analyzed_image_time_path::write_posture_analysis_optimization_data_header(posture_analysis_optimization_output());
	}
	if (run_expansion) {
		expansion_analysis_optimization_output.attach(image_server.results_storage.time_path_image_analysis_quantification(sub, "expansion_optimization_stats", true, sql).output());
		ns_analyzed_image_time_path::write_expansion_analysis_optimization_data_header(expansion_analysis_optimization_output());
	}
	unsigned region_count(0);
	for (unsigned int i = 0; i < data_selector.samples.size(); i++)
		region_count+=data_selector.samples[i].regions.size();

	map<std::string, ns_parameter_set_optimization_record> best_posture_parameter_sets, best_expansion_parameter_sets;
	
	//	if (i>5)
	//	break;
	


//	hold_times.resize(2);
	//thresholds.resize(2);

	if (image_server.verbose_debug_output()) 
		cout << "Considering " << data_selector.samples.size() << " samples.\n";

	unsigned pos(0);
	for (unsigned int i = 0; i < data_selector.samples.size(); i++) {
		bool found_regions(false);
		for (unsigned int j = 0; j < data_selector.samples[i].regions.size(); j++) {
			//	if (data_selector.samples[i].sample_name != "azure_a" || data_selector.samples[i].regions[j].region_name != "0")
			//		continue;
			cerr << ns_to_string_short((100.0*pos) / region_count) << "%...";
			pos++;
			if (data_selector.samples[i].regions[j].censored ||
				data_selector.samples[i].regions[j].excluded)
				continue;
			found_regions = true;
			if (image_server.verbose_debug_output())
				cout << "\nConsidering " << data_selector.samples[i].sample_name << "::" << data_selector.samples[i].regions[j].region_name << "\n";
			try {

				ns_time_series_denoising_parameters::ns_movement_score_normalization_type norm_type[1] =
				{ ns_time_series_denoising_parameters::ns_none };

				for (unsigned int k = 0; k < 1; k++) {
					const unsigned long region_id(data_selector.samples[i].regions[j].region_id);


					ns_time_path_solution time_path_solution;
					time_path_solution.load_from_db(region_id, sql, true);

					ns_time_series_denoising_parameters denoising_parameters(ns_time_series_denoising_parameters::load_from_db(region_id, sql));
					denoising_parameters.movement_score_normalization = norm_type[k];

					ns_time_path_image_movement_analyzer time_path_image_analyzer;
					ns_image_server::ns_posture_analysis_model_cache::const_handle_t handle;
					image_server.get_posture_analysis_model_for_region(region_id, handle, sql);
					ns_posture_analysis_model mod(handle().model_specification);
					mod.threshold_parameters.use_v1_movement_score = software_version_number == 1;
					handle.release();
					ns_acquire_for_scope<ns_analyzed_image_time_path_death_time_estimator> death_time_estimator(

						ns_get_death_time_estimator_from_posture_analysis_model(mod));

					time_path_image_analyzer.load_completed_analysis(region_id, time_path_solution, denoising_parameters, &death_time_estimator(), sql);
					death_time_estimator.release();
					ns_region_metadata metadata;

					try {
						ns_hand_annotation_loader by_hand_region_annotations;
						metadata = by_hand_region_annotations.load_region_annotations(ns_death_time_annotation_set::ns_censoring_and_movement_transitions, region_id, sql);
						time_path_image_analyzer.add_by_hand_annotations(by_hand_region_annotations.annotations);

					}
					catch (ns_ex & ex) {
						cerr << ex.text();
						metadata.load_from_db(region_id, "", sql);
					}

					if (run_posture)
						posture_analysis_optimization_output() << "\n";
					if (run_expansion)
						expansion_analysis_optimization_output() << "\n";
					cerr << metadata.plate_name() << "\n";

					//get current parameters
					ns_image_server::ns_posture_analysis_model_cache::const_handle_t posture_analysis_model_handle;
					image_server.get_posture_analysis_model_for_region(region_id, posture_analysis_model_handle, sql);

					if (run_posture) {
						map<std::string, ns_parameter_set_optimization_record>::iterator p = best_posture_parameter_sets.find(data_selector.samples[i].regions[j].region_metadata->plate_type_summary());
						if (p == best_posture_parameter_sets.end()) {
							if (image_server.verbose_debug_output())
								cout << "Creating record for plate type " << data_selector.samples[i].regions[j].region_metadata->plate_type_summary() << "\n";


							p = best_posture_parameter_sets.insert(best_posture_parameter_sets.begin(),
								std::pair<std::string, ns_parameter_set_optimization_record>(data_selector.samples[i].regions[j].region_metadata->plate_type_summary(),
									ns_parameter_set_optimization_record(posture_analysis_thresholds, posture_analysis_hold_times)));
							p->second.best = posture_analysis_model_handle().model_specification.threshold_parameters;   //set default parameters
						}
						else {

							if (image_server.verbose_debug_output())
								cout << "Adding to record for plate type " << data_selector.samples[i].regions[j].region_metadata->plate_type_summary() << "\n";
						}

						time_path_image_analyzer.write_posture_analysis_optimization_data(software_version_number, posture_analysis_thresholds, posture_analysis_hold_times, metadata, posture_analysis_optimization_output(), p->second.new_parameters_results);


						vector<double> thresh;
						vector<unsigned long>hold_t;
						thresh.push_back(posture_analysis_model_handle().model_specification.threshold_parameters.stationary_cutoff);
						hold_t.push_back(posture_analysis_model_handle().model_specification.threshold_parameters.permanance_time_required_in_seconds);
						time_path_image_analyzer.write_posture_analysis_optimization_data(software_version_number, thresh, hold_t, metadata, posture_analysis_optimization_output(), p->second.current_parameter_results);
					}
					if (run_expansion) {
						map<std::string, ns_parameter_set_optimization_record>::iterator p = best_expansion_parameter_sets.find(data_selector.samples[i].regions[j].region_metadata->plate_type_summary());
						if (p == best_expansion_parameter_sets.end()) {
							p = best_expansion_parameter_sets.insert(best_expansion_parameter_sets.begin(), std::pair<std::string, ns_parameter_set_optimization_record>(data_selector.samples[i].regions[j].region_metadata->plate_type_summary(), ns_parameter_set_optimization_record(posture_analysis_thresholds, posture_analysis_hold_times)));
							p->second.best = posture_analysis_model_handle().model_specification.threshold_parameters;   //set default parameters
						}
						time_path_image_analyzer.write_expansion_analysis_optimization_data(expansion_analysis_thresholds, expansion_analysis_hold_times, metadata, expansion_analysis_optimization_output(), p->second.new_parameters_results);


						vector<double> thresh;
						vector<unsigned long>hold_t;
						thresh.push_back(posture_analysis_model_handle().model_specification.threshold_parameters.death_time_expansion_cutoff);
						hold_t.push_back(posture_analysis_model_handle().model_specification.threshold_parameters.death_time_expansion_time_kernel_in_seconds);
						time_path_image_analyzer.write_expansion_analysis_optimization_data(thresh, hold_t, metadata, expansion_analysis_optimization_output(), p->second.current_parameter_results);
					}

				}

			}
			catch (ns_ex & ex) {
				std::cerr << "\n" << ex.text() << "\n";
			}
		}
		if (!found_regions && image_server.verbose_debug_output())
			cout << data_selector.samples[i].sample_name << " did not contain any valid plates.  Any that exist have been excluded or censored.\n";
		//if (i>5)
	//	break;
	}

	std::string results_text;
	for (unsigned int i = 0; i < 2; i++) {
		map<std::string, ns_parameter_set_optimization_record> *best_parameter_sets;
		vector<double> * thresh;
		vector<unsigned long> * hold;
		std::string data_name;
		if (i == 0) {
			if (!run_posture)
				continue;
			data_name = "Posture Analysis";
			best_parameter_sets = &best_posture_parameter_sets;
			thresh = &posture_analysis_thresholds;
			hold = &posture_analysis_hold_times;
		}
		else {
			if (!run_expansion)
				continue; 
			data_name = "Expansion Analysis";
			best_parameter_sets = &best_expansion_parameter_sets;
			thresh = &expansion_analysis_thresholds;
			hold = &expansion_analysis_hold_times;
		}

		results_text += "===Automated " + data_name + " Calibration Results == \n";
		results_text += "Calculated at " + ns_format_time_string_for_human(ns_current_time()) + "\n\n";
		for (map<std::string, ns_parameter_set_optimization_record>::iterator p = best_parameter_sets->begin(); p != best_parameter_sets->end(); p++) {

			results_text += "**For plates of type " + p->first + "\n";		
			p->second.find_best_parameter_set(*thresh, *hold, data_name=="Posture Analysis");
			if (p->second.new_parameters_results.number_valid_worms == 0) {

				results_text += "No worms were annotated by hand.\n\n";
				continue;
			}
			if (p->second.total_count == 0) {
				results_text += "No parameter sets converged on a solution.\n\n";
				continue;
			}
	

			p->second.filename= p->first;
			if (image_server.verbose_debug_output()) 
				cout << "Filename " << p->second.filename;
			
			for (unsigned int i = 0; i < p->second.filename.size(); i++) 
				if (!isalnum(p->second.filename[i]))
					p->second.filename[i] = '_';
			
			if (image_server.verbose_debug_output())
				cout << " was fixed to " << p->second.filename << "\n";

			const bool current_parameters_converged(p->second.current_parameter_results.counts[0][0] > 0);
			const bool best_parameters_converged(p->second.best_parameter_count > 0);
			const double current_msqerr = (p->second.current_parameter_results.death_total_mean_square_error_in_hours[0][0] / p->second.current_parameter_results.counts[0][0]);
			const double best_msqerr = p->second.best_mean_squared_error;

			if (!current_parameters_converged)
				results_text += "The existing parameter set never converged on a solution.\n";
			else {
				results_text += "The existing parameter set converged on a solution in " + ns_to_string_short((100.0 * p->second.current_parameter_results.counts[0][0]) / p->second.current_parameter_results.number_valid_worms,1) + "% of cases\n";
				results_text += "The existing parameter set produced estimates that differed from by - hand annotations by\n " + ns_to_string_short(sqrt(current_msqerr), 3) + " days on average(a mean squared error of " + ns_to_string_short(current_msqerr, 3) + " days squared)\n";
			}
			if (!best_parameters_converged)
				results_text += "The best possible parameter set never converged on a solution";
			else {
				results_text += "The best possible parameter set converged on a solution in " + ns_to_string_short((100.0 * p->second.best_parameter_count) / p->second.current_parameter_results.number_valid_worms, 1) + "% of cases\n";
				results_text += "The best possible parameter set produced estimates that differed from by-hand annotations by\n" + ns_to_string_short(sqrt(best_msqerr), 3) + " days on average (a mean squared error of " + ns_to_string_short(best_msqerr, 3) + " days squared)\n";
			}
			bool enough_worms = p->second.new_parameters_results.number_valid_worms >= 50;
			bool enough_convergences = 1.25*p->second.best_parameter_count >=p->second.new_parameters_results.number_valid_worms;
			if (!enough_worms)
				results_text += "Only " + ns_to_string(p->second.best_parameter_count) + " individuals were annotated by hand.\nThese results will not be meaningful until you annotate more individuals.\n";
			else if (!enough_convergences) 
				results_text += "The best parameter set did not converge in most cases.\nThese parameters are not recommended\n";
			else results_text += ns_to_string(p->second.best_parameter_count) + " individuals were annotated by hand to produce these estimates.\n";
			
			
			if (enough_convergences && enough_worms && current_parameters_converged && best_parameters_converged) {
				bool substantial_improvement = current_msqerr <= .8*best_msqerr;
				if (enough_worms && substantial_improvement)
					results_text += "A new parameter file named " + p->second.filename + " has been written to disk.\n  It is recommended that you use this model for subsequent analysis of this type of animals.\n";
			}
			results_text += "\n";
		}
	}

	//output best parameters to disk
	if (run_posture && run_expansion) {
		for (map<std::string, ns_parameter_set_optimization_record>::iterator expansion_p = best_expansion_parameter_sets.begin(); expansion_p != best_expansion_parameter_sets.end(); expansion_p++) {
			//build parameter file with optimal posture and expansion parameters
			map<std::string, ns_parameter_set_optimization_record>::iterator posture_p = best_posture_parameter_sets.find(expansion_p->first);
			if (posture_p == best_posture_parameter_sets.end())
				throw ns_ex("Could not find expansion index");
			expansion_p->second.best.permanance_time_required_in_seconds = posture_p->second.best.permanance_time_required_in_seconds;
			expansion_p->second.best.posture_cutoff = posture_p->second.best.posture_cutoff;
			expansion_p->second.best.stationary_cutoff = posture_p->second.best.stationary_cutoff;
			ns_acquire_for_scope<ostream> both_parameter_set(image_server.results_storage.optimized_posture_analysis_parameter_set(sub, expansion_p->second.filename, sql).output());
			expansion_p->second.best.write(both_parameter_set());
			both_parameter_set.release();
		}
	}
	else {
		map<std::string, ns_parameter_set_optimization_record> * best_parameter_sets;
		if (run_posture) {
			best_parameter_sets = &best_posture_parameter_sets;
		}
		else if (run_expansion) {
			best_parameter_sets = &best_expansion_parameter_sets;
		}	
		for (map<std::string, ns_parameter_set_optimization_record>::iterator p = best_parameter_sets->begin(); p != best_parameter_sets->end(); p++) {
			ns_acquire_for_scope<ostream> parameter_set_output(image_server.results_storage.optimized_posture_analysis_parameter_set(sub, p->second.filename, sql).output());
			p->second.best.write(parameter_set_output());
			parameter_set_output.release();
		}
	}



	ns_acquire_for_scope<ostream> summary(image_server.results_storage.optimized_posture_analysis_parameter_set(sub, "optimization_summary", sql).output());
	summary() << results_text;
	summary.release();
	ns_text_dialog td;
	td.grid_text.push_back(results_text);
	td.title = "Results";
	td.w = 1000;
	td.h = 400;
	ns_run_in_main_thread_custom_wait<ns_text_dialog> b(&td);

}

void ns_write_emperical_posture_model(const std::string & path_and_base_filename, const std::string &experiment_name,const std::string & strain,ns_emperical_posture_quantification_value_estimator & e){
	
	ofstream sample_file((path_and_base_filename + "="+strain+"=samples.csv").c_str());
	e.write_samples(sample_file,experiment_name);
	sample_file.close();
	e.generate_estimators_from_samples();
	ofstream moving_file((path_and_base_filename + "="+strain+ "=model_moving.csv").c_str());
	ofstream dead_file((path_and_base_filename + "="+strain+ "=model_dead.csv").c_str());
	ofstream visualization_file((path_and_base_filename + "="+strain+ "=model_visualization.csv").c_str());
	e.write(moving_file,dead_file, &visualization_file,experiment_name);
	moving_file.close();
	dead_file.close();
	visualization_file.close();

}

void ns_worm_learner::generate_experiment_movement_image_quantification_analysis_data(ns_movement_quantification_type  detail_level){
	unsigned int experiment_id = data_selector.current_experiment_id();
	const std::string experiment_name(data_selector.current_experiment_name());
	ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__,__LINE__));
	
	ns_image_server_results_subject sub_e;
	sub_e.experiment_id = experiment_id;
	ns_acquire_for_scope<ostream> o_all;
	
	ns_region_metadata m;

	if (detail_level == ns_quantification_summary){
		throw ns_ex("No longer implemented");
		//o_all.attach(image_server.results_storage.time_path_image_analysis_quantification(sub_e,"summary",true,sql()).output());
	//	ns_analyzed_image_time_path::write_summary_movement_quantification_analysis_header(o_all());
	//	o_all() <<"\n";
	}
	else if (detail_level == ns_quantification_detailed || detail_level == ns_quantification_abbreviated_detailed){
		std::string suffix;
		if (data_selector.strain_selected()){
			m = data_selector.current_strain();
			suffix = "_";
			suffix += m.plate_type_summary();
			for (unsigned int i = 0; i < suffix.size(); i++){
				if (suffix[i] == ':')
					suffix[i] = '_';
			}
		}
		o_all.attach(image_server.results_storage.time_path_image_analysis_quantification(sub_e,"detailed" + suffix,true,sql(),detail_level==ns_quantification_abbreviated_detailed,false).output());
	//	ns_analyzed_image_time_path::write_summary_movement_quantification_analysis_header(o_all());
		//o_all() <<"\n";

	}
	//sorted by strain
	std::map<string,ns_emperical_posture_quantification_value_estimator> value_estimators;
	ns_emperical_posture_quantification_value_estimator aggregate_value_estimator;
	if (detail_level == ns_quantification_detailed){
		bool header_written(false);
		//if we're loading detailed information, there is so much data we load it from files created during movement analysis.
		sql() << "SELECT r.id, r.name, s.name FROM sample_region_image_info as r, capture_samples as s WHERE r.sample_id = s.id AND s.experiment_id = " << experiment_id
			  << " AND r.censored = 0 AND r.excluded_from_analysis = 0 AND s.censored = 0 AND s.excluded_from_analysis=0";
		ns_sql_result res;
		sql().get_rows(res);
		for (unsigned int i = 0; i < res.size(); i++){
			cerr << (100*i)/res.size() << "%...";
			ns_image_server_results_subject sub;
			sub.region_id = atol(res[i][0].c_str());
			if (data_selector.strain_selected()){
				ns_region_metadata r_m;
				r_m.load_from_db(sub.region_id,"",sql());
				if (!r_m.matches(ns_region_metadata::ns_strain_and_conditions_1_2_and_3,m))
					continue;
			}
			ns_acquire_for_scope<istream> in(image_server.results_storage.time_path_image_analysis_quantification(sub,"detailed",false,sql(),detail_level==ns_quantification_abbreviated_detailed,false).input());
			if (in.is_null()){
				cerr << "Could not load cached movement quantification analysis for " << res[i][2] << "::" << res[i][1] << "\n";
				continue;
			}
			//remove header
			char a(0);
			if (header_written){
				while(a!='\n' && !in().fail())
					a = in().get();
				if (in().fail())
					continue;
				header_written = true;
			}
			while(true){
				char a(in().get());
				if (in().fail())
					break;
				o_all().put(a);
			}
			if (o_all().fail())
				throw ns_ex("Error while writing output");
			in.release();
		}
	}
	else if (detail_level == ns_quantification_summary  || 
		detail_level == ns_quantification_detailed_with_by_hand ||  
		detail_level == ns_build_worm_markov_posture_model_from_by_hand_annotations || 
		detail_level == ns_quantification_abbreviated_detailed){		
		//since there is less data here, we calculate it on the fly.
		bool header_written(false);
		movement_results.load(ns_death_time_annotation_set::ns_censoring_and_movement_transitions,0,0,experiment_id,sql(),false);
		unsigned long region_count(0);
		for (unsigned int i = 0; i < movement_results.samples.size(); i++)
			region_count+=movement_results.samples[i].regions.size();

		unsigned long regions_processed(0);

		for (unsigned int i = 0; i < movement_results.samples.size(); i++){
			for (unsigned int j = 0; j < movement_results.samples[i].regions.size(); j++){
				cerr << 100*regions_processed/region_count << "%";
				regions_processed++;

				ns_hand_annotation_loader by_hand_annotations;
				by_hand_annotations.load_region_annotations(ns_death_time_annotation_set::ns_censoring_and_movement_transitions,
															movement_results.samples[i].regions[j].metadata.region_id,
															experiment_id,
															movement_results.samples[i].regions[j].metadata.experiment_name,
															movement_results.samples[i].regions[j].metadata,
															sql());
				if (detail_level == ns_quantification_detailed_with_by_hand ||  
					detail_level == ns_build_worm_markov_posture_model_from_by_hand_annotations){
					//skip regions without by hand movement annotations
					if (by_hand_annotations.annotations.regions.empty())
						continue;
					if (by_hand_annotations.annotations.regions.begin()->second.locations.empty())
						continue;
					bool found_hand_movement_annotation(false);
					for (unsigned int i = 0; i < by_hand_annotations.annotations.regions.begin()->second.locations.size() && !found_hand_movement_annotation; i++){
						for (unsigned int j = 0; j < by_hand_annotations.annotations.regions.begin()->second.locations[i].annotations.size(); j++){
							if (by_hand_annotations.annotations.regions.begin()->second.locations[i].annotations[j].annotation_source != ns_death_time_annotation::ns_lifespan_machine
								&&
								(
								by_hand_annotations.annotations.regions.begin()->second.locations[i].annotations[j].type == ns_movement_cessation ||
								by_hand_annotations.annotations.regions.begin()->second.locations[i].annotations[j].type == ns_fast_movement_cessation ||
								by_hand_annotations.annotations.regions.begin()->second.locations[i].annotations[j].type == ns_translation_cessation
								)
							){
								found_hand_movement_annotation = true;
								break;
							}
						}
					}
					if (!found_hand_movement_annotation)
						continue;
				}
				movement_results.samples[i].regions[j].time_path_solution.load_from_db(movement_results.samples[i].regions[j].metadata.region_id,sql(),true);
				ns_posture_analysis_model dummy_model(ns_posture_analysis_model::dummy());
				const ns_posture_analysis_model * posture_analysis_model(&dummy_model); 
				ns_image_server::ns_posture_analysis_model_cache::const_handle_t handle;
				if (detail_level != ns_build_worm_markov_posture_model_from_by_hand_annotations){
					image_server.get_posture_analysis_model_for_region(movement_results.samples[i].regions[j].metadata.region_id, handle, sql());
					posture_analysis_model = &handle().model_specification;
				}
				ns_acquire_for_scope<ns_analyzed_image_time_path_death_time_estimator> death_time_estimator(
					ns_get_death_time_estimator_from_posture_analysis_model(
					handle().model_specification));
				const ns_time_series_denoising_parameters time_series_denoising_parameters(ns_time_series_denoising_parameters::load_from_db(movement_results.samples[i].regions[j].metadata.region_id,sql()));

				movement_results.samples[i].regions[j].time_path_image_analyzer.load_completed_analysis(
					movement_results.samples[i].regions[j].metadata.region_id,
					movement_results.samples[i].regions[j].time_path_solution,
					time_series_denoising_parameters,
					&death_time_estimator(),
					sql(),
					false);
				death_time_estimator.release();
				movement_results.samples[i].regions[j].time_path_image_analyzer.add_by_hand_annotations(by_hand_annotations.annotations);
				if (detail_level==ns_quantification_abbreviated_detailed){
					if (!header_written){
						o_all.attach(image_server.results_storage.time_path_image_analysis_quantification(sub_e,"detailed",true,sql(),true).output());
						if (movement_results.samples[i].regions[j].time_path_image_analyzer.size() > 0){
							movement_results.samples[i].regions[j].time_path_image_analyzer.group(0).paths[0].write_detailed_movement_quantification_analysis_header(o_all());
							o_all() <<"\n";
						}
						header_written = true;
					}
			
					movement_results.samples[i].regions[j].time_path_image_analyzer.write_detailed_movement_quantification_analysis_data(
																			movement_results.samples[i].regions[j].metadata,o_all(),false,-1,true);
				
				}
				else if (detail_level == ns_quantification_detailed_with_by_hand){
				
					if (!header_written){
						o_all.attach(image_server.results_storage.time_path_image_analysis_quantification(sub_e,"detailed_with_by_hand",true,sql()).output());
						if (movement_results.samples[i].regions[j].time_path_image_analyzer.size() > 0){
							movement_results.samples[i].regions[j].time_path_image_analyzer.group(0).paths[0].write_detailed_movement_quantification_analysis_header(o_all());
							o_all() <<"\n";
						}
						header_written = true;
					}
			
					movement_results.samples[i].regions[j].time_path_image_analyzer.write_detailed_movement_quantification_analysis_data(
																	movement_results.samples[i].regions[j].metadata,o_all(),(detail_level == ns_quantification_detailed_with_by_hand));
				}
				else if (detail_level == ns_build_worm_markov_posture_model_from_by_hand_annotations){
					bool added_data(false);
					for (unsigned int g = 0; g < movement_results.samples[i].regions[j].time_path_image_analyzer.size(); g++){
						for (unsigned int p = 0; p < movement_results.samples[i].regions[j].time_path_image_analyzer.group(g).paths.size(); p++){
							aggregate_value_estimator.add_by_hand_data_to_sample_set(2,&movement_results.samples[i].regions[j].time_path_image_analyzer.group(g).paths[p]);
							value_estimators[movement_results.samples[i].regions[j].metadata.device_regression_match_description()].add_by_hand_data_to_sample_set(2, &movement_results.samples[i].regions[j].time_path_image_analyzer.group(g).paths[p]);
					
						}
					}
				//	if (added_data)
				//	break;
				}
				else{
					throw ns_ex("No longer implemented");
					//movement_results.samples[i].regions[j].time_path_image_analyzer.write_summary_movement_quantification_analysis_data(
				//													movement_results.samples[i].regions[j].metadata,o_all());
				}
				movement_results.samples[i].regions[j].time_path_image_analyzer.clear();
			}
		}
	}	
	if (detail_level == ns_build_worm_markov_posture_model_from_by_hand_annotations){
		//TODO: Surely there's a better place to write these files?
		#ifdef _WIN32
		std::string base_path("c:\\");
		#else
		std::string base_path("/");
		#endif
		base_path+=experiment_name;
		for (std::map<string,ns_emperical_posture_quantification_value_estimator>::iterator p = value_estimators.begin(); p!=value_estimators.end(); p++){
			if (!p->second.raw_moving_cdf.samples().empty() &&
				!p->second.dead_cdf.samples().empty())
			ns_write_emperical_posture_model(base_path,p->first,experiment_name,p->second);
		}
		ns_write_emperical_posture_model(base_path,"all",experiment_name,aggregate_value_estimator);
	}
	
	o_all.release();
	sql.release();
}

void ns_find_new_paths(const ns_time_path_solution & baseline, const ns_time_path_solution & target, vector<char> & target_path_is_new_this_round){
	
	target_path_is_new_this_round.resize(0);
	target_path_is_new_this_round.resize(target.paths.size(),0);
	
	hungarian_problem_t matching_problem;
	int ** cost_matrix = new int *[baseline.paths.size()];
	for (unsigned int i = 0; i < baseline.paths.size(); i++)
		cost_matrix[i] = new int[target.paths.size()];
	try{
		for (unsigned int i = 0; i < baseline.paths.size(); i++){
			for (unsigned int j = 0; j < target.paths.size(); j++){
				const unsigned long d((baseline.paths[i].center/baseline.paths[i].stationary_elements.size()-target.paths[j].center/target.paths[j].stationary_elements.size()).squared());
					cost_matrix[i][j] = d;
			}
		}

		hungarian_init(&matching_problem,cost_matrix,
			baseline.paths.size(),//rows
			target.paths.size(),	//columns
			HUNGARIAN_MODE_MINIMIZE_COST);

		hungarian_solve(&matching_problem);
		
		int k = 0;
		for (unsigned int j = 0; j < target.paths.size(); j++){
			bool found(false);
			for (unsigned int i = 0; i < baseline.paths.size(); i++){
				if (matching_problem.assignment[i][j] != HUNGARIAN_ASSIGNED)
					continue;
				found = true;
				break;
			}
			if (!found){
				target_path_is_new_this_round[j] = 1;
			}
			k++;
		}
		hungarian_free(&matching_problem);
		for (unsigned int i = 0; i < baseline.paths.size(); i++)
			delete[] cost_matrix[i];
		delete[] cost_matrix;
	}
	catch(...){
		for (unsigned int i = 0; i < baseline.paths.size(); i++)
			delete[] cost_matrix[i];
		delete[] cost_matrix;
		throw;
	}
}
const unsigned long ns_location_not_matched(99999);
void ns_match_locations(const std::vector<ns_vector_2i> baseline, const std::vector<ns_vector_2i> & target, vector<unsigned long> & target_match){
	
	target_match.resize(0);
	target_match.resize(target.size(),ns_location_not_matched);
	
	hungarian_problem_t matching_problem;
	int ** cost_matrix = new int *[baseline.size()];
	for (unsigned int i = 0; i < baseline.size(); i++)
		cost_matrix[i] = new int[target.size()];
	try{
		for (unsigned int i = 0; i < baseline.size(); i++){
			for (unsigned int j = 0; j < target.size(); j++){
					cost_matrix[i][j] = (baseline[i]-target[j]).squared();
			}
		}

		hungarian_init(&matching_problem,cost_matrix,
			baseline.size(),//rows
			target.size(),	//columns
			HUNGARIAN_MODE_MINIMIZE_COST);

		hungarian_solve(&matching_problem);
		
		int k = 0;
		for (unsigned int j = 0; j < target.size(); j++){
			bool found(false);
			for (unsigned int i = 0; i < baseline.size(); i++){
				if (matching_problem.assignment[i][j] != HUNGARIAN_ASSIGNED)
					continue;
				found = true;
				target_match[j] = i;
				break;
			}
			if (!found)
				target_match[j] = ns_location_not_matched;
			k++;
		}
		hungarian_free(&matching_problem);
		for (unsigned int i = 0; i < baseline.size(); i++)
			delete[] cost_matrix[i];
		delete[] cost_matrix;
	}
	catch(...){
		for (unsigned int i = 0; i < baseline.size(); i++)
			delete[] cost_matrix[i];
		delete[] cost_matrix;
		throw;
	}
}

void ns_worm_learner::save_current_area_selections(){
		string device_name = ns_extract_scanner_name_from_filename(get_current_clipboard_filename());
		string default_filename = ns_format_time_string(ns_current_time()) + "=" + device_name + "=sample_regions.txt";
		
		ns_image_file_chooser im_cc;
		im_cc.save_file();
		im_cc.default_filename = default_filename;
		ns_file_chooser_file_type t;
		im_cc.filters.clear();
		im_cc.filters.push_back(ns_file_chooser_file_type("Text","txt"));
		ns_run_in_main_thread<ns_image_file_chooser> run_mt(&im_cc);
		if (im_cc.chosen)
			output_area_info(im_cc.result);
	}



struct ns_deduced_sample{
	ns_deduced_sample():sample_width_in_pixels(0),id(0),mask_image_id(0),mask_width_in_pixels(0),device_capture_period_in_seconds(0),number_of_consecutive_captures_per_sample(0){}
	std::string name,
			    device,
				incubator_name,
				incubator_location,
				region_mask_filename;
	unsigned long device_capture_period_in_seconds,
				number_of_consecutive_captures_per_sample,
				sample_width_in_pixels,
				mask_width_in_pixels;
	ns_64_bit id,
			  mask_image_id,
			  mask_image_record_id,
			  mask_id;
	void get_device_info(ns_sql & sql){
		if (name.empty())
			throw ns_ex("No name specified");
		std::string::size_type pos = name.find_last_of("_");
		if (pos == name.npos|| pos != name.size()-2)
			throw ns_ex("Sample name") << name << " doesn't have an underscore in the correct place.";
		device = name.substr(0,name.size()-2);
		sql << "SELECT incubator_name, incubator_location FROM device_inventory WHERE device_name = \"" << device << "\"";
		ns_sql_result res;
		sql.get_rows(res);
		if (res.size() == 0)
			return;
		incubator_name = res[0][0];
		incubator_location = res[0][1];
	}
	void resubmit_mask_for_processing(bool schedule_job,ns_sql & sql){
		mask_image_record_id = image_server.make_record_for_new_sample_mask(id,sql);

		sql << "INSERT INTO image_masks SET image_id = " << mask_image_id << ", processed='0',resize_factor=" << (sample_width_in_pixels/mask_width_in_pixels);
		mask_id = sql.send_query_get_id();
		
		sql << "UPDATE capture_samples SET mask_id=" << mask_id << " WHERE id=" << id;
		sql.send_query();
		if (schedule_job){
			sql << "INSERT INTO processing_jobs SET image_id=" << mask_image_record_id << ", mask_id=" << mask_id << ", "
					<< "op" << (unsigned int)ns_process_analyze_mask<< " = 1, time_submitted=" << ns_current_time() << ", urgent=1";
			sql.send_query();
		}
		sql.send_query("COMMIT");
	}
};


void ns_explode_slist(const std::string & s, vector<std::string> & vals){
	string cur;
	for (unsigned int i = 0; i < s.size(); i++){
			if (s[i] == DIR_CHAR){
				vals.push_back(cur);
				cur.resize(0);
			}
			else cur+=s[i];
		}
		if (cur.size() != 0)
			vals.push_back(cur);
};

struct ns_capture_image_d{
	ns_capture_image_d(const ns_64_bit & c, const ns_64_bit & i):captured_image_id(c),image_id(i){}
	ns_capture_image_d():captured_image_id(0),image_id(0){}
	ns_64_bit captured_image_id, image_id;
};

struct ns_interval_histogram{
	ns_interval_histogram(long i,unsigned c):interval(i),count(c){}
	ns_interval_histogram():interval(0),count(0){}
	long interval;
	unsigned long count;
};
bool ns_interval_histogram_less(const ns_interval_histogram & a, const ns_interval_histogram & b){
	return a.count < b.count;
}

ns_64_bit ns_worm_learner::create_experiment_from_directory_structure(const std::string & directory_name, const bool process_masks_locally){
	std::string dname(directory_name),lname(image_server.long_term_storage_directory);

	ns_to_lower(dname);
	ns_to_lower(lname);

	std::string::size_type pos = dname.find_first_of(lname);

	if (pos == dname.npos)
		throw ns_ex("The specified directory does not seem to be a subdirectory of the long term storage directory");
	
	std::string experiment_name_and_partition = directory_name.substr(lname.size()+1);
	if (experiment_name_and_partition.size() == 0)
		throw ns_ex("The specified directory does not seem to be a subdirectory of the long term storage directory");
	std::vector<std::string> name_split;
	ns_explode_slist(experiment_name_and_partition,name_split);
	
	std::string experiment_name,experiment_partition;

	if (name_split.size() > 2){
		throw ns_ex("The specified directory does not seem to be at the right depth in the directory tree");
	}
	else if (name_split.size() == 2){
		experiment_partition = name_split[0];
		experiment_name = name_split[1];
	}
	else experiment_name = name_split[0];

	ns_experiment_capture_specification::confirm_valid_name(experiment_name);

	std::string experiment_base_path(image_server.long_term_storage_directory);
	if (experiment_partition.size() > 0){
		 experiment_base_path += DIR_CHAR_STR;
		 experiment_base_path += experiment_partition;
	}
	std::string experiment_path = experiment_base_path + DIR_CHAR_STR +  experiment_name;

	ns_dir dir(experiment_path);
	std::vector<ns_deduced_sample> samples;

	ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__,__LINE__));
	std::sort(dir.dirs.begin(),dir.dirs.end());
	for (unsigned long i = 0; i < dir.dirs.size(); i++){
		if (dir.dirs[i] != "." &&
			dir.dirs[i] != ".." &&
			dir.dirs[i] != "video" &&
			dir.dirs[i] != "animal_storyboard" &&
			dir.dirs[i] != "region_masks"){
			samples.resize(samples.size()+1);
			samples.rbegin()->name = dir.dirs[i];
			samples.rbegin()->get_device_info(sql());
		}

	}

	if (samples.empty())
		throw ns_ex("The specified directory does not seem to have any valid sample subdirectories");

	sql() << "SELECT id FROM experiments WHERE name = \"" <<experiment_name << "\"";
	ns_sql_result res;
	sql().get_rows(res);
	if(res.size() != 0)
		throw ns_ex("An experiment with the name ") << experiment_name << " already exists!";

	sql() << "INSERT INTO experiments SET name='" << sql().escape_string(experiment_name) << "',description='',`partition`='" 
		  << sql().escape_string(experiment_partition) << "', time_stamp=0";

	const ns_64_bit experiment_id = sql().send_query_get_id();

	for (unsigned long i = 0; i < samples.size(); i++){
		cerr << "Creating record for sample " << samples[i].name << "\n";
		sql() << "INSERT INTO capture_samples SET experiment_id = " << ns_to_string(experiment_id) << ",name='" << sql().escape_string(samples[i].name) << "'"
				<< ",device_name='" << sql().escape_string(samples[i].device) << "',parameters=''"
				<< ",position_x=0 ,position_y=0"
				<< ",size_x=0,size_y=0"
				<< ",incubator_name='" << sql().escape_string(samples[i].incubator_name) 
				<< "',incubator_location='" <<  sql().escape_string(samples[i].incubator_location)
				<< "',desired_capture_duration_in_seconds=0"
				<< ",description='',model_filename='',reason_censored='',image_resolution_dpi='3200'"
				<< ",device_capture_period_in_seconds=0"
				<< ",number_of_consecutive_captures_per_sample=0"
				<< ", time_stamp=0";
		samples[i].id = sql().send_query_get_id();
	}
	
	for (unsigned long i = 0; i < samples.size(); i++){
		cerr << "Obtaining image width and processing mask for sample " << samples[i].name << "\n";
		//get image size
		std::string unprocessed_image_directory(experiment_base_path + DIR_CHAR_STR + experiment_name + DIR_CHAR_STR + samples[i].name + DIR_CHAR_STR + "captured_images");
		ns_dir dir;
		std::vector<std::string> files;
		dir.load_masked(unprocessed_image_directory,"tif",files);
		std::vector<unsigned long> sample_times;
		sample_times.reserve(files.size());
		for (unsigned long j = 0; j < files.size(); j++){
			ns_image_server_captured_image im;
			int offset(0);
			try{
				im.from_filename(files[j],offset);
				sample_times.push_back(im.capture_time);
			}catch(...){}
		}
		std::sort(sample_times.begin(),sample_times.end());
		std::map<long,ns_interval_histogram> intervals;
		for (unsigned long j = 1; j < sample_times.size(); j++){
			const long diff = sample_times[j]-sample_times[j-1];
			std::map<long,ns_interval_histogram>::iterator p = intervals.find(diff);
			if (p == intervals.end())
				intervals[diff] = ns_interval_histogram(diff,0);
			else intervals[diff].count++;
		}
		std::vector<ns_interval_histogram> sorted_intervals;
		for (std::map<long,ns_interval_histogram>::const_iterator p = intervals.begin(); p != intervals.end(); p++)
			sorted_intervals.push_back(p->second);
		std::sort(sorted_intervals.rbegin(),sorted_intervals.rend(),ns_interval_histogram_less);
		if (sorted_intervals.size() == 0)
			throw ns_ex("Not enough intervals to deduce spacing");
		unsigned long sample_interval, number_of_consecutive_sample_captures;
		if (sorted_intervals.size() == 1 || sorted_intervals[0].count > (8*sample_times.size())/10){
			sample_interval = sorted_intervals[0].interval;
			number_of_consecutive_sample_captures = 1;
		}
		else{
			unsigned long largest(sorted_intervals[0].interval),
				smallest(sorted_intervals[1].interval);
			ns_swap<unsigned long> s;
			if (smallest > largest)
				swap(smallest,largest);
			number_of_consecutive_sample_captures = largest/smallest;
			sample_interval=smallest;
		}
		if (number_of_consecutive_sample_captures > 2)
			cerr << ("Unusual number of consecutive samples: ") << number_of_consecutive_sample_captures << "\n";
		else if (sample_interval > 30*60*60)
			cerr <<("Unusual sample interval: ")<< sample_interval << "\n";
		else{
			sql() << "UPDATE capture_samples SET number_of_consecutive_captures_per_sample = " << number_of_consecutive_sample_captures 
				  << ", device_capture_period_in_seconds= " << sample_interval
				  << " WHERE id = " << samples[i].id;
			sql().send_query();
		}
		if (files.empty())
			continue;
		std::sort(files.rbegin(),files.rend());
		
		for (unsigned long j =0; j < files.size(); j++){
			try{
				ns_tiff_image_input_file<ns_8_bit> tiff_in;
				tiff_in.open_file(unprocessed_image_directory+DIR_CHAR_STR + files[j]);
				samples[i].sample_width_in_pixels = tiff_in.properties().width;
				tiff_in.close();
				break;
			}
			catch(ns_ex & ex){
				cerr << ex.text() << "\n";
			}
		}
		if (samples[i].sample_width_in_pixels==0){
			cerr << "Couldn't find a good captured image file for the sample\n";
			continue;
		}
		std::string mask_filename = "mask_" + samples[i].name + ".tif";
		std::string mask_relative_path = experiment_name + DIR_CHAR_STR + "region_masks";
		std::string mask_path(experiment_base_path + DIR_CHAR_STR + mask_relative_path + DIR_CHAR_STR + mask_filename);
		
		if (!ns_dir::file_exists(mask_path)){
			cerr << "The mask file didn't seem to exist\n";
			continue;
		}
		ns_tiff_image_input_file<ns_8_bit> tiff_in;
		tiff_in.open_file(mask_path);
		samples[i].mask_width_in_pixels = tiff_in.properties().width;
		ns_image_server_image im;
		im.filename = mask_filename;
		im.path = mask_relative_path;
		im.partition = experiment_partition;
		im.save_to_db(0,&sql());
		samples[i].mask_image_id = im.id;
		samples[i].resubmit_mask_for_processing(!process_masks_locally,sql());
	}
	ns_image_processing_pipeline pipeline(1024);
	if (process_masks_locally){
		for (unsigned int i = 0; i < samples.size(); i++){
			ns_processing_job job;
			job.image_id = samples[i].mask_image_record_id;
			job.mask_id = samples[i].mask_id;
			ns_processing_job_image_processor processor(job,image_server,pipeline);
			processor.run_job(sql());
		}
	}
	return experiment_id;
}


void ns_worm_learner::rebuild_experiment_samples_from_disk(const ns_64_bit experiment_id){
	ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__,__LINE__));

	sql() << "SELECT name FROM experiments WHERE id = " << experiment_id;
	std::string experiment_name = sql().get_value();

	sql() << "SELECT id,name FROM capture_samples WHERE experiment_id = " << experiment_id;
	ns_sql_result res;
	sql().get_rows(res);
	for (unsigned long i = 0; i < res.size(); i++){
		unsigned long number_of_additions(0);
		cout << "Considering sample " << res[i][1] << "\n";
		const ns_64_bit sample_id = ns_atoi64(res[i][0].c_str());
		const std::string sample_name = res[i][1];
		ns_file_location_specification path(image_server.image_storage.get_path_for_sample(sample_id,&sql()));
		std::string path_str = path.absolute_long_term_filename() + "captured_images";
		if (!ns_dir::file_is_writeable(path_str + DIR_CHAR_STR + "temp.tif"))
			throw ns_ex("The path ") << path_str << " is not writeable.  This is required to update database and rename image filenames accordingly.";
		ns_dir dir;
		std:: vector<std::string> filenames;
		dir.load_masked(path_str,".tif",filenames);
		sql() << "SELECT id, image_id FROM captured_images WHERE sample_id = " << sample_id;
		ns_sql_result res2;
		sql().get_rows(res2);
		map<ns_64_bit,ns_capture_image_d> image_lookup;

		for (unsigned long j = 0; j < res2.size(); j++){
			ns_capture_image_d d(ns_atoi64(res2[j][0].c_str()),ns_atoi64(res2[j][1].c_str()));
			image_lookup[d.captured_image_id] = d;
		}

		for (unsigned long j = 0; j < filenames.size(); j++){
			ns_image_server_captured_image captured_image;
			int offset(0);
			captured_image.from_filename(filenames[j],offset);
			captured_image.never_delete_image = true;
			map<ns_64_bit,ns_capture_image_d>::const_iterator p = image_lookup.find(captured_image.captured_images_id);
			if (p != image_lookup.end()){
				//if a captured image exists with the specified id, check to see if it points to the correct image record
				if (p->second.image_id == captured_image.capture_images_image_id){
					//check to see that the image record has the correct filename
					sql() << "SELECT filename FROM images WHERE id = " << p->second.image_id;
					ns_sql_result res3;
					sql().get_rows(res3);
					if (res3.size() != 0 && res3[0][0] == filenames[j])
						//this is great!  The image already exists and has the right filename.  move on.
						continue;
				}
			}
			number_of_additions++;
			cout << filenames[j] << " appears to be missing a database entry.  A new record will be created.\n";

			ns_image_server_image captured_image_image;
			captured_image_image.capture_time = captured_image.capture_time;
			captured_image_image.partition = image_server.image_storage.get_partition_for_experiment(experiment_id,&sql(),true);
			captured_image_image.filename = filenames[j];
			captured_image_image.path = path_str;
			std::string::size_type pos = captured_image_image.path.find(captured_image_image.partition);
			if (pos== captured_image_image.path.npos)
				throw ns_ex("Incorrect path identified: Images are not in a partition of the long term storage directory");
			std::string relative_path = captured_image_image.path.substr(pos+ captured_image_image.partition.size()+1);
			captured_image_image.path = relative_path;
			captured_image_image.save_to_db(0,&sql(),false);

			captured_image.capture_images_image_id = captured_image_image.id;
			captured_image.specified_16_bit = false;
			captured_image.captured_images_id = 0;
			captured_image.experiment_name = experiment_name;
			captured_image.experiment_id = experiment_id;
			captured_image.sample_id = sample_id;
			captured_image.sample_name = sample_name;
			captured_image.save(&sql());
			std::string new_filename = captured_image.filename(&sql()) + "." + ns_dir::extract_extension(captured_image_image.filename);
			std::string old_filename = captured_image_image.filename;
			bool success(ns_dir::move_file(path_str + DIR_CHAR_STR + old_filename,path_str + DIR_CHAR_STR + new_filename));
			if (!success){
				cerr << "Could not rename " << old_filename << " to " << new_filename << ".  If you run metadata repair on this experiment again, you will get duplicate entries.\n";
				continue;
			}
			captured_image_image.filename = new_filename;
			captured_image_image.save_to_db(captured_image_image.id,&sql());
		}
		if (number_of_additions == 0)
			cout << "No database entries were identified as missing.\n";
	}

	sql.release();
}

struct ns_region_processed_image_disk_info{
	ns_image_server_captured_image_region reg_im;
	ns_image_server_image im;
};

void repair_missing_captured_images(const ns_64_bit experiment_id){
	
	ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__,__LINE__));
	sql() << "SELECT r.id, r.capture_time,ri.id,s.id, r.capture_sample_image_id,s.device_name, s.name,s.experiment_id FROM sample_region_images as r, sample_region_image_info as ri, capture_samples as s WHERE "
			" r.region_info_id = ri.id AND ri.sample_id = s.id AND s.experiment_id = " << experiment_id;
	ns_sql_result res;
	sql().get_rows(res);
	ofstream of("c:\\server\\file_errors.csv");
	of << "region_image_id,time,region_info_id,sample_id,capture_image_id,status\n";
//	vector<ns_image_server_captured_image_region> problem, good;
	for (unsigned long i = 0; i < res.size(); i++){

		ns_image_server_captured_image_region r;
		bool good (r.load_from_db(ns_atoi64(res[i][0].c_str()),&sql()));
		of << res[i][0] << "," << res[i][1] << "," << res[i][2] << "," << res[i][3] << "," << res[i][4] << "," << (good?"good":"bad") << "\n";
		if (!good){
			ns_image_server_captured_image cp(r);
			cp.capture_time = atol(res[i][1].c_str());
			cp.captured_images_id = 0;
			cp.capture_images_image_id = 0;
			cp.device_name = res[i][5];
			cp.experiment_id = atol(res[i][7].c_str());
			cp.capture_images_small_image_id = 0;
			cp.sample_id = atol(res[i][3].c_str());
			cp.sample_name = res[i][6];
			cp.specified_16_bit=false;
			cp.save(&sql());
			r.captured_images_id = cp.captured_images_id;
			r.save(&sql());
		}
	}
	of.close();
	sql.release();
}
		

void ns_worm_learner::rebuild_experiment_regions_from_disk(const ns_64_bit experiment_id){
	ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__,__LINE__));

	sql() << "SELECT name FROM experiments WHERE id = " << experiment_id;
	std::string experiment_name = sql().get_value();

	sql() << "SELECT id,name FROM capture_samples WHERE experiment_id = " << experiment_id;
	ns_sql_result sample_res;
	sql().get_rows(sample_res);
	
	std::vector<ns_processing_task> additional_tasks_to_load;
	//additional_tasks_to_load.push_back(ns_unprocessed);
	additional_tasks_to_load.push_back(ns_process_spatial);
	additional_tasks_to_load.push_back(ns_process_lossy_stretch);
	additional_tasks_to_load.push_back(ns_process_threshold);
	additional_tasks_to_load.push_back(ns_process_thumbnail);
	std::vector<std::map<ns_64_bit,ns_region_processed_image_disk_info> > existing_processed_task_images;
	std::string partition(image_server.image_storage.get_partition_for_experiment(experiment_id,&sql(),true));

	for (unsigned long i = 0; i < sample_res.size(); i++){
		unsigned long number_of_additions(0);
		cout << "Considering sample " << sample_res[i][1] << "\n";
		const ns_64_bit sample_id = ns_atoi64(sample_res[i][0].c_str());
		const std::string sample_name = sample_res[i][1];
		const std::string device_name = sample_name.substr(0,sample_name.size()-2);
		sql() << "SELECT id,name FROM sample_region_image_info WHERE sample_id = "<< sample_id;
		ns_sql_result region_res;
		sql().get_rows(region_res);
		for (unsigned long k = 0; k < region_res.size(); k++){
			existing_processed_task_images.clear();
			existing_processed_task_images.resize(additional_tasks_to_load.size());

			const ns_64_bit region_info_id = ns_atoi64(region_res[k][0].c_str());

			//we need to build a list of all processed images to link them to the main image if they exist
			for (unsigned long j = 0; j < additional_tasks_to_load.size(); j++){
				ns_file_location_specification path2(image_server.image_storage.get_path_for_region(region_info_id,&sql(),additional_tasks_to_load[j]));
				std::string absolute_path(path2.absolute_long_term_filename());
				if (!ns_dir::file_exists(absolute_path))
					continue;
				ns_dir dir;
				std::vector<std::string> filenames;
				if (additional_tasks_to_load[j] == ns_process_thumbnail)
					dir.load_masked(absolute_path,"jpg",filenames);
				else dir.load_masked(absolute_path,"tif",filenames);
				std::string::size_type pos = absolute_path.find(partition);
				if (pos==absolute_path.npos)
					throw ns_ex("Incorrect path identified: Images are not in a partition of the long term storage directory");
				std::string relative_path = absolute_path.substr(pos+partition.size()+1);
				relative_path.resize(relative_path.size()-1);

				for (unsigned long l = 0; l < filenames.size(); l++){
					if (filenames[l] == "" ||
						filenames[l] == "." ||
						filenames[l] == "..")
						continue;
					ns_image_server_captured_image_region region_image;
					try{
						region_image.from_filename(filenames[l]);
						ns_region_processed_image_disk_info dim;
						dim.reg_im = region_image;
						dim.im.capture_time = region_image.capture_time;
						dim.im.partition = partition;
						dim.im.filename = filenames[l];
						dim.im.path = relative_path;

						existing_processed_task_images[j][region_image.region_images_id] = dim;
					}
					catch(...){

					}
				}
			}

			//now we look at each file and either link it up to an existing record or create a new record for it.
			ns_file_location_specification path(image_server.image_storage.get_path_for_region(region_info_id,&sql()));
			std::string absolute_path = path.absolute_long_term_filename();

			if (!ns_dir::file_exists(absolute_path))
				continue;
			if (!ns_dir::file_is_writeable(absolute_path + DIR_CHAR_STR + "temp.tif"))
				throw ns_ex("The path ") << absolute_path << " is not writeable.  This is required to update database and rename image filenames accordingly.";
			std::string::size_type pos = absolute_path.find(partition);
			if (pos==absolute_path.npos)
				throw ns_ex("Incorrect path identified: Images are not in a partition of the long term storage directory");
			std::string relative_path = absolute_path.substr(pos+partition.size()+1);
			relative_path.resize(relative_path.size()-1);
			//find all region images based on the existance of an unproccessed (masked) image
			ns_dir dir;
			std:: vector<std::string> filenames;
			dir.load_masked(absolute_path,".tif",filenames);
			sql() << "SELECT id FROM sample_region_images WHERE region_info_id = " << region_info_id;
			ns_sql_result res2;
			sql().get_rows(res2);
			std::set<ns_64_bit> existing_region_images;

			for (unsigned long j = 0; j < res2.size(); j++)
				existing_region_images.insert(existing_region_images.end(),ns_atoi64(res2[j][0].c_str()));
		
			sql() << "SELECT id, capture_time FROM captured_images WHERE sample_id = " << sample_id;
			sql().get_rows(res2);
			std::map<unsigned long,ns_64_bit> existing_captured_images;
			for (unsigned long j = 0; j < res2.size(); j++){
				const ns_64_bit id(ns_atoi64(res2[j][0].c_str()));
				existing_captured_images[atol(res2[j][1].c_str())] = id;
			}
		

			for (unsigned long j = 0; j < filenames.size(); j++){
				ns_image_server_captured_image_region region_image;
				region_image.from_filename(filenames[j]);
				region_image.region_info_id = region_info_id;
				region_image.device_name = device_name;

				std::set<ns_64_bit>::const_iterator p = existing_region_images.find(region_image.region_images_id);
				if (p != existing_region_images.end())
					continue;
				
				number_of_additions++;
				cout << filenames[j] << " appears to be missing a database entry.  A new record will be created.\n";

				ns_image_server_captured_image cap_im(region_image);
				std::map<unsigned long,ns_64_bit>::const_iterator p2 = existing_captured_images.find(cap_im.capture_time);
				if (p2 == existing_captured_images.end()){
					cout << "Creating a capture sample image record for the region image.\n";
					ns_64_bit old_im_id(cap_im.captured_images_id);
					cap_im.sample_id = sample_id;
					cap_im.experiment_id = experiment_id;
					cap_im.captured_images_id = 0;
					cap_im.save(&sql());
					region_image.captured_images_id = cap_im.captured_images_id;
					existing_captured_images[cap_im.capture_time] = cap_im.captured_images_id;
				}
				else region_image.captured_images_id = p2->second;
				
				ns_image_server_image unprocessed_image;
				unprocessed_image.capture_time = region_image.capture_time;
				unprocessed_image.partition = partition;
				unprocessed_image.filename = filenames[j];;
				unprocessed_image.path = relative_path;
				unprocessed_image.save_to_db(0,&sql(),false);
				region_image.region_images_image_id = unprocessed_image.id;
				

				for (unsigned long j=0; j < additional_tasks_to_load.size(); j++){

					std::map<ns_64_bit,ns_region_processed_image_disk_info>::iterator p = 
						existing_processed_task_images[j].find(region_image.region_images_id);

					if (p == existing_processed_task_images[j].end())
						continue;
					//create a new database record for existing file and link it to the region image record
					p->second.im.id = 0;
					p->second.im.save_to_db(0,&sql());
					region_image.op_images_[additional_tasks_to_load[j]] = p->second.im.id;
				}
				
	//			region_image.captured_images_id = cap_im.captured_images_id;
				
				region_image.specified_16_bit = false;
				region_image.experiment_name = experiment_name;
				region_image.experiment_id = experiment_id;
				region_image.sample_id = sample_id;
				region_image.sample_name = sample_name;
				//create a new record
				region_image.mask_color = 0;
				region_image.detected_worm_state = ns_detected_worm_unsorted;
				region_image.region_images_id = 0;
				region_image.save(&sql());
				region_image.update_all_processed_image_records(sql());

				/*std::string new_filename = im.filename(&sql()) + "." + ns_dir::extract_extension(image.filename);
				std::string old_filename = image.filename;
				bool success(ns_dir::move_file(path_str + DIR_CHAR_STR + old_filename,path_str + DIR_CHAR_STR + new_filename));
				if (!success){
					cerr << "Could not rename " << old_filename << " to " << new_filename << ".  If you run metadata repair on this experiment again, you will get duplicate entries.\n";
					continue;
				}
				image.filename = new_filename;
				image.save_to_db(image.id,&sql());*/
			}
			if (number_of_additions == 0)
				cout << "No database entries were identified as missing.\n";
		}
	}
	sql.release();
}
void ns_worm_learner::test_time_path_analysis_parameters(unsigned long region_id){
	ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__,__LINE__));
	ns_time_path_solver tp_solver;
	tp_solver.load(region_id,sql());
	ns_time_path_solver_parameters default_parameters(ns_time_path_solver_parameters::default_parameters(region_id,sql()));
	vector<vector<ns_time_path_solver_parameters> > parameters;

	unsigned long fragment_duration_in_hours[] = {4,3,2,1,0};
	unsigned long stationary_path_duration_in_hours[] = {4,3,2,1,0};

	ns_hand_annotation_loader by_hand_region_annotations;
	by_hand_region_annotations.load_region_annotations(
			ns_death_time_annotation_set::ns_censoring_and_movement_transitions,region_id,sql());					

	parameters.resize(5);
	for (unsigned int i = 0; i < 5; i++){
		ns_time_path_solver_parameters p(default_parameters);
		p.min_stationary_object_path_fragment_duration_in_seconds = fragment_duration_in_hours[i]*60*60;
		parameters[i].resize(5);
		for (unsigned int j = 0; j < 5; j++){
			p.min_final_stationary_path_duration_in_minutes = stationary_path_duration_in_hours[j]*60;
			parameters[i][j] = p;
		}
	}

	ns_image_server_results_subject sub2;
	sub2.region_id = region_id;
	ns_acquire_for_scope<std::ostream> quant(image_server.results_storage.animal_position_timeseries_3d(
		sub2,sql(),ns_image_server_results_storage::ns_3d_plot,"path_quantification").output());

	quant() << "Min Stationary object path fragment duration (hours),Min Final stationary path duration (hours),"
				"Total objects detected,Number of actual worms, Number of non-worm objects, Number of low-density_paths, Path ID,Center X, Center Y,Start Time,End Time,Duration (hours),Number of Observations in path,Low Density Path,Minimum distance to censored location, Likely censored, New This Round\n";
	vector<vector<ns_time_path_solution> > solutions(parameters.size());
	for (unsigned int i = 0; i < parameters.size(); i++){
		solutions[i].resize( parameters[i].size());
		for (unsigned int j = 0; j < parameters[i].size(); j++){
			//get the solutions
			ns_time_path_solver tp(tp_solver);
			tp.solve(parameters[i][j],solutions[i][j],&sql());
	
			//find which paths are new relative to the refence
			vector<char> path_is_new_this_round(solutions[i][j].paths.size(),0);
			if (j > 0)
				ns_find_new_paths(solutions[i][0],solutions[i][j],path_is_new_this_round);

			//output the solutions to disk
			ns_image_server_results_subject sub;
			sub.region_id = region_id;
			std::string type = "OPFD=" + ns_to_string(parameters[i][j].min_stationary_object_path_fragment_duration_in_seconds) + "&"
								"SPD=" + ns_to_string(parameters[i][j].min_final_stationary_path_duration_in_minutes);
		 
			ns_acquire_for_scope<ostream> position_3d_file_output(
			image_server.results_storage.animal_position_timeseries_3d(
				sub,sql(),ns_image_server_results_storage::ns_3d_plot,type
			).output()
			);
			solutions[i][j].output_visualization_csv(position_3d_file_output());
			position_3d_file_output.release();

			image_server.results_storage.write_animal_position_timeseries_3d_launcher(sub,ns_image_server_results_storage::ns_3d_plot,sql(),type);
			
			//find which elements are censored
			ns_time_path_solution & s(solutions[i][j]);
			vector<char> censored(s.path_groups.size(),0);
			vector<double> min_distances(s.path_groups.size(),10000);
			unsigned long num_censored(0), num_low_density(0),not_annotated(0);
			std::vector<ns_vector_2i> location_centers(s.path_groups.size());
			std::vector<ns_vector_2i> excluded_object_positions(by_hand_region_annotations.annotations.regions[region_id].locations.size());
			
			for (unsigned int k = 0; k < s.path_groups.size(); k++)
				location_centers[k] = s.paths[s.path_groups[k].path_ids[0]].center / s.paths[s.path_groups[k].path_ids[0]].stationary_elements.size();
			for (unsigned int k = 0; k < by_hand_region_annotations.annotations.regions[region_id].locations.size(); k++)
				excluded_object_positions[k] = by_hand_region_annotations.annotations.regions[region_id].locations[k].properties.position;
		
			std::vector<unsigned long> matched;
			ns_match_locations(excluded_object_positions,location_centers,matched);

			
			for (unsigned int k = 0; k <s.path_groups.size(); k++){
				if (matched[k] != ns_location_not_matched){

					min_distances[k]  = (excluded_object_positions[matched[k]] -location_centers[k]).mag();
					if (min_distances[k] < 1000){
						censored[k] = 1;
						num_censored++;
					}
				}
				 if (s.paths[s.path_groups[k].path_ids[0]].is_low_density_path)
						num_low_density++;
			}
			
			//output metadata for each path in this solution
			for (unsigned int k = 0; k < s.path_groups.size(); k++){
				ns_vector_2i center(s.paths[s.path_groups[k].path_ids[0]].center / s.paths[s.path_groups[k].path_ids[0]].stationary_elements.size());
				quant()  << parameters[i][j].min_stationary_object_path_fragment_duration_in_seconds/(60.0*60.0) << ","
						<< parameters[i][j].min_final_stationary_path_duration_in_minutes/60.0 << ","
						<< s.path_groups.size() << ","
						<< s.path_groups.size()-num_censored << ","
						<< num_censored << ","
						<< num_low_density << ","
						<< j << ","
						<< center.x << ","
						<< center.y << ","
						<< s.time(*s.paths[s.path_groups[k].path_ids[0]].stationary_elements.begin()) << ","
						<< s.time(*s.paths[s.path_groups[k].path_ids[0]].stationary_elements.rbegin()) << ","
						<< (s.time(*s.paths[s.path_groups[k].path_ids[0]].stationary_elements.begin()) - 
							s.time(*s.paths[s.path_groups[k].path_ids[0]].stationary_elements.rbegin()))/(60.0*60.0) << ","
						<< s.paths[s.path_groups[k].path_ids[0]].stationary_elements.size() << ","
						<< (s.paths[s.path_groups[k].path_ids[0]].is_low_density_path?"1":"0") << ","
						<< min_distances[k] << ","
						<< (censored[k]?"1":"0") << ","
						<< (path_is_new_this_round[k]?"1":"0") << "\n";
			}
			s.unlink_detection_results();
		}
	}
	quant.release();

}

void ns_worm_learner::load_strain_metadata_into_database(const std::string filename){
	ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__,__LINE__));
	ifstream i(filename.c_str());
	if (i.fail())
		throw ns_ex("Could not load ") << filename;
	std::string v,v2;
	getline(i,v,',');
	getline(i,v2,'\n');
	if (i.fail() || ns_tolower(v) != "strain" || ns_tolower(v2) != "genotype")
		throw ns_ex("The metadata file must have a header line \"strain,genotype\\n\" and the data must follow in that format.");
	
	std::vector<ns_genotype_db_info> data;
	long l(0);
	while(true){
		getline(i,v,',');
		if (i.fail())
			break;
		for (unsigned int j =0; j < v.size(); j++){
			if (!isdigit(v[j]) && !(isalpha(v[j])))
				throw ns_ex("Encountered a weird strain name on line ") << l << ": " << v;
		}
		getline(i,v2,'\n');	
		if (!v2.empty() && v2[0] != '"' && v2[v2.size()-1] != '"'){
			for (unsigned int j =0; j < v2.size(); j++){
				if (v2[j] == ',')
					throw ns_ex("Encountered an ill-formed genotype on line ") << l << ": " << v2;
			}
		}
		if (i.fail())
			break;
		l++;
		if (v2.empty())
			continue;//throw ns_ex("Could not parse file on line ") << l;
		std::vector<ns_genotype_db_info>::size_type s(data.size());
		//cerr << v << ": " << v2 << "\n";
		data.resize(s+1);
		data[s].strain = v;
		data[s].genotype = v2;
	}
	ns_genotype_fetcher fetcher;
	fetcher.add_information_to_database(data,&sql());
}
unsigned long is_equal(0),is_not_equal(0);

struct ns_scale_param{
	double m,b;
	void calc(const std::vector<double> & v){
		double mmax(0),mmin(256);
		for (unsigned int i = 0; i < v.size(); i++){
			if (v[i] > mmax)
				mmax  = v[i];
			if (v[i] < mmin)
				mmin = v[i];
		}
		b = -mmin; //set lowest value to zero
		if (mmax == mmin)
			m = 255.0/mmax;
		m = 255.0/(mmax-mmin);
	}
};
struct ns_movement_quantification_visualization{
	ns_image_standard image, image_previous, difference, composit;
	vector<double> difference_values,im_values,im_prev_values;
	ns_scale_param difference_scaling,im_scaling;
	void register_difference_values(const double im_v, const double im_prev_v,const double & d){
		im_values.push_back(im_v);
		im_values.push_back(im_prev_v);
		difference_values.push_back(d);
	}
	void calculate_difference_scaling(){
		difference_scaling.calc(difference_values);
		im_scaling.calc(im_values);
		difference_values.resize(0);
		im_values.resize(0);
	}
	void init(const ns_image_properties & prop){
		image.init(prop);
		image_previous.init(prop);
		difference.init(prop);
		for (unsigned int y = 0; y < prop.height; y++)
			for (unsigned int x = 0; x < prop.width; x++){
				image[y][x] = image_previous[y][x] = difference[y][x] = 0;
			}
	}
	void specify_values(unsigned long y, unsigned long x, double im_v, double im_prev_v,double d){
		difference[y][x] = (ns_8_bit)((d+difference_scaling.b)*difference_scaling.m);
		image[y][x] = (ns_8_bit)((im_v+im_scaling.b)*im_scaling.m);
		image_previous[y][x] = (ns_8_bit)((im_prev_v+im_scaling.b)*im_scaling.m);
	}
	void calculate_composit(){
		ns_image_properties prop(image.properties());
		prop.width*=3;
		composit.prepare_to_recieve_image(prop);
		for (unsigned int y = 0; y < difference.properties().height; y++){
			for (unsigned int x = 0; x < difference.properties().width; x++){
				composit[y][x] = image_previous[y][x];
			}
			for (unsigned int x = 0; x < difference.properties().width; x++){
				composit[y][x+difference.properties().width] = image[y][x];
			}
			for (unsigned int x = 0; x < difference.properties().width; x++){
				composit[y][x+2*difference.properties().width] = difference[y][x];
			}
		}
	}
	static void concatenate(const std::vector<ns_movement_quantification_visualization *> & g,const std::vector<std::string> & labels, ns_image_standard & im){
		if (g.empty())
			return;
		g[0]->calculate_composit();
		for (unsigned int i = 1; i < g.size(); i++){
			g[i]->calculate_composit();
			if (g[i]->composit.properties() != g[i-1]->composit.properties())
				throw ns_ex("Invalid concatenation");
		}
		ns_image_properties p_big(g[0]->composit.properties());
		ns_image_properties p(p_big);
		p_big.height*=g.size();
		im.init(p_big);
		for (unsigned int i = 0; i < g.size(); i++){
			for (unsigned int y = 0; y < p.height; y++)
				for (unsigned int x = 0; x < p.width; x++){
					im[i*p.height+y][x] = g[i]->composit[y][x];
				}
		}
		ns_acquire_lock_for_scope lock(font_server.default_font_lock, __FILE__, __LINE__);
		ns_font & font(font_server.get_default_font());
		font.set_height(14);
		for(unsigned int i = 0; i < labels.size(); i++){
			font.draw_grayscale(5,i*p.height+16,255,labels[i],im);
		}
		lock.release();
	}
	static void output(const std::string & base, int worm_id,unsigned long time, const ns_image_standard & im){
		string full_dir(base + "im_" + ns_to_string(worm_id));
		ns_dir::create_directory_recursive(full_dir);

		std::string f(full_dir + "\\" + "im_" + ns_to_string(worm_id) + "_" + ns_to_string(time) + ".tif");
		ns_save_image(f,im);
	}
};

ns_8_bit ns_crop(ns_8_bit val,ns_8_bit eq_val,ns_8_bit crop){
	if (val >= crop)
		return eq_val;
	return 0;
}

void ns_worm_learner::generate_single_frame_posture_image_pixel_data(const bool single_region){
	load_current_experiment_movement_results(ns_death_time_annotation_set::ns_censoring_and_movement_transitions,data_selector.current_experiment_id());
	ns_hand_annotation_loader by_hand_annotations;
	ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__,__LINE__));
	by_hand_annotations.load_experiment_annotations(ns_death_time_annotation_set::ns_censoring_and_movement_transitions,data_selector.current_experiment_id(),sql());
	const int total_images_per_worm(50);
	ofstream o_data("y:\\posture_analysis\\out_data.csv"),o_stats("y:\\posture_analysis\\out_stats.csv");
	std::string image_output_base_dir("y:\\posture_analysis\\images\\");

	o_data << "Region Id, Worm Id, time, pixel value, pixel mean shifted value, pixel mean scaled value,pixel stdev scaled value,pixel zscore,freq,equalized frequency, equalized cropped frequency\n";
	o_stats << "Region Id, Worm Id, time, number of pixels,mean(pixel), stdev(Pixel), Size of Overlap between frames,"
		"Mean absolute value movement difference, Mean mean shifted value movement difference, Mean mean scaled value movement difference,Mean stdev scaled value movement difference, Mean absolute value Movement Zscore difference, Mean Abs Zscore * STDEV, Mean equalized absolute value movement difference, Equalization crop value, Mean equalized cropped absolute value moment difference\n";
	unsigned long hist[255],cdf[255],eq_hist[255],eq_cropped_hist[255];
	ns_movement_quantification_visualization images_raw,images_offset,images_zscore,images_equalized,images_scaled,images_var,images_equalized_cropped;
	vector<ns_movement_quantification_visualization *> all_images;
	vector<std::string> labels;
	all_images.push_back(&images_raw);
	labels.push_back("Raw Image");
	all_images.push_back(&images_offset);
	labels.push_back("Shifted Zero Mean");
	all_images.push_back(&images_scaled);
	labels.push_back("Scaled Zero Mean");
	all_images.push_back(&images_var);
	labels.push_back("Unity Variance Scaled");
	all_images.push_back(&images_zscore);
	labels.push_back("Zscore");
	all_images.push_back(&images_equalized);
	labels.push_back("Equalized");
	all_images.push_back(&images_equalized_cropped);
	labels.push_back("Equalized with Low Cropping");
	ns_image_standard output_vis;
	for (unsigned int i = 0; i < movement_results.samples.size(); i++){
		cerr << "Sample " <<movement_results.samples[i].name() << " has " << movement_results.samples[i].regions.size() << " regions\n";
		for (unsigned int j = 0; j < movement_results.samples[i].regions.size(); j++){
			const ns_64_bit region_id(movement_results.samples[i].regions[j].metadata.region_id);
			const ns_time_series_denoising_parameters time_series_denoising_parameters(ns_time_series_denoising_parameters::load_from_db(movement_results.samples[i].regions[j].metadata.region_id,sql()));

			if (single_region && region_id != data_selector.current_region().region_id)
				continue;
			try{
				movement_results.samples[i].regions[j].time_path_solution.load_from_db(region_id,sql(),true);
				movement_results.samples[i].regions[j].time_path_image_analyzer.load_completed_analysis(region_id,movement_results.samples[i].regions[j].time_path_solution,time_series_denoising_parameters,0,sql(),true);
				movement_results.samples[i].regions[j].time_path_image_analyzer.add_by_hand_annotations(by_hand_annotations.annotations);
				for (unsigned int w = 0; w < movement_results.samples[i].regions[j].time_path_image_analyzer.size(); w++){
					if (ns_death_time_annotation::is_excluded(movement_results.samples[i].regions[j].time_path_image_analyzer.group(w).paths[0].excluded()))
						continue;
					int number_of_images = movement_results.samples[i].regions[j].time_path_image_analyzer.group(w).paths[0].element_count();
						if (number_of_images  > total_images_per_worm)
							number_of_images = total_images_per_worm;

					int start_i = 20;//movement_results.samples[i].regions[j].time_path_image_analyzer.group(w).paths[0].element_count()/2;
					if (start_i + number_of_images >=  movement_results.samples[i].regions[j].time_path_image_analyzer.group(w).paths[0].element_count())
						start_i = movement_results.samples[i].regions[j].time_path_image_analyzer.group(w).paths[0].element_count() - number_of_images;
					if (start_i == 0){
						start_i++;
						number_of_images--;
					}

					movement_results.samples[i].regions[j].time_path_image_analyzer.load_images_for_group(w,start_i+number_of_images,sql(),true,false);
					double avg_prev(0), stdev_prev(0);
					ns_image_standard equalized,equalized_prev;
					for (unsigned int k = start_i; k < start_i + number_of_images; k++){
						for (unsigned int m = 0; m < 255; m++){
							hist[m] = 0;
							eq_hist[m] = 0;
							eq_cropped_hist[m]=0;
						}
					
						const ns_image_standard & im(movement_results.samples[i].regions[j].time_path_image_analyzer.group(w).paths[0].element(k).image());
						unsigned long count(0);
						for (unsigned int y = 0; y < im.properties().height; y++){
							for (unsigned int x = 0; x < im.properties().width; x++){
								if (movement_results.samples[i].regions[j].time_path_image_analyzer.group(w).paths[0].element(k).worm_threshold(y,x) ||
									movement_results.samples[i].regions[j].time_path_image_analyzer.group(w).paths[0].element(k-1).worm_threshold(y,x)){
									hist[im[y][x]]++;
									count++;
								}
							}
						}
						int s_i(0),e_i(count);
						double avg(0),var(0),min_cdf_occupancy(count);
						short percentile_90th(-1);
						int percentile_90th_i(9*count/10);
						if (count > 0){
							int c(0);
							for (unsigned int m = 0; m < 255; m++){
								if (hist[m]+c < s_i || c >= e_i){
									c+=hist[m];
									if (m == 0)
										cdf[m] = 0;
									else cdf[m] = cdf[m-1];
									if (percentile_90th == -1 && c>=percentile_90th_i)
										percentile_90th = m;
									continue;
								}
								int num;
								if (c < s_i)
									num = hist[m]+c-s_i;
								else if (c +hist[m]>= e_i)
									num = e_i-c;
								else num = hist[m];
								c+=hist[m];
								if (m == 0)
									cdf[m] = num;
								else cdf[m] = num + cdf[m-1];
								
								if (percentile_90th_i == -1 && c>percentile_90th_i)
									percentile_90th = m;

								if (c > 0 && cdf[m] < min_cdf_occupancy)
									min_cdf_occupancy = cdf[m];
								avg+=num*(double)m;
							}
							avg/=(e_i-s_i);
							c= 0;
							for (unsigned int m = 0; m < 255; m++){
								if (hist[m]+c < s_i || c >= e_i){
									c+=hist[m];
									continue;
								}
								int num;
								if (c < s_i)
									num = hist[m]+c-s_i;
								if (c +hist[m]>= e_i)
									num = e_i-c;
								else num = hist[m];
								c+=hist[m];
								var+=num*(m-avg)*(m-avg);
							}
							var/=(e_i-s_i);
						}
						else var = 1;

						equalized.prepare_to_recieve_image(im.properties());
						ns_8_bit equalized_crop_value(percentile_90th/10);
						for (unsigned int y = 0; y < im.properties().height; y++)
							for (unsigned int x = 0; x < im.properties().width; x++){
								equalized[y][x] = (int)floor(.5+ (cdf[im[y][x]] - min_cdf_occupancy)/(count- min_cdf_occupancy)*255);
								eq_hist[equalized[y][x]]++;
								eq_cropped_hist[ns_crop(im[y][x],equalized[y][x],equalized_crop_value)]++;
							}

						const double stdev(sqrt(var));
						unsigned long diff_count(0);	
						double diff_zscore(0),diff_shift(0),diff_scale(0),diff_raw(0),diff_equalized(0),diff_equalized_cropped(0),diff_var;
						if (k > start_i && k > 0){
							
							const ns_image_standard & im_prev(movement_results.samples[i].regions[j].time_path_image_analyzer.group(w).paths[0].element(k-1).image());
						
							for (unsigned int y = 0; y < im.properties().height; y++){
								for (unsigned int x = 0; x < im.properties().width; x++){
									if (movement_results.samples[i].regions[j].time_path_image_analyzer.group(w).paths[0].element(k).worm_threshold(y,x) ||
										movement_results.samples[i].regions[j].time_path_image_analyzer.group(w).paths[0].element(k-1).worm_threshold(y,x)){
										double d_z(fabs((im[y][x]-avg)/stdev - (im_prev[y][x]-avg_prev)/stdev_prev)),
											   d_o(fabs((im[y][x]-avg) - (im_prev[y][x]-avg_prev))),
											   d_m(fabs((im[y][x]/avg) - (im_prev[y][x]/avg_prev))),
											   d_r(abs((long)im[y][x] - (long)im_prev[y][x])),
											   d_eq(abs((long)equalized[y][x]- (long)equalized_prev[y][x])),
											   d_eq_c(abs((long)ns_crop(im[y][x],equalized[y][x],equalized_crop_value)
											   - (long)ns_crop(im_prev[y][x],equalized_prev[y][x],equalized_crop_value))),
											   d_v(fabs((im[y][x]/stdev) - (im_prev[y][x]/stdev_prev)));

										diff_zscore+=d_z;
										diff_var+=d_v;
										diff_raw+=d_r;
										diff_scale+=d_m;
										diff_equalized+=d_eq;
										diff_shift+=d_o;
										diff_equalized_cropped+=d_eq_c;
										diff_count++;

										images_raw.register_difference_values(im[y][x],im_prev[y][x],d_r);
										images_offset.register_difference_values(im[y][x]-avg,im_prev[y][x]-avg_prev,d_o);
										images_zscore.register_difference_values((im[y][x]-avg)/stdev,(im_prev[y][x]-avg_prev)/stdev_prev,d_z);
										images_equalized.register_difference_values(equalized[y][x],equalized_prev[y][x],d_eq);
										images_equalized_cropped.register_difference_values(ns_crop(im[y][x],equalized[y][x],equalized_crop_value),
																							ns_crop(im_prev[y][x],equalized_prev[y][x],equalized_crop_value),d_eq_c);
										images_scaled.register_difference_values(im[y][x]/avg,im_prev[y][x]/avg_prev,d_m);
										images_var.   register_difference_values(im[y][x]/stdev,im_prev[y][x]/stdev_prev,d_v);
																				
									}
								}
							}
							for (unsigned int l = 0; l < all_images.size(); l++){
								all_images[l]->calculate_difference_scaling();
								all_images[l]->init(im.properties());
							}
							for (unsigned int y = 0; y < im.properties().height; y++){
								for (unsigned int x = 0; x < im.properties().width; x++){
									if (movement_results.samples[i].regions[j].time_path_image_analyzer.group(w).paths[0].element(k).worm_threshold(y,x) ||
										movement_results.samples[i].regions[j].time_path_image_analyzer.group(w).paths[0].element(k-1).worm_threshold(y,x)){
									
										double d_z(fabs((im[y][x]-avg)/stdev - (im_prev[y][x]-avg_prev)/stdev_prev)),
											d_o(fabs((im[y][x]-avg) - (im_prev[y][x]-avg_prev))),
											d_m(fabs((im[y][x]/avg) - (im_prev[y][x]/avg_prev))),
											d_r(abs((long)im[y][x] - (long)im_prev[y][x])),
											d_eq(abs((long)equalized[y][x] - (long)equalized_prev[y][x])),
											 d_eq_c(abs((long)ns_crop(im[y][x],equalized[y][x],equalized_crop_value)- 
											 (long)ns_crop(im_prev[y][x],equalized_prev[y][x],equalized_crop_value))),
											d_v(fabs((im[y][x]/stdev) - (im_prev[y][x]/stdev_prev)));

											images_raw.specify_values(y,x,im[y][x],im_prev[y][x],d_r);
											images_scaled.specify_values(y,x,im[y][x]/avg,im_prev[y][x]/avg_prev,d_m);
											images_offset.specify_values(y,x,im[y][x]-avg,im_prev[y][x]-avg_prev,d_o);
											images_zscore.specify_values(y,x,(im[y][x]-avg)/stdev,(im_prev[y][x]-avg_prev)/stdev_prev,d_z);
											images_equalized.specify_values(y,x,equalized[y][x],equalized_prev[y][x],d_eq);
											images_equalized_cropped.specify_values(y,x,ns_crop(im[y][x],equalized[y][x],equalized_crop_value),
												ns_crop(im_prev[y][x],equalized_prev[y][x],equalized_crop_value),d_eq_c);
											images_var      .specify_values(y,x,im[y][x]/stdev,im_prev[y][x]/stdev_prev,d_v);
									}
								}

							}
							
							unsigned long t((movement_results.samples[i].regions[j].time_path_image_analyzer.group(w).paths[0].element(k).absolute_time - movement_results.samples[i].regions[j].metadata.time_at_which_animals_had_zero_age));
							ns_movement_quantification_visualization::concatenate(all_images,labels,output_vis);
							ns_movement_quantification_visualization::output(image_output_base_dir,w,t/60,output_vis);
						}
						if (diff_count > 0){
							diff_shift /=diff_count;
							diff_scale /=diff_count;
							diff_zscore/=diff_count;
							diff_raw/=diff_count;
							diff_equalized/=diff_count;
							diff_var/=diff_count;
							diff_equalized_cropped/=diff_count;
						}
						avg_prev = avg;
						stdev_prev = stdev;
						equalized.pump(equalized_prev,1024);

						unsigned long t((movement_results.samples[i].regions[j].time_path_image_analyzer.group(w).paths[0].element(k).absolute_time - movement_results.samples[i].regions[j].metadata.time_at_which_animals_had_zero_age));

						o_stats << region_id << "," << w << "," 
							<< t/60 <<","<< count << "," << avg << "," << stdev << "," ;
						if (diff_count > 0)
							o_stats << diff_count << "," << diff_raw << "," <<diff_shift << "," << diff_scale << ","  << diff_var << "," << diff_zscore << "," << diff_zscore*(stdev+stdev_prev)/2 << 
							"," << diff_equalized << "," << (int)equalized_crop_value << "," << diff_equalized_cropped << "\n";
						else o_stats << ",,,,\n";
							
				

						for (unsigned int m = 0; m < 255; m++){
							o_data << region_id << "," << w << "," 
							<< t/60 << ","
							<< m << ","
							<< m - avg << ",";
							if (avg != 0)
								o_data << m/avg << ",";
							else o_data << ",";
							if (stdev != 0){
								o_data << m/stdev << "," << (m-avg)/stdev << "," ;
							}
							else o_data << ",,";
							o_data << "," << hist[m] << "," << eq_hist[m] << "," << eq_cropped_hist[m] << "\n";
						}
					}
			
					movement_results.samples[i].regions[j].time_path_image_analyzer.group(w).clear_images();

				}
			}
			catch(ns_ex & ex){
				cerr << ex.text() << "\n";
			}
		}
	}

}

void ns_worm_learner::compile_experiment_survival_and_movement_data(bool use_by_hand_censoring,const ns_region_visualization & vis,const  ns_movement_data_source_type::type & type){
//const bool scatter_proportion_plot, const bool use_interpolated_data){
	unsigned int experiment_id = data_selector.current_experiment_id();

	ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__,__LINE__));
	ns_death_time_annotation_compiler survival_curve_compiler;

	
	ns_death_time_annotation_set::ns_annotation_type_to_load type_to_load;
	if (vis == ns_survival_curve)
		type_to_load = ns_death_time_annotation_set::ns_censoring_and_movement_transitions;
	else
		type_to_load = ns_death_time_annotation_set::ns_all_annotations;
	
	cerr << "Loading Machine Annotations...\n";
	load_current_experiment_movement_results(type_to_load,experiment_id);

	//we explicitly check to see if any regions need to have their censoring recalulated.
	//people were often forgetting to do this, so we now bug them about it.
	std::vector<ns_64_bit> regions_needing_censoring_recalculation;
	regions_needing_censoring_recalculation.reserve(15);
	for (unsigned int i = 0; i < movement_results.samples.size(); i++)
		for (unsigned int j = 0; j < movement_results.samples[i].regions.size(); j++) {
			const ns_region_metadata & metadata(movement_results.samples[i].regions[j].metadata);
			if (metadata.by_hand_annotation_timestamp > metadata.movement_rebuild_timestamp)
				regions_needing_censoring_recalculation.push_back(metadata.region_id);
		}
	if (!regions_needing_censoring_recalculation.empty()) {
		class ns_choice_dialog dialog;
		dialog.title = ns_to_string(regions_needing_censoring_recalculation.size()) + " regions have by-hand annotations that are not encorporated into the censoring calculations.  How should this be handled?\n";
		dialog.title += "Immediately: Recalculate censoring immediately on this machine\n";
		dialog.title += "Schedule: Schedule jobs to recalculate censoring using the image processing server\n";
		dialog.title += "Ignore: Use older, out-of-date censoring calculations\n";
		dialog.option_1 = "Immediately";
		dialog.option_2 = "Schedule";
		dialog.option_3 = "Ignore";
		ns_run_in_main_thread<ns_choice_dialog> b(&dialog);
		switch (dialog.result) {
		case 1: {
			image_server.register_server_event(ns_image_server_event("Recalculating censoring"), &sql());
			ns_image_processing_pipeline p(1024);
			for (unsigned int i = 0; i < regions_needing_censoring_recalculation.size(); i++) {
				image_server.add_subtext_to_current_event(ns_to_string((int)(i *100.0 / regions_needing_censoring_recalculation.size())) + "%...", &sql());
				ns_processing_job job;
				job.region_id = regions_needing_censoring_recalculation[i];
				job.maintenance_task = ns_maintenance_rebuild_movement_from_stored_image_quantification;
				analyze_worm_movement_across_frames(job, &image_server, sql(), false);
			}
			break;
		}
		case 2: {
			image_server.register_server_event(ns_image_server_event("Submitting jobs to cluster."), &sql());
			ns_processing_job job;
			for (unsigned int i = 0; i < regions_needing_censoring_recalculation.size(); i++) {
				sql() << "INSERT INTO processing_jobs SET region_id=" << regions_needing_censoring_recalculation[i] << ", "
					<< "maintenance_task=" << (unsigned int)ns_maintenance_rebuild_movement_from_stored_image_quantification << ", time_submitted=" << ns_current_time() << ", urgent=1";
				sql().send_query();
			}
			sql().send_query("COMMIT");
			ns_image_server_push_job_scheduler::request_job_queue_discovery(sql());
			return;
		}
		case 3:
			image_server.register_server_event(ns_image_server_event("Ignoring request to rebuild censoring data."), &sql());
			break;
		default: throw ns_ex("Unknown result!");
		}
	}
	
	std::vector<ns_image_standard >sample_graphs;
	sample_graphs.reserve(movement_results.samples.size());


	ns_image_server_results_subject results_subject;
	results_subject.experiment_id = experiment_id;
	results_subject.experiment_name = movement_results.experiment_name();
	vector<map<ns_death_time_annotation::ns_by_hand_annotation_integration_strategy , ns_acquire_for_scope<ostream> > > movement_data_plate_file_with_incomplete(ns_death_time_annotation::ns_number_of_multiworm_censoring_strategies);
	vector<map<ns_death_time_annotation::ns_by_hand_annotation_integration_strategy , ns_acquire_for_scope<ostream> > > movement_data_plate_file_without_incomplete(ns_death_time_annotation::ns_number_of_multiworm_censoring_strategies);
	ns_acquire_for_scope<ostream> movement_data_plate_file_with_alternate_missing_return_strategy_1,
								  movement_data_plate_file_with_alternate_missing_return_strategy_2;
	ns_acquire_for_scope<ostream> censoring_diagnostics_by_plate_machine,
								  censoring_diagnostics_by_plate_by_hand;

	const ns_death_time_annotation::ns_missing_worm_return_strategy default_missing_return_strategy(ns_death_time_annotation::ns_censoring_minimize_missing_times),
		alternate_missing_return_strategy_1(ns_death_time_annotation::ns_censoring_assume_uniform_distribution_of_missing_times),
		alternate_missing_return_strategy_2(ns_death_time_annotation::ns_censoring_assume_uniform_distribution_of_only_large_missing_times);

	const ns_death_time_annotation::ns_by_hand_annotation_integration_strategy by_hand_annotation_integration_strategy[2] = 
					{ns_death_time_annotation::ns_machine_annotations_if_no_by_hand,ns_death_time_annotation::ns_only_machine_annotations};

				

	if (vis == ns_movement_area_plot || vis == ns_movement_scatter_proportion_plot){
		for (unsigned int censoring_strategy = 0; censoring_strategy < (int)ns_death_time_annotation::ns_number_of_multiworm_censoring_strategies; censoring_strategy++){
			if (censoring_strategy == (int)ns_death_time_annotation::ns_by_hand_censoring)
				continue;
			for (unsigned int bhais = 0; bhais < 2; bhais++){
				movement_data_plate_file_with_incomplete[censoring_strategy][by_hand_annotation_integration_strategy[bhais]]
					.attach(image_server.results_storage.movement_timeseries_data(
					by_hand_annotation_integration_strategy[bhais],
					(ns_death_time_annotation::ns_multiworm_censoring_strategy)censoring_strategy,
					ns_death_time_annotation::ns_censoring_minimize_missing_times,
					ns_include_unchanged,
					results_subject,"grouped_by_plate","movement_timeseries",sql()).output());
				ns_worm_movement_measurement_summary::out_header(movement_data_plate_file_with_incomplete[censoring_strategy][by_hand_annotation_integration_strategy[bhais]]());
				movement_data_plate_file_without_incomplete[censoring_strategy][by_hand_annotation_integration_strategy[bhais]].attach(
					image_server.results_storage.movement_timeseries_data(
					by_hand_annotation_integration_strategy[bhais],
					(ns_death_time_annotation::ns_multiworm_censoring_strategy)censoring_strategy,
					ns_death_time_annotation::ns_censoring_minimize_missing_times,
					ns_force_to_fast_moving,
					results_subject,"grouped_by_plate","movement_timeseries",sql()).output());
				ns_worm_movement_measurement_summary::out_header(movement_data_plate_file_without_incomplete[censoring_strategy][by_hand_annotation_integration_strategy[bhais]]());
			}
		}	
		censoring_diagnostics_by_plate_machine.attach(image_server.results_storage.movement_timeseries_data(
			ns_death_time_annotation::ns_only_machine_annotations,
			ns_death_time_annotation::ns_merge_multiple_worm_clusters_and_missing_and_censor,
			default_missing_return_strategy,
			ns_include_unchanged,
			results_subject,"grouped_by_plate","censoring_diagnostics",sql()).output());
		censoring_diagnostics_by_plate_by_hand.attach(image_server.results_storage.movement_timeseries_data(
			ns_death_time_annotation::ns_machine_annotations_if_no_by_hand,
			ns_death_time_annotation::ns_merge_multiple_worm_clusters_and_missing_and_censor,
			default_missing_return_strategy,
			ns_include_unchanged,
			results_subject,"grouped_by_plate","censoring_diagnostics",sql()).output());
		ns_worm_movement_summary_series::output_censoring_diagnostic_header(censoring_diagnostics_by_plate_by_hand());
		ns_worm_movement_summary_series::output_censoring_diagnostic_header(censoring_diagnostics_by_plate_machine());
		movement_data_plate_file_with_alternate_missing_return_strategy_1.attach(image_server.results_storage.movement_timeseries_data(
			ns_death_time_annotation::ns_machine_annotations_if_no_by_hand,
			ns_death_time_annotation::ns_merge_multiple_worm_clusters_and_missing_and_censor,
			alternate_missing_return_strategy_1,
			ns_include_unchanged,
			results_subject,"grouped_by_plate","movement_timeseries",sql()).output());
		ns_worm_movement_measurement_summary::out_header(movement_data_plate_file_with_alternate_missing_return_strategy_1());	
		movement_data_plate_file_with_alternate_missing_return_strategy_2.attach(image_server.results_storage.movement_timeseries_data(
			ns_death_time_annotation::ns_machine_annotations_if_no_by_hand,
			ns_death_time_annotation::ns_merge_multiple_worm_clusters_and_missing_and_censor,
			alternate_missing_return_strategy_2,
			ns_include_unchanged,
			results_subject,"grouped_by_plate","movement_timeseries",sql()).output());
			ns_worm_movement_measurement_summary::out_header(movement_data_plate_file_with_alternate_missing_return_strategy_2());
	}
	
	
	//unsigned long number_of_curves(0);
	//for (unsigned int i = 0; i < movement_results.samples.size(); i++){
	//	number_of_curves+= movement_results.samples[i].regions.size();
	//}

	//unsigned long curve_number(0);
	//
	
	cerr << "Loading By Hand Annotations...\n";
	//STEP 1 in generating death times: load by hand annotations
	ns_hand_annotation_loader by_hand_annotations;
	if (use_by_hand_censoring){
		by_hand_annotations.load_experiment_annotations(ns_death_time_annotation_set::ns_censoring_and_movement_transitions,experiment_id,sql());
	}	
	unsigned long total_regions_processed(0);
	for (unsigned int i = 0; i < movement_results.samples.size(); i++){
		std::vector<ns_image_standard> graphs_area;
		graphs_area.reserve(movement_results.samples[i].regions.size());
		std::vector<ns_image_standard> graphs_prop;
		graphs_prop.reserve(movement_results.samples[i].regions.size());
		
		if (movement_results.total_number_of_regions() > 0)
		cerr << (100*total_regions_processed)/movement_results.total_number_of_regions() << "%...";
		total_regions_processed+=movement_results.samples[i].regions.size();
		for (unsigned int j = 0; j < movement_results.samples[i].regions.size(); j++){
	
			try{
				//STEP 2 in generating death times: add all the machine data to survival curve compiler
				survival_curve_compiler.add(movement_results.samples[i].regions[j].death_time_annotation_set,movement_results.samples[i].regions[j].metadata);
				
				//if we're only outputting death times, nothing in the following section matters
				if (vis == ns_survival_curve)
					continue;
				
		
				ns_death_time_annotation_compiler compiled_region;
				compiled_region.add(movement_results.samples[i].regions[j].death_time_annotation_set,movement_results.samples[i].regions[j].metadata);
				compiled_region.add(by_hand_annotations.annotations,ns_death_time_annotation_compiler::ns_do_not_create_regions);
				compiled_region.normalize_times_to_zero_age();
				
				//generate movement series debugging telemetry for each by hand annotation strategy
				for (unsigned int bhais = 0; bhais < 2; bhais++){
					for (unsigned int censoring_strategy = 0; censoring_strategy < (int)ns_death_time_annotation::ns_number_of_multiworm_censoring_strategies; censoring_strategy++){
						if (censoring_strategy == (int)ns_death_time_annotation::ns_by_hand_censoring)
							continue;
					
						results_subject.sample_name = movement_results.samples[i].name();
						results_subject.sample_id = movement_results.samples[i].id();
						results_subject.region_name = movement_results.samples[i].regions[j].metadata.region_name;
						results_subject.region_id =movement_results.samples[i].regions[j].metadata.region_id;

						std::string title = movement_results.samples[i].name() + "::" + movement_results.samples[i].regions[j].metadata.region_name;
				
						if (movement_results.samples[i].regions[j].metadata.strain.size() != 0)
							title+= "::" + movement_results.samples[i].regions[j].metadata.strain;
				
						ns_worm_movement_summary_series series;
						ns_death_time_annotation_set set;
					
						series.from_death_time_annotations(by_hand_annotation_integration_strategy[bhais],
							(ns_death_time_annotation::ns_multiworm_censoring_strategy)censoring_strategy,
							default_missing_return_strategy,
							compiled_region,ns_force_to_fast_moving);
						series.generate_censoring_annotations(movement_results.samples[i].regions[j].metadata,0,set);
						series.to_file(movement_results.samples[i].regions[j].metadata,movement_data_plate_file_with_incomplete[censoring_strategy][by_hand_annotation_integration_strategy[bhais]]());
						series.to_file(movement_results.samples[i].regions[j].metadata,movement_data_plate_file_without_incomplete[censoring_strategy][by_hand_annotation_integration_strategy[bhais]]());

						series.from_death_time_annotations(by_hand_annotation_integration_strategy[bhais],
							(ns_death_time_annotation::ns_multiworm_censoring_strategy)censoring_strategy,
							default_missing_return_strategy,
							compiled_region,ns_include_unchanged);
						series.generate_censoring_annotations(movement_results.samples[i].regions[j].metadata,0,set);
						series.to_file(movement_results.samples[i].regions[j].metadata,movement_data_plate_file_with_incomplete[censoring_strategy][by_hand_annotation_integration_strategy[bhais]]());
					
						if (censoring_strategy == ns_death_time_annotation::ns_merge_multiple_worm_clusters_and_missing_and_censor){

							series.from_death_time_annotations(by_hand_annotation_integration_strategy[bhais],
								(ns_death_time_annotation::ns_multiworm_censoring_strategy)censoring_strategy,
								alternate_missing_return_strategy_1,
								compiled_region,ns_include_unchanged);
							series.generate_censoring_annotations(movement_results.samples[i].regions[j].metadata,0,set);
							series.to_file(movement_results.samples[i].regions[j].metadata,
									movement_data_plate_file_with_alternate_missing_return_strategy_1());

							series.from_death_time_annotations(by_hand_annotation_integration_strategy[bhais],
								(ns_death_time_annotation::ns_multiworm_censoring_strategy)censoring_strategy,
								alternate_missing_return_strategy_2,
								compiled_region,ns_include_unchanged);

							series.to_file(movement_results.samples[i].regions[j].metadata,
									movement_data_plate_file_with_alternate_missing_return_strategy_2());

						}

					}
				}
			}
			catch(ns_ex & ex){
				if (graphs_area.size() > 0){
					graphs_area.pop_back();
					
					graphs_prop.pop_back();
				}
				cerr << ex.text() << "\n";
			}
		}

	}
	for (unsigned int i = 0; i < movement_data_plate_file_with_incomplete.size(); i++){
		for (map<ns_death_time_annotation::ns_by_hand_annotation_integration_strategy , ns_acquire_for_scope<ostream> >::iterator p = 
				movement_data_plate_file_with_incomplete[i].begin();
				p != movement_data_plate_file_with_incomplete[i].end();
				p++)
			p->second.release();
			for (map<ns_death_time_annotation::ns_by_hand_annotation_integration_strategy , ns_acquire_for_scope<ostream> >::iterator p = 
				movement_data_plate_file_without_incomplete[i].begin();
				p != movement_data_plate_file_without_incomplete[i].end();
				p++)
			p->second.release();
	}
	
	movement_data_plate_file_with_alternate_missing_return_strategy_1.release();
	movement_data_plate_file_with_alternate_missing_return_strategy_2.release();
	censoring_diagnostics_by_plate_machine.release();
	censoring_diagnostics_by_plate_by_hand.release();
	
	//STEP 3 in generating death times: add by hand annotations to survival curve compiler
	survival_curve_compiler.add(by_hand_annotations.annotations,ns_death_time_annotation_compiler::ns_do_not_create_regions);

	if (vis == ns_survival_curve){
		//STEP 4 in generating death times: open a bunch of output files

		ns_acquire_for_scope<ostream> survival_jmp_file_detailed_days(image_server.results_storage.survival_data(results_subject,"survival_with_alternate_censoring_schemes","machine_jmp_days","csv",sql()).output());
		//ns_acquire_for_scope<ostream> survival_jmp_file_detailed_hours(image_server.results_storage.survival_data(results_subject,"survival_with_alternate_censoring_schemes","machine_jmp_hours","csv",sql()).output());
		//ns_acquire_for_scope<ostream> survival_jmp_file_time_interval_detailed_days(image_server.results_storage.survival_data(results_subject,"survival_with_alternate_censoring_schemes","machine_jmp_time_interval_days","csv",sql()).output());
		//ns_acquire_for_scope<ostream> survival_jmp_file_time_interval_detailed_hours(image_server.results_storage.survival_data(results_subject,"survival_with_alternate_censoring_schemes","machine_jmp_time_interval_hours","csv",sql()).output());
		
		ns_acquire_for_scope<ostream> survival_jmp_file_multiple_events_days(image_server.results_storage.survival_data(results_subject,"survival_multiple_events","machine_jmp_days","csv",sql()).output());
		//ns_acquire_for_scope<ostream> survival_jmp_file_multiple_events_hours(image_server.results_storage.survival_data(results_subject,"survival_multiple_events","machine_jmp_hours","csv",sql()).output());
		
		ns_acquire_for_scope<ostream> survival_jmp_file_machine_simple_days(image_server.results_storage.survival_data(results_subject,"survival_simple","machine_jmp_days","csv",sql()).output());
		//ns_acquire_for_scope<ostream> survival_jmp_file_machine_simple_hours(image_server.results_storage.survival_data(results_subject,"survival_simple","machine_jmp_hours","csv",sql()).output());
		ns_acquire_for_scope<ostream> survival_jmp_file_machine_hand_simple_days(image_server.results_storage.survival_data(results_subject,"survival_simple","machine_hand_jmp_days","csv",sql()).output());
		//ns_acquire_for_scope<ostream> survival_jmp_file_machine_hand_simple_hours(image_server.results_storage.survival_data(results_subject,"survival_simple","machine_hand_jmp_hours","csv",sql()).output());
	
		ns_acquire_for_scope<ostream> survival_jmp_file_simple_with_control_groups_days(image_server.results_storage.survival_data(results_subject,"survival_simple_with_control_groups","machine_jmp_days","csv",sql()).output());
		//ns_acquire_for_scope<ostream> survival_jmp_file_simple_with_control_groups_hours(image_server.results_storage.survival_data(results_subject,"survival_simple_with_control_groups","machine_jmp_hours","csv",sql()).output());
		
		ns_acquire_for_scope<ostream> survival_jmp_file_time_by_hand_interval_simple_days(image_server.results_storage.survival_data(results_subject,"survival_interval","machine_jmp_time_interval_days","csv",sql()).output());
		//ns_acquire_for_scope<ostream> survival_jmp_file_time_by_hand_interval_simple_hours(image_server.results_storage.survival_data(results_subject,"survival_interval","machine_jmp_time_interval_hours","csv",sql()).output());
		
		ns_acquire_for_scope<ostream> survival_jmp_file_time_machine_interval_simple_days(image_server.results_storage.survival_data(results_subject,"survival_interval","machine_hand_jmp_time_interval_days","csv",sql()).output());
		//ns_acquire_for_scope<ostream> survival_jmp_file_time_machine_interval_simple_hours(image_server.results_storage.survival_data(results_subject,"survival_interval","machine_hand_jmp_time_interval_hours","csv",sql()).output());

	//	ns_acquire_for_scope<ostream> survival_jmp_summary_file(image_server.results_storage.survival_data(results_subject,"summary","machine_summary_jmp","csv",sql()).output());
	//	ns_acquire_for_scope<ostream> survival_jmp_strain_summary_file(image_server.results_storage.survival_data(results_subject,"summary","machine_strain_summary_jmp","csv",sql()).output());
	//	ns_acquire_for_scope<ostream> survival_xml_summary_file(image_server.results_storage.survival_data(results_subject,"summary","machine_summary_xml","xml",sql()).output());

		//ns_acquire_for_scope<ostream> survival_jmp_summary_file_with_image_stats(image_server.results_storage.survival_data(results_subject,"summary","machine_summary_jmp_with_image_stats","csv",sql()).output());
		
		//ns_acquire_for_scope<ostream> survival_matlab_file(image_server.results_storage.survival_data(results_subject,"matlab","machine_matlab","m",sql(),true).output());
	//	ns_acquire_for_scope<ostream> survival_matlab_file_grouped(image_server.results_storage.survival_data(results_subject,"matlab","machine_matlab_grouped","m",sql(),true).output());
		
		ns_acquire_for_scope<ostream> annotation_diagnostics(image_server.results_storage.survival_data(results_subject,"survival_simple","annotation_diagnostics","csv",sql()).output());
		
		//ns_acquire_for_scope<ostream> survival_excel_file(image_server.results_storage.survival_data(results_subject,"jmp",sql()).output());
		//ns_acquire_for_scope<ostream> survival_ts_file(image_server.results_storage.survival_data(results_subject,"ts",sql()).output());
		
		survival_curve_compiler.generate_validation_information(annotation_diagnostics());
		annotation_diagnostics.release();
	

		//STEP 5 in generating death times: compile all the accumulated by hand and machine data
		ns_lifespan_experiment_set machine_set,machine_hand_set;
		cerr << "\n";
		cerr << "Generating Survival Curve Set...\n";
		survival_curve_compiler.generate_survival_curve_set(machine_set,ns_death_time_annotation::ns_only_machine_annotations,false,false);
		survival_curve_compiler.generate_survival_curve_set(machine_hand_set,ns_death_time_annotation::ns_machine_annotations_if_no_by_hand,false,false);
		
		//don't use death times annotated by hand but missing a corresponding machine-detected event.  
		//This is done to exclude by hand annotations that correspond to events identified in previous anaysis of the plate but not in the most recent one.
		//All by hand annotations have to match up to a /current/ machine annotation.  
		machine_set.include_only_events_detected_by_machine();
		machine_hand_set.include_only_events_detected_by_machine();
		if (machine_set.curves.size() == 0)
			throw ns_ex("The current experiment does not have any valid plates.");

		cerr << "Computing Risk Time series...\n";
		machine_set.generate_survival_statistics();
	
		cerr << "Writing Files...\n";
		//STEP 5 in generating death times: write everything out to disk

		machine_set.output_JMP_file(ns_death_time_annotation::ns_only_machine_annotations,ns_lifespan_experiment_set::ns_output_single_event_times,ns_lifespan_experiment_set::ns_days,survival_jmp_file_machine_simple_days(),ns_lifespan_experiment_set::ns_simple);
		survival_jmp_file_machine_simple_days.release();	
		machine_hand_set.output_JMP_file(ns_death_time_annotation::ns_only_machine_annotations,ns_lifespan_experiment_set::ns_output_single_event_times,ns_lifespan_experiment_set::ns_days,survival_jmp_file_machine_hand_simple_days(),ns_lifespan_experiment_set::ns_simple);
		survival_jmp_file_machine_hand_simple_days.release();	
		machine_set.output_JMP_file(ns_death_time_annotation::ns_only_machine_annotations,ns_lifespan_experiment_set::ns_output_single_event_times,ns_lifespan_experiment_set::ns_days,survival_jmp_file_simple_with_control_groups_days(),ns_lifespan_experiment_set::ns_simple_with_control_groups);
		survival_jmp_file_simple_with_control_groups_days.release();
		machine_set.output_JMP_file(ns_death_time_annotation::ns_only_machine_annotations,ns_lifespan_experiment_set::ns_output_single_event_times,ns_lifespan_experiment_set::ns_days,survival_jmp_file_detailed_days(),ns_lifespan_experiment_set::ns_detailed_with_censoring_repeats);
		survival_jmp_file_detailed_days.release();	
		//machine_set.output_JMP_file(ns_death_time_annotation::ns_only_machine_annotations,ns_lifespan_experiment_set::ns_output_single_event_times,ns_lifespan_experiment_set::ns_days,survival_jmp_file_multiple_events_days(),ns_lifespan_experiment_set::ns_multiple_events);
		//survival_jmp_file_multiple_events_days.release();

		//machine_hand_set.output_JMP_file(ns_death_time_annotation::ns_machine_annotations_if_no_by_hand,ns_lifespan_experiment_set::ns_output_single_event_times,ns_lifespan_experiment_set::ns_hours,survival_jmp_file_machine_simple_hours(),ns_lifespan_experiment_set::ns_simple);
		//survival_jmp_file_machine_simple_hours.release();
		//machine_hand_set.output_JMP_file(ns_death_time_annotation::ns_machine_annotations_if_no_by_hand,ns_lifespan_experiment_set::ns_output_single_event_times,ns_lifespan_experiment_set::ns_hours,survival_jmp_file_machine_hand_simple_hours(),ns_lifespan_experiment_set::ns_simple);
		//survival_jmp_file_machine_hand_simple_hours.release();
		//machine_set.output_JMP_file(ns_death_time_annotation::ns_machine_annotations_if_no_by_hand,ns_lifespan_experiment_set::ns_output_single_event_times,ns_lifespan_experiment_set::ns_hours,survival_jmp_file_detailed_hours(),ns_lifespan_experiment_set::ns_detailed_with_censoring_repeats);
		//survival_jmp_file_detailed_hours.release();
		//machine_set.output_JMP_file(ns_death_time_annotation::ns_machine_annotations_if_no_by_hand,ns_lifespan_experiment_set::ns_output_single_event_times,ns_lifespan_experiment_set::ns_hours,survival_jmp_file_simple_with_control_groups_hours(),ns_lifespan_experiment_set::ns_simple_with_control_groups);
		//survival_jmp_file_simple_with_control_groups_hours.release();
		//machine_set.output_JMP_file(ns_death_time_annotation::ns_machine_annotations_if_no_by_hand,ns_lifespan_experiment_set::ns_output_single_event_times,ns_lifespan_experiment_set::ns_hours,survival_jmp_file_multiple_events_hours(),ns_lifespan_experiment_set::ns_multiple_events);
		//survival_jmp_file_simple_with_control_groups_hours.release();

		machine_set.output_JMP_file(ns_death_time_annotation::ns_only_machine_annotations,ns_lifespan_experiment_set::ns_output_event_intervals,ns_lifespan_experiment_set::ns_days,survival_jmp_file_time_machine_interval_simple_days(),ns_lifespan_experiment_set::ns_simple);
		survival_jmp_file_time_machine_interval_simple_days.release();	
		machine_set.output_JMP_file(ns_death_time_annotation::ns_machine_annotations_if_no_by_hand,ns_lifespan_experiment_set::ns_output_event_intervals,ns_lifespan_experiment_set::ns_days,survival_jmp_file_time_by_hand_interval_simple_days(),ns_lifespan_experiment_set::ns_simple);
		survival_jmp_file_time_by_hand_interval_simple_days.release();	
		//machine_set.output_JMP_file(ns_death_time_annotation::ns_only_machine_annotations,ns_lifespan_experiment_set::ns_output_event_intervals,ns_lifespan_experiment_set::ns_days,survival_jmp_file_time_interval_detailed_days(),ns_lifespan_experiment_set::ns_detailed_with_censoring_repeats);
		//survival_jmp_file_time_interval_detailed_days.release();		

		//machine_set.output_JMP_file(ns_death_time_annotation::ns_only_machine_annotations,ns_lifespan_experiment_set::ns_output_event_intervals,ns_lifespan_experiment_set::ns_hours,survival_jmp_file_time_machine_interval_simple_hours(),ns_lifespan_experiment_set::ns_simple);
		//survival_jmp_file_time_machine_interval_simple_hours.release();
		//machine_set.output_JMP_file(ns_death_time_annotation::ns_machine_annotations_if_no_by_hand,ns_lifespan_experiment_set::ns_output_event_intervals,ns_lifespan_experiment_set::ns_hours,survival_jmp_file_time_by_hand_interval_simple_hours(),ns_lifespan_experiment_set::ns_simple);
		//survival_jmp_file_time_by_hand_interval_simple_hours.release();
		//machine_set.output_JMP_file(ns_death_time_annotation::ns_only_machine_annotations,ns_lifespan_experiment_set::ns_output_event_intervals,ns_lifespan_experiment_set::ns_hours,survival_jmp_file_time_interval_detailed_hours(),ns_lifespan_experiment_set::ns_detailed_with_censoring_repeats);
		//survival_jmp_file_time_interval_detailed_hours.release();

		//ns_acquire_for_scope<ostream> regression_stats_file_days(image_server.results_storage.survival_data(results_subject,"summary",
		//		std::string("machine_device_regression_parameters_days"),
		//		"csv",sql()).output());		
		//ns_acquire_for_scope<ostream> regression_stats_file_hours(image_server.results_storage.survival_data(results_subject,"summary",
		//		std::string("machine_device_regression_parameters_hours"),
		//		"csv",sql()).output());		

		//ns_lifespan_device_normalization_statistics_set::output_JMP_header(regression_stats_file_days());
		//ns_lifespan_device_normalization_statistics_set::output_JMP_header(regression_stats_file_hours());
		//machine_set.normalization_stats_for_death.output_JMP_file(regression_stats_file_days(),24*60*60);
		//machine_set.normalization_stats_for_death.output_JMP_file(regression_stats_file_hours(),60*60);
		//machine_set.normalization_stats_for_fast_movement_cessation.output_JMP_file(regression_stats_file_days(),24*60*60);
		//machine_set.normalization_stats_for_fast_movement_cessation.output_JMP_file(regression_stats_file_hours(),60*60);
		//machine_set.normalization_stats_for_translation_cessation.output_JMP_file(regression_stats_file_days(),24*60*60);
		//machine_set.normalization_stats_for_translation_cessation.output_JMP_file(regression_stats_file_hours(),60*60);
		//regression_stats_file_hours.release();
		//regression_stats_file_days.release();
		
	//	if (!survival_jmp_summary_file.is_null()){
			//summary_aggregator.out_JMP_summary_file(survival_jmp_summary_file());
		//	survival_jmp_summary_file.release();
		//}
		//if (!survival_jmp_strain_summary_file.is_null()){
	//		grouped_summary_aggregator.out_JMP_summary_file(survival_jmp_strain_summary_file());
//			survival_jmp_strain_summary_file.release();
		//}

	//	if (!survival_jmp_summary_file_with_image_stats.is_null()){
		/*	try{
				ns_capture_sample_region_statistics_set region_stats;
				region_stats.loa d_whole_experiment(experiment_id,sql());
				ns_capture_sample_statistics_set sample_stats;
				sample_stats.load_whole_experiment(experiment_id,sql());
				region_stats.set_sample_data(sample_stats);

				region_stats.output_plate_statistics_with_mortality_data_header(survival_jmp_summary_file_with_image_stats());
				region_stats.output_plate_statistics_with_mortality_data(summary_aggregator,survival_jmp_summary_file_with_image_stats());
			}
			catch(ns_ex & ex){
				cerr << ex.text() << "\n";
			}
			survival_jmp_summary_file_with_image_stats.release();
			*/
		//}

		/*if (!survival_matlab_file.is_null()){
			ns_lifespan_experiment_set common_interval;
			set.force_common_time_set_to_constant_time_interval(10*60,common_interval);
			set.output_matlab_file(survival_matlab_file());
			ns_lifespan_experiment_set grouped;
			common_interval.group_strains(grouped);
			//set.output_JMP_summary_file(survival_jmp_strain_summary_file());
			grouped.output_matlab_file(survival_matlab_file_grouped());
			survival_matlab_file.release();
			survival_matlab_file_grouped.release();
		}*/
		cerr << "Writing Detailed Animal Files\n";
		//generate detailed animal data
		bool include_region_image_info(false);

		ns_image_server_results_subject results_subject;
		results_subject.experiment_id = data_selector.current_experiment_id();
		/*
		ns_capture_sample_statistics_set s_set;
		if (include_region_image_info)
			s_set.load_whole_experiment(data_selector.current_experiment_id(),sql());

		ns_capture_sample_region_statistics_set r_set;
		if (include_region_image_info){
			r_set.load_whole_experiment(data_selector.current_experiment_id(),sql());
			r_set.set_sample_data(s_set);
		}
	*/
		ns_acquire_for_scope<ostream> animal_data_file(image_server.results_storage.animal_event_data(results_subject,"detailed_animal_data",sql()).output());
		survival_curve_compiler.generate_detailed_animal_data_file(include_region_image_info,animal_data_file());
		animal_data_file.release();
		
		cerr << "Done\n";
	}
	
	
	if (vis == ns_movement_area_plot || vis == ns_movement_scatter_proportion_plot){
		
		cerr << "Generating Strain Aggregates\n";
		std::string title(movement_results.experiment_name());
		const unsigned long marker_pos(0);
		survival_curve_compiler.normalize_times_to_zero_age();
		//sort by strain
		std::map<std::string, ns_death_time_annotation_compiler> regions_sorted_by_strain;
		for (ns_death_time_annotation_compiler::ns_region_list::iterator p = survival_curve_compiler.regions.begin(); p != survival_curve_compiler.regions.end();p++){
			ns_death_time_annotation_compiler & c(regions_sorted_by_strain[p->second.metadata.strain + "-" + p->second.metadata.strain_condition_1 + "-" + p->second.metadata.strain_condition_2]);
			c.regions.insert(c.regions.end(),ns_death_time_annotation_compiler::ns_region_list::value_type(p->first,p->second));
			c.specifiy_region_metadata(p->second.metadata.region_id,p->second.metadata);
		}

		ns_death_time_annotation::ns_by_hand_annotation_integration_strategy by_hand_annotation_integration_strategy[2] = 
		{ns_death_time_annotation::ns_machine_annotations_if_no_by_hand,ns_death_time_annotation::ns_only_machine_annotations};

		for (unsigned int bhais = 0; bhais < 2; bhais++){
			for (unsigned int censoring_strategy = 0; censoring_strategy < (int)ns_death_time_annotation::ns_number_of_multiworm_censoring_strategies; censoring_strategy++){
				if (censoring_strategy == (int)ns_death_time_annotation::ns_by_hand_censoring)
					continue;
				cerr << "Computing data using censoring strategy " << 
				
					ns_death_time_annotation::multiworm_censoring_strategy_label((ns_death_time_annotation::ns_multiworm_censoring_strategy)censoring_strategy) << "...\n";
			/*	ns_worm_movement_summary_series series_with_incomplete;
				ns_worm_movement_summary_series series_without_incomplete;
				series_with_incomplete.from_death_time_annotations((ns_death_time_annotation::ns_multiworm_censoring_strategy)censoring_strategy,survival_curve_compiler,false);
				series_without_incomplete.from_death_time_annotations((ns_death_time_annotation::ns_multiworm_censoring_strategy)censoring_strategy,survival_curve_compiler,true);
				*/
				ns_image_server_results_subject sub;
				sub.experiment_id = movement_results.experiment_id();
	
				ns_acquire_for_scope<ostream> output_data_with_incomplete(image_server.results_storage.movement_timeseries_data(
					by_hand_annotation_integration_strategy[bhais],
					(ns_death_time_annotation::ns_multiworm_censoring_strategy)censoring_strategy,
					default_missing_return_strategy,
					ns_include_unchanged,
					sub,
					"strain_aggregates","movement_timeseries",sql()).output());
				ns_acquire_for_scope<ostream> output_data_without_incomplete(image_server.results_storage.movement_timeseries_data(
					by_hand_annotation_integration_strategy[bhais],
					(ns_death_time_annotation::ns_multiworm_censoring_strategy)censoring_strategy,
					default_missing_return_strategy,
					ns_force_to_fast_moving,
					sub,
					"strain_aggregates","movement_timeseries",sql()).output());
				if (output_data_with_incomplete.is_null() || output_data_without_incomplete.is_null())
					throw ns_ex("Could not open aggregate data file");

				ns_acquire_for_scope<ostream> censoring_diagnostics(image_server.results_storage.movement_timeseries_data(
					by_hand_annotation_integration_strategy[bhais],
					ns_death_time_annotation::ns_merge_multiple_worm_clusters_and_missing_and_censor,
					default_missing_return_strategy,
					ns_include_unchanged,
					sub,"strain_aggregates","censoring_diagnostics",sql()).output());

				ns_worm_movement_summary_series::output_censoring_diagnostic_header(censoring_diagnostics());

				ns_worm_movement_measurement_summary::out_header(output_data_with_incomplete());
				ns_worm_movement_measurement_summary::out_header(output_data_without_incomplete());


				ns_acquire_for_scope<ostream> output_data_alternate_missing_return_strategy_1;
				ns_acquire_for_scope<ostream> output_data_alternate_missing_return_strategy_2;
				if (censoring_strategy == ns_death_time_annotation::ns_merge_multiple_worm_clusters_and_missing_and_censor){
				
					output_data_alternate_missing_return_strategy_1.attach(image_server.results_storage.movement_timeseries_data(
					by_hand_annotation_integration_strategy[bhais],
						(ns_death_time_annotation::ns_multiworm_censoring_strategy)censoring_strategy,
					alternate_missing_return_strategy_1,
					ns_include_unchanged,sub,
					"strain_aggregates","movement_timeseries",sql()).output());

					output_data_alternate_missing_return_strategy_2.attach(image_server.results_storage.movement_timeseries_data(
					by_hand_annotation_integration_strategy[bhais],
						(ns_death_time_annotation::ns_multiworm_censoring_strategy)censoring_strategy,
					alternate_missing_return_strategy_2,
					ns_include_unchanged,sub,
					"strain_aggregates","movement_timeseries",sql()).output());
					ns_worm_movement_measurement_summary::out_header(output_data_alternate_missing_return_strategy_1());
					ns_worm_movement_measurement_summary::out_header(output_data_alternate_missing_return_strategy_2());
				}
		
				if (1){
					for (std::map<std::string, ns_death_time_annotation_compiler>::iterator p = regions_sorted_by_strain.begin(); p != regions_sorted_by_strain.end(); p++){
						const std::string & strain_name(p->first);
						ns_worm_movement_summary_series strain_series;
						ns_region_metadata aggregated_metadata(p->second.regions.begin()->second.metadata);
						aggregated_metadata.device.clear();
						aggregated_metadata.incubator_location.clear();
						aggregated_metadata.region_name.clear();
						aggregated_metadata.region_id = 0;
						aggregated_metadata.sample_id = 0;
						aggregated_metadata.sample_name.clear();
						aggregated_metadata.time_at_which_animals_had_zero_age = 0;
						ns_death_time_annotation_set set;

						strain_series.from_death_time_annotations(
						by_hand_annotation_integration_strategy[bhais],
							(ns_death_time_annotation::ns_multiworm_censoring_strategy)censoring_strategy,
							default_missing_return_strategy,
							p->second,ns_force_to_fast_moving);
						strain_series.to_file(aggregated_metadata,output_data_without_incomplete());

						strain_series.from_death_time_annotations(
							by_hand_annotation_integration_strategy[bhais],
					
							(ns_death_time_annotation::ns_multiworm_censoring_strategy)censoring_strategy,
							default_missing_return_strategy,
							p->second,ns_include_unchanged);
						strain_series.to_file(aggregated_metadata,output_data_with_incomplete());

						if (censoring_strategy == ns_death_time_annotation::ns_merge_multiple_worm_clusters_and_missing_and_censor){
					//		strain_series.generate_censoring_diagnostic_file(aggregated_metadata,censoring_diagnostics());
							strain_series.from_death_time_annotations(
								by_hand_annotation_integration_strategy[bhais],
					
								(ns_death_time_annotation::ns_multiworm_censoring_strategy)censoring_strategy,
								alternate_missing_return_strategy_1,
								p->second,ns_include_unchanged)	;
								strain_series.to_file(aggregated_metadata,output_data_alternate_missing_return_strategy_1());
							
				//			strain_series.generate_censoring_diagnostic_file(aggregated_metadata,censoring_diagnostics());

							strain_series.from_death_time_annotations(
								by_hand_annotation_integration_strategy[bhais],
					
								(ns_death_time_annotation::ns_multiworm_censoring_strategy)censoring_strategy,
								alternate_missing_return_strategy_2,
								p->second,ns_include_unchanged)	;
								strain_series.to_file(aggregated_metadata,output_data_alternate_missing_return_strategy_2());
							
					//		strain_series.generate_censoring_diagnostic_file(aggregated_metadata,censoring_diagnostics());
						}
						//output_plot_prop.release();
						//output_plot_area.release();
					}
				}
				output_data_with_incomplete.release();
				output_data_without_incomplete.release();
			}

			//ns_make_collage(sample_graphs, current_image, 256,true,255,false,(float)(1.0/sample_graphs.size()));

			/*ns_image_storage_reciever_handle<ns_image_storage_handler::ns_component> r(
				image_server.results_storage.movement_timeseries_collage(
					results_subject,
					ns_movement_data_source_type::type_string(type),
					ns_tiff,
					1024,
					sql()));
			current_image.pump(r.output_stream(),1024);
			draw_image(-1,-1,current_image);*/
		}
	}
	
	sql.release();
}
	

void ns_worm_learner::calculate_image_statistics_for_experiment_sample(unsigned long experiment_id,ns_sql & sql,bool overwrite_extant){
	sql << "SELECT id,name FROM capture_samples WHERE experiment_id = " << experiment_id;
	ns_sql_result res;
	sql.get_rows(res);
	for (unsigned int i = 0; i < res.size(); i++){
		cerr << "Processing sample " << res[i][1] << "\n";
		sql << "SELECT id, image_id, image_statistics_id FROM captured_images WHERE sample_id = " << res[i][0] << " AND censored = 0 AND problem = 0 AND currently_being_processed = 0";
		if (!overwrite_extant)
			sql << " AND image_statistics_id = 0";
		ns_sql_result res2;
		sql.get_rows(res2);
		for (unsigned int j = 0; j < res2.size(); j++){
			cerr << (unsigned long)((100*j)/(float)res2.size()) << "%...";
			ns_image_statistics stats;
			unsigned long capture_sample_image_id(atol(res2[j][0].c_str()));
			ns_image_server_image im;
			im.id = atol(res2[j][1].c_str());
			stats.calculate_statistics_from_image(im,sql);
			cerr << "(" << stats.image_statistics.mean << ")";
			ns_64_bit previous_db_id(ns_atoi64(res2[j][2].c_str()));
			ns_64_bit db_id(previous_db_id);
			stats.submit_to_db(db_id,sql);
			if (previous_db_id != db_id){
				sql << "UPDATE captured_images SET image_statistics_id=" << db_id << " WHERE id=" << capture_sample_image_id;
				sql.send_query();
			}
			cerr << "\n";
		}
	}
};

void ns_worm_learner::upgrade_tables(){
	
	ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__,__LINE__));
	ns_alert_dialog d;
	if (image_server.upgrade_tables(&sql(),false,image_server.current_sql_database(),false))
		d.text = "Schema update completed.";
	else
		d.text = "No update was needed.";
		
	sql.release();
		
	//d.act();
	ns_run_in_main_thread<ns_alert_dialog> dd(&d);

}
void ns_worm_learner::handle_file_request(const string & fname){
	string filename(fname);
	if (filename.substr(0,8)=="file:///")
		filename = fname.substr(8);

	std::string ext = ns_tolower(ns_dir::extract_extension(filename));

	current_clipboard_filename = filename;

	cerr << "received " << filename << "(" << ext << ")\n";
	if (ext == "m4v"){
		std::string output_basename = ns_dir::extract_filename_without_extension(filename);
		cerr << "Processing video " << filename << ": ";
		ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__,__LINE__));
		ns_image_processing_pipeline::wrap_m4v_stream(filename,output_basename,25,!generate_mp4_,sql());
		sql.release();
		cerr<< " Done.\n";
	}
	else if (ext == "jpg" || ext == "tif" || ext == "jp2"){
		load_file(filename);
		draw();
	}
	else if (ext == "xml"){
		string debug_filename(ns_dir::extract_filename_without_extension(filename) + "=summary.txt");
		bool write_to_disk,write_to_db;
		ask_if_schedule_should_be_submitted_to_db(write_to_disk,write_to_db);
	
		if (write_to_disk || write_to_db){
			vector<std::string> warnings;
			try{
				ns_image_server::process_experiment_capture_schedule_specification(filename,warnings,overwrite_submitted_capture_specification,write_to_db,debug_filename);
			}
			catch(ns_ex & ex){
				warnings.push_back(ex.text());
			}
			if (warnings.size() > 0){
				ns_text_dialog td;
				td.grid_text.push_back("The following issues were identified in the schedule you supplied:");
				td.grid_text.insert(td.grid_text.end(),warnings.begin(),warnings.end());
				td.title = "Capture Schedule Information";
				ns_run_in_main_thread_custom_wait<ns_text_dialog> b(&td);

			}
		}
	}
	
	else throw ns_ex("Unknown file type:") << ext;

}
void ns_worm_learner::paste_from_clipboard(){
#ifndef _WIN32
	throw ns_ex("Clipboard currently only supported on Windows");
#else
	if (OpenClipboard(NULL) == 0)
		throw ns_ex("Could not open clipboard");
	try{

		HBITMAP hbmp = 0;
		//BITMAPINFO bmp_info;
		//bool have_bmp_info = false;

		unsigned int clipboard_format = 0;
		std::vector<std::string> unknown_formats;
		bool handled_object(false);
		while(clipboard_format = EnumClipboardFormats(clipboard_format)){
			if (CF_GDIOBJFIRST <= clipboard_format && clipboard_format <= CF_GDIOBJLAST)
				unknown_formats.push_back( std::string("CF_GDIOBJ #") + ns_to_string(clipboard_format - CF_GDIOBJFIRST));
			else 
			if (CF_PRIVATEFIRST <= clipboard_format && clipboard_format <= CF_PRIVATELAST)
				unknown_formats.push_back( std::string("CF_PRIVATE #") + ns_to_string(clipboard_format - CF_PRIVATEFIRST));
			else		
			switch(clipboard_format){
				case CF_BITMAP:{
					hbmp = (HBITMAP)GetClipboardData(CF_BITMAP);
					if (hbmp == NULL)
						throw ns_ex("Could not load bitmap from clipboard!");
					if (hbmp == 0)
						throw ns_ex("Cannot load image from bitmap.");
					BITMAP bmp;
					if (!GetObject(hbmp, sizeof(BITMAP), (LPSTR)&bmp)) 
						throw ns_ex("Could not read from bitmap handle");

					ns_image_properties p;
					if (bmp.bmBitsPixel == 16)
						throw ns_ex("16 bit image in clipboard!");
					p.components = 1+2*(bmp.bmBitsPixel == 24 || bmp.bmBitsPixel == 32);
					p.height = bmp.bmHeight;
					p.width = bmp.bmWidth;
					current_image.prepare_to_recieve_image(p);
					HDC dev = GetDC(NULL);

					char * buff = new char[sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD)];
					LPBITMAPINFO bmpinfo = (LPBITMAPINFO)buff;
					bmpinfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
					bmpinfo->bmiHeader.biWidth = p.width; 
					bmpinfo->bmiHeader.biHeight = p.height;
					bmpinfo->bmiHeader.biPlanes = 1; 
					bmpinfo->bmiHeader.biBitCount = p.components*8;
					bmpinfo->bmiHeader.biCompression = BI_RGB;
					bmpinfo->bmiHeader.biSizeImage = ((p.width * p.components*8 +31) & ~31) /8
										  * p.height; 

					bmpinfo->bmiHeader.biXPelsPerMeter = 0;
					bmpinfo->bmiHeader.biYPelsPerMeter = 0;
					bmpinfo->bmiHeader.biClrUsed = 0;
					bmpinfo->bmiHeader.biClrImportant = 0;

					if (p.components == 1){
						for (unsigned int y = 0; y < p.height; y++)
							if (GetDIBits(dev,hbmp,y,1,(LPVOID)current_image[p.height-y-1],bmpinfo,DIB_RGB_COLORS) != 1)
								throw ns_ex("Error copying bitmap from clipboard");
					}
					else{
						for (unsigned int y = 0; y < p.height; y++){
							if (GetDIBits(dev,hbmp,y,1,(LPVOID)current_image[p.height-y-1],bmpinfo,DIB_RGB_COLORS) != 1)
								throw ns_ex("Error copying bitmap from clipboard");
							for (unsigned int x = 0; x < p.width; x++){
								ns_8_bit t = current_image[p.height-y-1][3*x + 2];
								current_image[p.height-y-1][3*x + 2] = current_image[p.height-y-1][3*x];
								current_image[p.height-y-1][3*x] = t;
							}
						}
					}
					delete[] buff;
					draw();
					handled_object = true;
					break;
				}
				case CF_HDROP	:{
		
					HDROP h = (HDROP)GetClipboardData(CF_HDROP);
					unsigned long num = DragQueryFile(h,0xFFFFFFFF,NULL,0);
					if (num == 0)
						break;
					//cerr << num << "\n";
					for (unsigned int i = 0; i < num; i++){
						UINT length = DragQueryFile(h,i,NULL,0);
						std::string fname;
						char * filename = new char[length+1];
						try{
							DragQueryFile(h,i,filename,length+1);
							fname = filename;
							delete filename;
						}
						catch(...){
							delete filename;
							throw;
						}
						handle_file_request(fname);
					}
					handled_object = true;
					break;
				}
				/*case CF_DIB	:{unknown_formats.push_back("CF_DIB");break;}
				case CF_DIBV5:{unknown_formats.push_back("CF_DIBV5");break;}
				case CF_DIF	:{unknown_formats.push_back("CF_DIF");break;}
				case CF_DSPBITMAP:{unknown_formats.push_back("CF_DSPBITMAP");	break;}
				case CF_DSPENHMETAFILE:{unknown_formats.push_back("CF_DSPENHMETAFILE");break;}	
				case CF_DSPMETAFILEPICT:{unknown_formats.push_back("CF_DSPMETAFILEPICT");break;}
				case CF_DSPTEXT	:{unknown_formats.push_back("CF_DSPTEXT");break;}
				case CF_ENHMETAFILE	:{unknown_formats.push_back("CF_ENHMETAFILE");break;}
				case CF_LOCALE	:{unknown_formats.push_back("CF_LOCALE");break;}
				case CF_METAFILEPICT:{unknown_formats.push_back("CF_METAFILEPICT");break;}	
				case CF_OEMTEXT	:{unknown_formats.push_back("CF_OEMTEXT");break;}
				case CF_OWNERDISPLAY:{unknown_formats.push_back("CF_OWNERDISPLAY");break;}
				case CF_PALETTE	:{unknown_formats.push_back("CF_PALETTE");break;} 	
				case CF_PENDATA	:{unknown_formats.push_back("CF_PENDATA");break;}
				case CF_RIFF:{unknown_formats.push_back("CF_RIFF");break;}
				case CF_SYLK:{unknown_formats.push_back("CF_SYLK");break;}	
				case CF_TEXT:{unknown_formats.push_back("CF_TEXT");break;}	
				case CF_WAVE:{unknown_formats.push_back("CF_WAVE");break;}
				case CF_TIFF:{unknown_formats.push_back("CF_TIFF");break;}	
				case CF_UNICODETEXT:{unknown_formats.push_back("CF_UNICODETEXT");break;}*/
				default:
					char a[100];
					if (GetClipboardFormatName(clipboard_format,a,100) != 0)
						unknown_formats.push_back(a);
					else unknown_formats.push_back(ns_to_string(std::string("Unknown #") + ns_to_string(clipboard_format)));
			}
		}
		if (!handled_object){
			cout << "Could not parse clipboard objects with format(s):\n";
			for (unsigned int i = 0; i < unknown_formats.size(); i++)
				cout << "\t" << unknown_formats[i] << "\n";
		}
		CloseClipboard();
	}
	catch(...){
		CloseClipboard();
		throw;
	}
#endif // _WIN32
}

void ns_worm_learner::run_binary_morpholgical_manipulations(){
	std::stack<ns_vector_2i> stack;
	ns_image_bitmap b;
	ns_remove_small_holes(current_image,110,stack,b);
	draw();
}

void ns_worm_learner::copy_to_clipboard(){
#ifndef _WIN32
	throw ns_ex("Clipboard currently only supported on Windows");
#else
	if (OpenClipboard(NULL) == 0)
		throw ns_ex("Could not open clipboard");
	EmptyClipboard();
	CloseClipboard();
	if (OpenClipboard(NULL) == 0)
		throw ns_ex("Could not open clipboard");
	try{
		BITMAPINFO * bmp_info =  current_image.create_GDI_bitmapinfo();
		/*if (NULL == SetClipboardData(CF_BITMAP,(HANDLE)bmp)){
			ns_ex ex("Could not copy image to clipboard:");
			ex.append_windows_error();
			throw ex;
		}*/
		if (NULL == SetClipboardData(CF_DIB,(HANDLE)bmp_info)){
			ns_ex ex("Could not copy image information to the clipboard:");
			ex.append_windows_error();
			throw ex;
		}
		CloseClipboard();
	}
	catch(ns_ex & ex){
		cerr << ex.text() << "\n";
		CloseClipboard();
	}
	catch(...){
		cerr << "Unknown Exception";
		CloseClipboard();
	}
#endif // _WIN32
}
struct ns_subregion_source_info{
	unsigned long region_id,x,y,w,h;
};
ns_subregion_source_info ns_get_subregion_source_information(istream & in){
	ns_subregion_source_info si;
	std::string tmp;
	in >> tmp;
	ns_to_lower(tmp);
	if (tmp != "region_id")
		throw ns_ex("Malformed subregion specification file: ") << tmp;
	in >> tmp;
	si.region_id = atol(tmp.c_str());
	in >> tmp;
	ns_to_lower(tmp);
	if(tmp!="subregion_x_y_w_h")
		throw ns_ex("Malformed subregion specification file: ") << tmp;
	in >> si.x;
	in >> si.y;
	in >> si.w;
	in >> si.h;
	if (in.fail())
		throw ns_ex("Malformed subregion specification file: EOF Found before subregion specification");
	return si;
}

void ns_worm_learner::output_subregion_as_test_set(const std::string & old_info){
	ifstream in(old_info.c_str());
	if (in.fail())
		throw ns_ex("Could not open subregion specification file: ") << old_info;
	ns_subregion_source_info si(ns_get_subregion_source_information(in));
	in.close();
	ns_sql * sql = image_server.new_sql_connection(__FILE__,__LINE__);
	try{
		*sql << "SELECT sample_id,name FROM sample_region_image_info WHERE id=" << si.region_id;
		ns_sql_result res;
		sql->get_rows(res);
		if(res.size() == 0)
			throw ns_ex("Could find region ") << si.region_id;
		unsigned long sample_id = atol(res[0][0].c_str());
		std::string region_name = res[0][1];

		*sql << "SELECT image_id,capture_time FROM sample_region_images WHERE region_info_id = " << si.region_id << " ORDER BY capture_time ASC";
		sql->get_rows(res);
		if(res.size() == 0)
			throw ns_ex("Could not load any rows for region ") << region_name;
		ns_image_standard temp;
		ns_image_standard temp_out;
		for (unsigned int i = 0; i < res.size(); i++){
			unsigned long image_id = atol(res[i][0].c_str());
			unsigned long capture_time = atol(res[i][1].c_str());

			ns_image_server_image im;
			im.load_from_db(image_id,sql);
			ns_image_storage_source_handle<ns_8_bit> handle = image_server.image_storage.request_from_storage(im,sql);
			handle.input_stream().pump(temp,1024);
			if (si.x + si.w >= temp.properties().width || si.y + si.h >= temp.properties().height)
				throw ns_ex("Invalid subregion (x,y,w,h)=") << si.x << "(" << si.y << "," << si.w <<"," << si.h << ")" << " in image with dimensions " << temp.properties().width << "," << temp.properties().height;
			ns_image_properties prop(temp.properties());
			prop.width = si.w;
			prop.height = si.h;
			temp_out.prepare_to_recieve_image(prop);
			for (unsigned long y_ = 0; y_ < si.h; y_++)
				for (unsigned long x_ = 0; x_ < prop.components*si.w; x_++)
					temp_out[y_][x_] = temp[si.y+y_][prop.components*si.x+x_];
			std::string dir = ns_dir::extract_path(old_info);
			std::string fname = dir + DIR_CHAR_STR + region_name + "=" + ns_format_time_string(capture_time) + "=" + ns_to_string(capture_time) + ".tif";
			cerr << "Outputting " << fname << "\n";		
			ns_save_image(fname,temp_out);
		}

		delete sql;
	}
	catch(...){
		delete sql;
		throw;
	}

}
struct ns_debug_set_file_info{
	ns_debug_set_file_info(const std::string & dir_, const std::string & filename_){
		int state(0);
		dir = dir_;
		filename = filename_;
		std::string tmp;
		for (unsigned int i = 0; i < filename_.size(); i++){
			char c = filename_[i];
			if (c == '='){
				state++; continue;
			}
			switch(state){
				case 0:  //source region name
				case 1:  //human readable date
				case 2: break; //human readable time
				case 3:
					if (c == '.'){
						capture_time = atol(tmp.c_str());
						if (capture_time == 0)
							throw ns_ex("Could not parse capture time: ") << filename_ << "(" <<tmp << ")";
						return;
					}
					else
						tmp+=c;
					break;
				default: throw ns_ex("Could not parse filename ") << filename_;
			}
		}
		throw ns_ex("Reached end of filename before capture time was found: ") << filename_;
	}
	std::string full_path() const{return dir + DIR_CHAR + filename;}
	std::string dir;
	std::string filename;
	unsigned long capture_time;
	unsigned long region_id;
};

void ns_worm_learner::input_subregion_as_new_experiment(const std::string &new_info){
	/*ifstream in(new_info.c_str());
	if (in.fail())
		throw ns_ex("Could not open subregion specification file: ") << new_info;
	ns_get_subregion_source_information(in);
	std::string tmp,experiment_name, region_name;
	in >> tmp;
	if (tmp != "new_experiment_name")
			throw ns_ex("Malformed subregion specification file: ") << tmp;
	in >> experiment_name;
	in >> tmp;
	if (tmp != "new_region_name")
		throw ns_ex("Malformed subregion specification file: ") << tmp;
	in >> region_name;
	if (in.fail())
		throw ns_ex("Malformed subregion specification file: Premature EOF");
	in >> tmp;
	bool clear_old_experiment(false);
	if (!in.fail() && tmp == "clear_old_experiment"){
		in >> tmp;
		if (in.fail())
			throw ns_ex("Malformed subregion specification file: Premature EOF after clear_old_experiment");
		clear_old_experiment = (tmp == "1");
	}
	in.close();

	ns_dir dir;
	dir.load_masked(ns_dir::extract_path(new_info),"tif",dir.files);
	std::vector<ns_debug_set_file_info> frames;
	frames.reserve(dir.files.size());
	for (unsigned int i = 0; i < dir.files.size(); i++){
		frames.push_back(ns_debug_set_file_info(ns_dir::extract_path(new_info),dir.files[i]));
		cerr << frames[i].filename << " (" << ns_format_time_string(frames[i].capture_time) << ")\n";
		ifstream in(frames[i].full_path().c_str());
		if (in.fail())
			throw ns_ex("Could not open ") << frames[i].full_path();
		in.close();
	}
	if (frames.size() < 3)
		throw ns_ex("Not enough frames: " ) << (unsigned long)frames.size() << " loaded.";




	cerr << frames.size() << " files loaded\n";

	unsigned long max_short_capture_interval = (3*(frames[1].capture_time - frames[0].capture_time))/2;
	unsigned long min_long_capture_interval = (3*(frames[2].capture_time - frames[0].capture_time))/4;
	const std::string device_name("debug_gen");
	cerr << "Max short capture interval: " << max_short_capture_interval << "\n";
	cerr << "Min long capture interval: " << min_long_capture_interval << "\n";
	//return;
	ns_sql * sql = image_server.new_sql_connection(__FILE__,__LINE__);
	try{
		//make experiment
		*sql << "SELECT id FROM experiments WHERE name='" << experiment_name << "'";
		ns_sql_result res;
		sql->get_rows(res);
		unsigned long experiment_id;
		if (res.size() == 0){
			*sql << "INSERT INTO experiments SET name='" << experiment_name << "', description='Automatically generated debug set', hidden=0";
			experiment_id = sql->send_query_get_id();
		}
		else{
			if (!clear_old_experiment)
				throw ns_ex("Experiment ") << experiment_name << " already exists.  No deletion specified.";
			experiment_id = atol(res[0][0].c_str());
		}
		*sql << "DELETE images FROM images,sample_region_images,sample_region_image_info,capture_samples WHERE images.id = sample_region_images.image_id AND sample_region_images.region_info_id = sample_region_image_info.id AND sample_region_image_info.sample_id = capture_samples.id AND capture_samples.experiment_id=" << experiment_id;
		sql->send_query();
		*sql << "DELETE sample_region_images FROM sample_region_images,sample_region_image_info,capture_samples WHERE sample_region_images.region_info_id = sample_region_image_info.id AND sample_region_image_info.sample_id = capture_samples.id AND capture_samples.experiment_id=" << experiment_id;
		sql->send_query();
		*sql << "DELETE sample_region_image_info FROM sample_region_images,sample_region_image_info,capture_samples WHERE sample_region_image_info.sample_id = capture_samples.id AND capture_samples.experiment_id=" << experiment_id;
		sql->send_query();
		*sql << "DELETE FROM capture_samples WHERE experiment_id = " << experiment_id;
		sql->send_query();
		*sql << "INSERT INTO capture_samples SET experiment_id = " << experiment_id << ", name = '" << region_name << "', device_name='" << device_name << "', short_capture_interval=" << max_short_capture_interval << ", long_capture_interval = " << min_long_capture_interval;
		unsigned long sample_id = sql->send_query_get_id();
		
		*sql << "INSERT INTO sample_region_image_info SET sample_id = " << sample_id << ", name='" << region_name << "'";
		unsigned long region_info_id = sql->send_query_get_id();
		
		for (unsigned int i = 0; i < frames.size(); i++){
			ns_image_server_captured_image captured_image;
			captured_image.experiment_id = experiment_id;
			captured_image.sample_id = sample_id;
			captured_image.capture_time = frames[i].capture_time;
			captured_image.device_name = device_name;
			captured_image.save(sql);
			ofstream *out = image_server.image_storage.request_binary_output(captured_image,false,sql);
			captured_image.save(sql);
			ifstream in(frames[i].full_path().c_str(),std::ios::binary);
			if (in.fail())
				throw ns_ex("Could not load ") << frames[i].full_path();
			*out << in.rdbuf();
			in.close();
			out->close();
			ns_image_server_captured_image_region region_image;
			region_image.experiment_id = experiment_id;
			region_image.sample_id = sample_id;
			region_image.region_info_id = region_info_id;
			region_image.device_name = device_name;
			region_image.capture_time = frames[i].capture_time;
			region_image.region_images_image_id = captured_image.capture_images_image_id;
			region_image.captured_images_id = captured_image.captured_images_id;
			region_image.save(sql);
		}

		delete sql;
	}
	catch(...){
		delete sql;
		throw;
	}
	*/	
}



/*void ns_worm_learner::run_temporal_inference(){
	unsigned int region_info_id = 80;
	ns_sql * sql = image_server.new_sql_connection("",0);
	try{
		*sql << "SELECT r.id FROM sample_region_image_info as r, capture_samples as s WHERE r.sample_id = s.id AND s.experiment_id = 23";
		ns_sql_result res;
		sql->get_rows(res);
		res.resize(1);
		res[0][0] = "55";
		for (unsigned int i = 0; i < res.size(); i++){
			try{
				unsigned long region_id = atol(res[i][0].c_str());
				ns_worm_multi_frame_interpolation mfi;
				mfi.clear_previous_interpolation_results(region_id,*sql);
				mfi.load_all_region_worms(region_id,*sql);
				mfi.run_analysis(0,*sql);
			}
			catch(ns_ex & ex){
				cerr << ex.text() << "\n";
			}
		}
		draw();
		delete sql;
	}
	catch(...){
		delete sql;
		throw;
	}

}*/
void ns_worm_learner::to_bw(){
	//convert current image to grayscale.
	if (current_image.properties().components != 1){
		//current_image->color_converter(b&w)->current_image_indirect->current_image
		ns_image_standard_indirect current_indirect(current_image);
		ns_image_stream_color_converter<ns_8_bit, ns_image_standard::storage_type>color_converter(128);
		color_converter.set_output_components(1);
		ns_image_stream_binding<ns_image_stream_color_converter<ns_8_bit, ns_image_standard::storage_type>,
								ns_image_standard_indirect > to_black_and_white(color_converter,current_indirect,128);
		current_image.pump(to_black_and_white,128);
	}
}

void ns_worm_learner::to_color(){
	//convert current image to grayscale.
	if (current_image.properties().components != 3){
		//current_image->color_converter(b&w)->current_image_indirect->current_image
		ns_image_standard_indirect current_indirect(current_image);
		ns_image_stream_color_converter<ns_8_bit, ns_image_stream_static_offset_buffer<ns_8_bit> >color_converter(128);
		color_converter.set_output_components(3);
		ns_image_stream_binding<ns_image_stream_color_converter<ns_8_bit, ns_image_stream_static_offset_buffer<ns_8_bit> >,
								ns_image_standard_indirect > to_color(color_converter,current_indirect,128);
		current_image.pump(to_color,128);
	}
}

/*
void make_experiment_from_dir(const std::string & filename){
	std::string path = ns_dir::extract_path(filename);
	ns_dir dir;
	std::vector<std::string> filenames;
	dir.load_masked(path,".tif",filenames);
	if (filenames.size() == 0)
		throw ns_ex("No files found in directory ") << path;
	
	std::string experiment_name = ns_dir::extract_filename_without_extension(filename);
	if (experiment_name.size() == 0)
		experiment_name = "processing_experiment";

	ns_sql * sql = image_server.new_sql_connection(__FILE__,__LINE__);

	*sql << "INSERT into experiments SET name = '" << experiment_name << "'";
	unsigned int id = sql->send_query_get_id();

	current_image.pump(detection_spatial_median,512);
}*/


void ns_worm_learner::make_object_collage(){
	if (worm_detection_results == 0)
		worm_detection_results = worm_detector.run(0,0,current_image,detection_brightfield,detection_spatial_median,
		static_mask,
		ns_worm_detection_constants::get(ns_worm_detection_constant::minimum_worm_region_area,current_image.properties().resolution),
		ns_worm_detection_constants::get(ns_worm_detection_constant::maximum_worm_region_area,current_image.properties().resolution),
		ns_worm_detection_constants::get(ns_worm_detection_constant::maximum_region_diagonal,current_image.properties().resolution),
		(*model_specification)().model_specification,ns_worm_detection_constants::get(ns_worm_detection_constant::maximum_number_of_actual_worms_per_image),
		0,
		"spine_debug_output");
	ns_image_standard bmp;
	current_image.pump(bmp,1024);
	worm_detection_results->generate_region_collage(detection_brightfield,detection_spatial_median,bmp).pump(current_image,1024);
	draw_image(-1,-1,current_image);
}
void ns_output_svgs(std::vector<ns_svg> & objects,const std::string & path, const bool reject=false){
	
		ns_dir::create_directory_recursive(path);
		for (unsigned int i = 0; i < objects.size(); i++){
			std::string f(path + DIR_CHAR_STR);
			if (reject) f+= "r";
			f+="object_" + ns_to_string(i) + ".svg";
			ofstream o(f.c_str());
			if (o.fail())
				throw ns_ex("Could not open ") << f;
			objects[i].compile(o);
			o.close();
		}

}
void ns_worm_learner::make_spine_collage(const bool svg,const std::string & svg_directory){
	if (worm_detection_results == 0)
		worm_detection_results = worm_detector.run(0,0,current_image,detection_brightfield,
		detection_spatial_median,
		static_mask,
		ns_worm_detection_constants::get(ns_worm_detection_constant::minimum_worm_region_area,current_image.properties().resolution),
		ns_worm_detection_constants::get(ns_worm_detection_constant::maximum_worm_region_area,current_image.properties().resolution),
		ns_worm_detection_constants::get(ns_worm_detection_constant::maximum_region_diagonal,current_image.properties().resolution),
		(*model_specification)().model_specification,ns_worm_detection_constants::get(ns_worm_detection_constant::maximum_number_of_actual_worms_per_image),
			0,"spine_debug_output",ns_detected_worm_info::ns_vis_both);
	worm_detection_results->create_spine_visualizations(current_image);
	draw_image(-1,-1,current_image);
	if (svg){
		std::vector<ns_svg> objects;
		worm_detection_results->create_spine_visualizations(objects);
		ns_output_svgs(objects,svg_directory);
	}
}
void ns_worm_learner::make_spine_collage_with_stats(const bool svg,const std::string & svg_directory){
	if (worm_detection_results == 0)
		worm_detection_results = worm_detector.run(0,0,current_image,detection_brightfield,
		detection_spatial_median,
		static_mask,
		ns_worm_detection_constants::get(ns_worm_detection_constant::minimum_worm_region_area,current_image.properties().resolution),
		ns_worm_detection_constants::get(ns_worm_detection_constant::maximum_worm_region_area,current_image.properties().resolution),
		ns_worm_detection_constants::get(ns_worm_detection_constant::maximum_region_diagonal,current_image.properties().resolution),
		(*model_specification)().model_specification,ns_worm_detection_constants::get(ns_worm_detection_constant::maximum_number_of_actual_worms_per_image),
			0,
			"spine_debug_output",ns_detected_worm_info::ns_vis_both);
	worm_detection_results->create_spine_visualizations_with_stats(current_image);
	draw_image(-1,-1,current_image);
	if (svg){
		std::vector<ns_svg> objects;
		worm_detection_results->create_spine_visualizations_with_stats(objects);
		ns_output_svgs(objects,svg_directory);
	}
}

void ns_worm_learner::show_edges(){
	if (worm_detection_results == 0)
		worm_detection_results = worm_detector.run(0,0,current_image,detection_brightfield,
		detection_spatial_median,
		static_mask,
		ns_worm_detection_constants::get(ns_worm_detection_constant::minimum_worm_region_area,current_image.properties().resolution),
		ns_worm_detection_constants::get(ns_worm_detection_constant::maximum_worm_region_area,current_image.properties().resolution),
		ns_worm_detection_constants::get(ns_worm_detection_constant::maximum_region_diagonal,current_image.properties().resolution),
		(*model_specification)().model_specification,ns_worm_detection_constants::get(ns_worm_detection_constant::maximum_number_of_actual_worms_per_image),
			0,"spine_debug_output",ns_detected_worm_info::ns_vis_both);
	worm_detection_results->create_edge_visualization(current_image);
	draw_image(-1,-1,current_image);
}

void ns_worm_learner::make_reject_spine_collage(const bool svg,const std::string & svg_directory){
	if (worm_detection_results == 0)
		worm_detection_results = worm_detector.run(0,0,current_image,detection_brightfield,
		detection_spatial_median,
		static_mask,
		ns_worm_detection_constants::get(ns_worm_detection_constant::minimum_worm_region_area,current_image.properties().resolution),
		ns_worm_detection_constants::get(ns_worm_detection_constant::maximum_worm_region_area,current_image.properties().resolution),
		ns_worm_detection_constants::get(ns_worm_detection_constant::maximum_region_diagonal,current_image.properties().resolution),
		(*model_specification)().model_specification,ns_worm_detection_constants::get(ns_worm_detection_constant::maximum_number_of_actual_worms_per_image),
			0,"spine_debug_output",ns_detected_worm_info::ns_vis_both);
	worm_detection_results->create_reject_spine_visualizations(current_image);
	if (svg){
		std::vector<ns_svg> objects;
		worm_detection_results->create_reject_spine_visualizations(objects);
		ns_output_svgs(objects,svg_directory,true);
	}
	draw_image(-1,-1,current_image);
}

void ns_worm_learner::make_reject_spine_collage_with_stats(const bool svg,const std::string & svg_directory){
	if (worm_detection_results == 0)
		worm_detection_results = worm_detector.run(0,0,current_image,detection_brightfield,
		detection_spatial_median,
		static_mask,
		ns_worm_detection_constants::get(ns_worm_detection_constant::minimum_worm_region_area,current_image.properties().resolution),
		ns_worm_detection_constants::get(ns_worm_detection_constant::maximum_worm_region_area,current_image.properties().resolution),
		ns_worm_detection_constants::get(ns_worm_detection_constant::maximum_region_diagonal,current_image.properties().resolution),
		(*model_specification)().model_specification,ns_worm_detection_constants::get(ns_worm_detection_constant::maximum_number_of_actual_worms_per_image),
			0,
			"spine_debug_output",ns_detected_worm_info::ns_vis_both);
	worm_detection_results->create_reject_spine_visualizations_with_stats(current_image);
	if (svg){
		std::vector<ns_svg> objects;
		worm_detection_results->create_reject_spine_visualizations_with_stats(objects);
		ns_output_svgs(objects,svg_directory,true);
	}
	draw_image(-1,-1,current_image);
}

void ns_worm_learner::calculate_vertical_offset(const std::string & filename){
	throw ns_ex("Not implemented");
	/*ns_image_standard second;
	load_file(filename, second);
	ns_vector_2i diff = ns_image_registration<128,ns_8_bit>::register_images(current_image,second,ns_compound_registration);
	cerr << "Offset = " << diff << "\n";
	ns_image_standard im;
	ns_image_properties prop = second.properties();
	ns_vector_2i d = ns_vector_2i(abs(diff.x),abs(diff.y));
	prop.height-=d.y*2;
	prop.width-=d.x*2;
	im.prepare_to_recieve_image(prop);

	for (unsigned int y =d.y; y < prop.height-d.y; y++){
		for (unsigned int x =d.x; x < (prop.width-d.x)*prop.components; x++)
			im[y][x] = abs((int)current_image[y][x] - (int)second[y+diff.y][x+diff.x]);
	}
	im.pump(current_image,512);
	draw_image(-1,-1,current_image);*/
}
void ns_worm_learner::difference_threshold(){
	cerr << "Running difference threshold.";
	to_bw();
	//find correct threshold level and use it.
	//current_image->thresholder->current_image_indirect->current_image
	ns_acquire_lock_for_scope lock(font_server.default_font_lock, __FILE__, __LINE__);
	font_server.get_default_font().set_height(48);
	unsigned int res = 10;
	std::vector<ns_image_standard> images(res*res );
	#pragma omp parallel for
	for (int i = 0; i < (int)res; i++)
		for (unsigned int j = 0; j < res; j++){
			int difference = 800+200*(i),
			kernel_size = 4+j;
			ns_difference_thresholder::run(current_image, images[i+res*j],difference,kernel_size,0);
			ns_text_stream_t s;
			s << "D=" << difference << " K = " << kernel_size;
			font_server.get_default_font().draw(0,60,ns_color_8(155,155,155),s.text(),images[i+res*j]);
		}
	lock.release();
	ns_make_collage(images, current_image, 512);

	/*
	ns_image_standard thresholded_image;
	//ns_region_growing_segmenter(current_image, thresholded_image,1, 1,.5);
	ns_difference_thresholder thresholder;
	thresholder.run(current_image, thresholded_image,60,12);
	thresholded_image.pump(current_image,128);
	*/
	draw_image(-1,-1,current_image);
}



void ns_worm_learner::calculate_erosion_gradient(){
	/*ns_erosion_intensity_profiler profiler;
	ofstream o("c:\\steps.csv");
	vector<double> steps;
	for (unsigned int i = 1; i < 32; i*=2){
		profiler.get_profile(i,current_image,steps);
		o << i << ",";
		for (unsigned int j = 0; j < steps.size(); j++)
			o << steps[j] << ",";
		o << "\n";
	}
	o.close();*/
}

void ns_worm_learner::apply_threshold(){

	/*ns_histogram<unsigned int, ns_8_bit> hist = current_image.histogram();
	int m;
	for (m= hist.size(); m >= 0; m--){
		if (hist[m] != 0)
			break;
	}

	ofstream outp("c:\\hist.txt");
	for (unsigned int i = 0; i < m; i++)
		outp << i << "\t" << hist[i] << "\n";
	outp.close();
	return;*/

	ns_image_standard_indirect current_indirect(current_image);

	to_bw();
	//find correct threshold level and use it.
	//current_image->thresholder->current_image_indirect->current_image

	//ns_image_standard thresholded_image;
	//ns_region_growing_segmenter(current_image, thresholded_image,1, 1,.5);
	//ns_difference_thresholder(current_image, thresholded_image,70,3);
	//thresholded_image.pump(current_image,128);
	
	ns_8_bit thresh = 1;//find_adaptive_threshold<ns_8_bit,ns_image_standard>(current_image);
	
	//ns_8_bit thresh = find_adaptive_threshold_gradient<ns_8_bit,ns_image_standard>(current_image);
	//cout << "Final Threshold = " << (int)thresh << "\n";

	ns_image_stream_apply_threshold<ns_8_bit>
		thresholder(128);

	ns_image_stream_binding<ns_image_stream_apply_threshold<ns_8_bit>, ns_image_standard_indirect >
		bound_threshold(thresholder,current_indirect,128);

	thresholder.set_threshold(thresh);

	current_image.pump(bound_threshold,128);

	draw_image(-1,-1,current_image);
    
}
void ns_worm_learner::view_current_mask(){
	if (!mask_loaded())
		throw ns_ex("No mask is currently loaded.");
	current_mask.pump(current_image,128);
	draw_image(-1,-1,current_image);
}

void ns_worm_learner::apply_mask_on_current_image(){
	if (!mask_loaded())
		throw ns_ex("No mask is currently loaded.");

	//image->stream_splitter->current_image
	//                      ->mask_splitter

	ns_image_stream_mask_splitter<ns_8_bit, ns_image_stream_file_sink<ns_8_bit> > region_splitter(4096);

	*region_splitter.mask_info() = mask_analyzer.mask_info();
	region_splitter.specify_mask(current_mask);
	
	current_image.pump(region_splitter,4096);

}

void ns_worm_learner::apply_spatial_average(){
	to_bw();
	//store for worm detection
	current_image.pump(detection_brightfield,512);

	ns_spatial_median_calculator<ns_8_bit,true> spatial_averager(1024,ns_image_processing_pipeline_spatial_average_kernal_width);

	ns_image_standard_indirect current_indirect(current_image);

	ns_image_stream_binding<
		ns_spatial_median_calculator<ns_8_bit,true>,
		ns_image_standard_indirect >
		spatial_averager_bound(spatial_averager, current_indirect,1024);

	current_image.pump(spatial_averager_bound,1024);
	
	ns_crop_lower_intensity<ns_8_bit>(current_image,(ns_8_bit)ns_worm_detection_constants::get(ns_worm_detection_constant::tiff_compression_intensity_crop_value,current_image.properties().resolution));
	//store for worm detection
	current_image.pump(detection_spatial_median,512);

	draw_image(-1,-1,current_image);

}

void ns_worm_learner::remove_large_objects(){
	ns_image_standard opened;
	ns_close<5>(current_image,opened);

	ns_detected_object_manager manager;
	
	ns_identify_contiguous_bitmap_regions(opened,manager.objects);
	manager.constrain_region_area(ns_worm_detection_constants::get(ns_worm_detection_constant::connected_object_area_cutoff,current_image.properties().resolution),
										opened.properties().width*opened.properties().height,
										(unsigned int)sqrt((float)(opened.properties().width*opened.properties().width + opened.properties().height*opened.properties().height)+1));

	for (unsigned int i = 0; i < manager.objects.size(); i++){		
		unsigned int w = manager.objects[i]->bitmap().properties().width,
					 h = manager.objects[i]->bitmap().properties().height;
		ns_image_bitmap & bitmap(manager.objects[i]->bitmap());
		for (unsigned int y = 0; y < h; y++)
			for (unsigned int x = 0; x < w; x++)
				if (bitmap[y][x])
					current_image[manager.objects[i]->offset_in_source_image.y + y][manager.objects[i]->offset_in_source_image.x + x] = 0;
	}

	//for (unsigned int y = 0; y < current_image.properties().height; y++)
	//	for (unsigned int x = 0; x < current_image.properties().width; x++)
	//		current_image[y][x]*=255;
	draw_image(-1,-1,current_image);
}

void ns_worm_learner::stretch_levels(){
	::stretch_levels(current_image,current_image.histogram(),0);
	draw_image(-1,-1,current_image);
}

void ns_worm_learner::stretch_levels_approx(){
	::stretch_levels(current_image,current_image.histogram(),.01);
	draw_image(-1,-1,current_image);
}

void ns_worm_learner::detect_and_output_objects(){
	ns_detected_object_manager object_manager;
	ns_identify_contiguous_bitmap_regions(current_image,object_manager.objects);
	std::vector<ns_image_bitmap *> bmps(object_manager.objects.size());
	for (unsigned int i = 0; i < object_manager.objects.size(); i++){
		bmps[i] = &object_manager.objects[i]->bitmap();
	
	}
	ns_make_collage(bmps,current_image,1024);
	stretch_levels();
	
	draw_image(-1,-1,current_image);

}

void ns_worm_learner::set_static_mask(ns_image_standard & im){
	if (static_mask == 0)
		static_mask = new ns_image_standard;
	im.pump(*static_mask,1024);
}
void ns_worm_learner::process_contiguous_regions(){
	if (worm_detection_results != 0)
		delete worm_detection_results;	
	unsigned long start_time = ns_current_time();

	if (detection_brightfield.properties().height == 0)
		apply_spatial_average();
	if (thresholded_image.properties().height == 0)
		two_stage_threshold();

	std::string debug_file("");		
	worm_detection_results = worm_detector.run(0,0,detection_brightfield,thresholded_image,
		detection_spatial_median,
		static_mask,
		ns_worm_detection_constants::get(ns_worm_detection_constant::minimum_worm_region_area,current_image.properties().resolution),
		ns_worm_detection_constants::get(ns_worm_detection_constant::maximum_worm_region_area,current_image.properties().resolution),
		ns_worm_detection_constants::get(ns_worm_detection_constant::maximum_region_diagonal,current_image.properties().resolution),
		(*model_specification)().model_specification,1000,0,debug_file,ns_detected_worm_info::ns_vis_both);

	cerr << "min object size: " << 
	ns_worm_detection_constants::get(ns_worm_detection_constant::minimum_worm_region_area,current_image.properties().resolution) << "; max object size: " << 
		ns_worm_detection_constants::get(ns_worm_detection_constant::maximum_worm_region_area,current_image.properties().resolution)<< "; max object diagonal: " << 
		ns_worm_detection_constants::get(ns_worm_detection_constant::maximum_region_diagonal,current_image.properties().resolution) << " model file: " << (*model_specification)().model_specification.model_name << "\n";


	cout << worm_detection_results->actual_worm_list().size() << " out of " << worm_detection_results->number_of_putative_worms() << " potential objects were identified as worms.\n";

	cout << "Reasons for rejection: \n";
	std::map<std::string,unsigned long> reasons = worm_detection_results->give_worm_rejection_reasons();
	cout << "Reason : # of worms rejected for that reason\n";
	for (std::map<std::string,unsigned long>::iterator p = reasons.begin(); p != reasons.end(); p++){
		cout << p->first << ": " << p->second << "\n";
	}
	unsigned long stop_time = ns_current_time();
	cerr << "\nComputation time: " << stop_time - start_time << "\n";
	detection_spatial_median.pump(current_image,128);
	to_color();
	cerr << "Creating Visualization.\n";
	
	worm_detection_results->create_visualization(30,3,current_image,"", true);
	cerr << "Drawing Image.\n";
	draw_image(-1,-1,current_image);
}


void ns_worm_learner::characterize_precomputed_movement(const unsigned long short_1_id, const unsigned long short_2_id, const unsigned long long_id){
	/*ns_worm_movement_measurement_analysis_set movement_analysis;
	
	ns_sql * sql = image_server.new_sql_connection(__FILE__,__LINE__);
	try{
		ns_image_server_captured_image_region s1,s2,l;
		s1.load_from_db(short_1_id,sql);
		s2.load_from_db(short_2_id,sql);
		l.load_from_db(long_id,sql);
		movement_analysis.load_results(s1,s2,l,*sql);

		std::vector< ns_image_storage_source_handle<ns_8_bit> > sources;
		sources.reserve(3);
		sources.resize(sources.size()+1,image_server.image_storage.request_from_storage(s1,ns_process_spatial,sql));
		sources.resize(sources.size()+1,image_server.image_storage.request_from_storage(s2,ns_process_spatial,sql));
		sources.resize(sources.size()+1,image_server.image_storage.request_from_storage(l,ns_process_spatial,sql));

		//load source images from disk
		std::vector<ns_image_standard> images(sources.size());
		for (unsigned int i = 0; i < sources.size(); i++)
			sources[i].input_stream().pump(images[i],1024);

		movement_analysis.analyze_movement(true);

		//generate mapping diagram
		movement_analysis.classifier.draw_movement_map(images[0],images[1],images[2],current_image);
		draw();
		ns_image_standard out;
		movement_analysis.classifier.draw_movement_shading(images[0],out,s1.display_label(),true);	
		ns_save_image("c:\\tt\\cur.jpg",out);
		ns_save_image("c:\\tt\\posture.tif",movement_analysis.posture_visualization);
		delete sql;
	}
	catch(...){
		delete sql;
		throw;
	}*/
}

void ns_worm_learner::test_image_metatadata(){
	std::string tst; 
	tst.resize(256*1024);
	for (unsigned int i = 0; i < tst.size(); i++){
		tst[i] ='a'+i%24;
	}
	current_image.set_description(tst);
	cerr << current_image.properties().description.size();
	cerr << "...";
	save_current_image("c:\\tst.tif");
	load_file("c:\\tst.tif");
	cerr << current_image.properties().description.size();
}
void ns_worm_learner::decimate_folder(const std::string & filename){
	std::string path = ns_dir::extract_path(filename);
	cerr << "input path: " << path << "\n";
	std::string output_path = path + DIR_CHAR_STR + "decimated";
	cerr << "output path: " << output_path << "\n";
	ns_dir dir;
	dir.load_masked(path,"tif",dir.files);
	cerr << "Input file count: " << dir.files.size() << "\n";
	ns_dir::create_directory_recursive(output_path);
	for (unsigned int i = 0; i < dir.files.size(); i+=150){
		ns_dir::copy_file(path + DIR_CHAR_STR + dir.files[i], output_path + DIR_CHAR_STR + dir.files[i]);
	}
};

void ns_worm_learner::translate_f_score_file(const std::string & filename){
	ifstream in(filename.c_str());
	if (in.fail())
		throw ns_ex("Could not open ") << filename;
	std::string path = ns_dir::extract_path(filename);
	std::string ofile = path + DIR_CHAR_STR + "fscore_trans.txt";
	ofstream o(ofile.c_str());
	while(true){
		int feature;
		in >> feature;
		in.get();
	
	//	std::string feature;
	//	char a;
	//	while(true){
	//		a = in.get();
	///		if (a == ':' || in.fail()) break;
	//		feature+=a;
	//	}

		if (in.fail()) break;
		double val;
		in >> val;
		//while(a!='\n' && !in.fail())
		//	a=in.get();
		o << ns_classifier_abbreviation((ns_detected_worm_classifier)(feature-1));
		o << ":\t" << val << "\n";
	}
	in.close();
	o.close();
}
void ns_worm_learner::characterize_movement(const std::string & filename, const std::string & filename_2){
	throw ns_ex("No absolute grayscale loaded");
	/*ns_image_standard dummy;
	if (worm_detection_results == 0)
		throw ns_ex("Only images in which worms have currently been detected can be compared for movement.");
	ns_image_standard second_gray;
	load_file(filename,second_gray);
	ns_image_standard second_thresh;
	 ns_two_stage_difference_thresholder::run(second_gray,second_thresh);
	ns_image_worm_detection_results * second_results = worm_detector.run(0,0,dummy,second_thresh,
		second_gray,
		static_mask,
		ns_worm_detection_constants::get(ns_worm_detection_constant::minimum_worm_region_area,second_gray.properties().resolution),
		ns_worm_detection_constants::get(ns_worm_detection_constant::maximum_worm_region_area,second_gray.properties().resolution),
		ns_worm_detection_constants::get(ns_worm_detection_constant::maximum_region_diagonal,second_gray.properties().resolution),
		*model_specification);
	
	std::vector<ns_movement_object *> first,
								 second,
								 third;
	try{
		for (unsigned int i = 0; i < worm_detection_results->actual_worm_list().size(); i++)
			first.push_back(new ns_movement_worm_object(worm_detection_results->actual_worm_list()[i]));
		for (unsigned int i = 0; i < second_results->actual_worm_list().size(); i++)
			second.push_back(new ns_movement_worm_object(second_results->actual_worm_list()[i]));


		if (filename_2 ==""){
			ns_nearest_neighbor_map nn_map;
			
			nn_map.calculate(first,second,true);
			nn_map.draw_neighbor_map(detection_spatial_median,second_gray,current_image);
			draw_image(-1,-1,current_image);
		}
		else{
			

			ns_image_standard third_gray;
			load_file(filename_2,third_gray);
			ns_image_standard third_thresh;
			ns_two_stage_difference_thresholder::run(third_gray,third_thresh);
			ns_image_worm_detection_results * third_results = worm_detector.run(0,0,dummy,third_thresh,
				third_gray,
				static_mask,
				ns_worm_detection_constants::get(ns_worm_detection_constant::minimum_worm_region_area,third_gray.properties().resolution),
				ns_worm_detection_constants::get(ns_worm_detection_constant::maximum_worm_region_area,third_gray.properties().resolution),
				ns_worm_detection_constants::get(ns_worm_detection_constant::maximum_region_diagonal,third_gray.properties().resolution),
				*model_specification);
			try{
				for (unsigned int i = 0; i < third_results->actual_worm_list().size(); i++)
					third.push_back(new ns_movement_worm_object(third_results->actual_worm_list()[i]));
				
				ns_worm_movement_classifier classifier;
				classifier.calculate(first,second,third,true,&current_image);
				classifier.draw_movement_map(detection_spatial_median,second_gray,third_gray,current_image);
				ns_image_standard out;
				classifier.draw_movement_shading(detection_spatial_median,out);
				ns_save_image("c:\\tt\\cur.jpg",out);
				draw_image(-1,-1,current_image);
				
			}
			catch(...){
				delete third_results;
				throw;
			}
			delete third_results;
		}

		for (unsigned int i = 0; i < first.size(); i++)
			delete first[i];
		for (unsigned int i = 0; i < second.size(); i++)
			delete second[i];
		for (unsigned int i = 0; i < third.size(); i++)
			delete third[i];

	}
	catch(...){
		for (unsigned int i = 0; i < first.size(); i++)
			delete first[i];
		for (unsigned int i = 0; i < second.size(); i++)
			delete second[i];
		for (unsigned int i = 0; i < third.size(); i++)
			delete third[i];

		delete second_results;
		throw;
	}
	delete second_results;
	*/
}
void ns_worm_learner::grayscale_from_blue(){
	ns_image_standard im;
	current_image.pump(im,512);
	ns_image_properties p = im.properties();
	p.components = 1;
	current_image.prepare_to_recieve_image(p);
	for (unsigned int y = 0; y < p.height; y++)
		for (unsigned int x = 0; x < p.width; x++)
			current_image[y][x] = im[y][3*x+2];
	draw();
}

bool ns_worm_learner::load_threshold_if_possible(const std::string & filename){
	std::string f = ns_dir::extract_filename(filename);
	std::string path = ns_dir::extract_path(filename);
	cerr << filename << "\n" << path << "\n";
	std::string::size_type l = path.find_last_of("\\");
	if (l == path.npos)
		return false;
	std::string step = path.substr(l+1,std::string::npos);
	if (step != "spatial")
		return false;

	std::string::size_type ll = f.find_last_of("=");
	if (ll == path.npos)
		return false;
	std::string t_f = f.substr(0,ll);

	std::string thresh_dir = filename.substr(0,l+1) + "threshold";
	ns_dir dir;

	try{
		dir.load(thresh_dir);
	}
	catch(ns_ex & ex){
		cerr << ex.text() << "\n";;
		return false;
	}
	catch(...){
		return false;
	}
	if (dir.files.size() == 0)
		return false;

	std::string thresh;
	for (unsigned int i = 0; i < dir.files.size(); i++){
		if (dir.files[i].find(t_f) != dir.files[i].npos){
			thresh = dir.files[i];
			break;
		}
	}
	std::string thresh_full = thresh_dir + DIR_CHAR_STR + thresh;
	if (thresh.size() == 0 || !ns_dir::file_exists(thresh_full))
		return false;

	load_file(thresh_full,current_image); 
	
	return true;
}
/*
void ns_worm_learner::get_ip_and_port_for_mask_upload(std::string & ip_address,unsigned long & port){
	if (image_server.current_sql_database() != image_server.mask_upload_database)
		throw ns_ex("To upload experiments to the mask file cluster, the terminal must be set to access the database ") << image_server.mask_upload_database;
	ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__,__LINE__));
	sql() << "SELECT ip,port,database_used FROM hosts WHERE name='" << image_server.mask_upload_hostname << "'";
	ns_sql_result res;
	sql().get_rows(res);
	if (res.size() == 0)
		throw ns_ex("Could not locate remote mask server with host name ") << image_server.mask_upload_hostname;
	if (res[0][2] != image_server.mask_upload_database)
			throw ns_ex("To upload experiments to the mask file cluster, the mask server must be set to access the database ") 
			<< image_server.mask_upload_database << ".  Currently it is set to " << res[0][2];
	ip_address = res[0][0];
	port = atol(res[0][1].c_str());
}
*/
ns_mask_info ns_worm_learner::send_mask_to_server(const ns_64_bit & sample_id){
	ns_mask_info mask_info;
	if (!mask_loaded())
		throw ns_ex("No mask is currently loaded.");
	//Connecting to image server
	//Updating database
	cerr << "\nUpdating database....";
	ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__,__LINE__));
	sql() << "INSERT INTO images SET filename = '" << current_mask_filename << "', creation_time = '" << ns_current_time() << "', "
		<< "path = '" << ns_image_server::unclaimed_masks_directory() << "', currently_under_processing=0, partition=''";
	mask_info.image_id = sql().send_query_get_id();
	sql() << "INSERT INTO image_masks SET image_id = " << mask_info.image_id << ", processed='0'";
	mask_info.mask_id = sql().send_query_get_id();
	
	ns_image_server_image image;
	image.load_from_db(mask_info.image_id, &sql());
	bool had_to_use_local_storage;
	ns_image_storage_reciever_handle<ns_8_bit> image_storage = image_server.image_storage.request_storage(image, ns_tiff, 1.0,512, &sql(), had_to_use_local_storage, false, false);

	//ns_image_standard decoded_image;
	current_mask.pump(image_storage.output_stream(), 512);
	//decoded_image.pump(sender,512);
	//c.close();


	sql() << "UPDATE capture_samples SET mask_id=" << mask_info.mask_id << " WHERE id=" << sample_id;
	sql().send_query();
	sql() << "INSERT INTO processing_jobs SET image_id=" << mask_info.image_id << ", mask_id=" << mask_info.mask_id << ", "
		<< "op" << (unsigned int)ns_process_analyze_mask << " = 1, time_submitted=" << ns_current_time() << ", urgent=1";
	sql().send_query();
	sql().send_query("COMMIT");
	
	cerr << "\nDone.\n";
	return mask_info;
     
}

void ns_worm_learner::diff_encode(const std::string & f1, const std::string & f2){
	ns_image_standard im1,im2;
	ns_image_standard * im[2]={&im1,&im2};
	const std::string * f[2]={&f1,&f2};
	std::vector<std::string> f_base(2);
	for (unsigned int i = 0; i < 2; i++){
		ns_load_image(*f[i],*im[i]);
		f_base[i] = ns_dir::extract_filename_without_extension(*f[i]);
		for (unsigned y = 0; y < im[i]->properties().height; y++)
			for (unsigned x = 0; x < im[i]->properties().width; x++)
					(*im[i])[y][x] &= 0xFE;
		ns_save_image(f_base[i]+"_crop.tif",*im[i]);
	}
	if (im[0]->properties() != im[1]->properties())
		throw ns_ex("Different sized images!");

	ns_image_standard d,d2,sign;
	d.prepare_to_recieve_image(im1.properties());
	d2.prepare_to_recieve_image(im1.properties());
	sign.prepare_to_recieve_image(im1.properties());
	for (unsigned y = 0; y < im[0]->properties().height; y++){
		for (unsigned x = 0; x < im[0]->properties().width; x++){
			int i = (int)im1[y][x] - (int)im2[y][x];
			d[y][x] = 0;
			d[y][x] |= ((unsigned char)(i < 0)) & 0x80; //set highest bit for negatives
			d[y][x] |= ((unsigned char)(1+ (i < 0)*-2)*i)>>1; //set absolute value of difference.
			sign[y][x] = 255*(i>0);
			d2[y][x] = abs(i);
		}
	}
	ns_save_image(f_base[0] + "_diff_crop.tif",d);
	ns_save_image(f_base[0] + "_diff_data.tif",d2);
	ns_save_image(f_base[0] + "_diff_sign.tif",sign);

}
/*
void ns_worm_learner::repair_image_headers(const std::string & filename){
	std::vector<std::string> correct_header_source_filenames;
	std::vector<std::string> correct_header_source_filename_paths;

	std::vector<std::string> correct_header_source_directories;
	std::string correct_source_base_dir("Z:\\stroustrup\\image_server_storage\\2007_12_12_first_high_res_run\\");
	correct_header_source_directories.push_back(correct_source_base_dir+"cf512\\training_set");
	correct_header_source_directories.push_back(correct_source_base_dir+"cf18544\\training_set");
	correct_header_source_directories.push_back(correct_source_base_dir+"tj1060\\training_set");
	correct_header_source_directories.push_back(correct_source_base_dir+"tj1062\\training_set");

	for (unsigned int i = 0; i < correct_header_source_directories.size(); i++){
		ns_dir dir;
		dir.load_masked(correct_header_source_directories[i],"tif",dir.files);
		correct_header_source_filenames.insert(correct_header_source_filenames.end(),dir.files.begin(),dir.files.end());
		for (unsigned int j = 0; j < dir.files.size(); j++)
			correct_header_source_filename_paths.push_back(correct_header_source_directories[i]);
	}

	std::string to_be_fixed_dir(ns_dir::extract_path(filename));
	ns_dir to_be_fixed;
	to_be_fixed.load_masked(to_be_fixed_dir,"tif",to_be_fixed.files);
	for (unsigned int i = 0; i < to_be_fixed.files.size(); i++){
		std::string correct_source_filename("");
		for (unsigned int j = 0; j < correct_header_source_filenames.size(); j++){
			if (correct_header_source_filenames[j] == to_be_fixed.files[i]){
				correct_source_filename = correct_header_source_filename_paths[j] + "\\" + correct_header_source_filenames[j];
				break;
			}
		}
		if (correct_source_filename.size() == 0){
			cerr << "Could not find correct header source for file " << to_be_fixed.files[i] << "\n";
			break;
		}
		cerr << "Fixing " << to_be_fixed.files[i] << "...";
		 ns_worm_training_set_image::repair_image_header(correct_source_filename,to_be_fixed_dir + "\\" + to_be_fixed.files[i]);
		//cout << to_be_fixed.files[i] << " matches " << correct_source_filename << "\n";
	}

}*/

void ns_worm_learner::generate_training_set_image(){
	if (!load_threshold_if_possible(current_filename))
		two_stage_threshold();
	process_contiguous_regions();
	
	ns_worm_training_set_image::generate(*worm_detection_results,current_image);
	draw();
}
void ns_worm_learner::process_training_set_image(){
	
	ns_annotated_training_set training_set;
	ns_worm_training_set_image::decode(current_image,training_set);
	cerr << "Found " << training_set.worms.size() << " worms and " << training_set.non_worms.size() << " non-worms out of " << training_set.objects.size() << " objects.\n";
	
	std::vector<ns_image_standard *> worms(training_set.worms.size()),
								non_worms(training_set.non_worms.size());
	for (unsigned int i = 0; i < worms.size(); i++) worms[i] = &training_set.worms[i]->object.absolute_grayscale();
	for (unsigned int i = 0; i < non_worms.size(); i++) non_worms[i] = &training_set.non_worms[i]->object.absolute_grayscale();
	ns_image_standard yes;
	ns_make_collage(worms,yes,512);
	ns_image_standard no;
	ns_make_collage(non_worms,no,512);
	ns_image_standard tmp;
	current_image.pump(tmp,512);
	std::vector<const ns_image_standard *> cl;
	cl.push_back(&tmp);
	cl.push_back(&yes);
	cl.push_back(&no);
	ns_compile_collage(cl,current_image,512,1,0,false,100);
	draw();

}
void ns_worm_learner::output_learning_set(const std::string & directory, const bool & process){
		
	if (process && current_filename != ""){
		if (!load_threshold_if_possible(current_filename))
			two_stage_threshold();
		process_contiguous_regions();
	}
	ns_image_standard im;

	std::string base_dir = ns_dir::extract_path(directory);

	std::string worm_dir = base_dir + DIR_CHAR_STR + "worm";
	std::string non_worm_dir = base_dir + DIR_CHAR_STR + "dirt";
	ns_dir::create_directory_recursive(worm_dir);
	ns_dir::create_directory_recursive(non_worm_dir);

	unsigned int image_id = 0;
	ns_dir dir;
	try{
		dir.load_masked(worm_dir,"tif",dir.files);
		image_id+=(unsigned int)dir.files.size();
	}catch(...){}

	for (unsigned int i = 0; i < worm_detection_results->number_of_actual_worms(); i++){
		std::string filename = worm_dir + DIR_CHAR_STR + "ns_w_" + ns_to_string(image_id) + ".tif";
		ns_tiff_image_output_file<ns_8_bit> tiff_out;
		ns_image_stream_file_sink<ns_8_bit> file_sink(filename,tiff_out,1.0,128);
		worm_detection_results->generate_actual_training_set_visualization(i,im);
		im.pump(file_sink,128);
		image_id++;
	}
	image_id = 0;
	try{
		dir.load_masked(non_worm_dir,"tif",dir.files);
		image_id+=(unsigned int)dir.files.size();
	}
	catch(...){}
	for (unsigned int i = 0; i < worm_detection_results->number_of_non_worms(); i++){
		std::string filename = non_worm_dir + DIR_CHAR_STR + "ns_n_" + ns_to_string(image_id) + ".tif";
		ns_tiff_image_output_file<ns_8_bit> tiff_out;
		ns_image_stream_file_sink<ns_8_bit> file_sink(filename,tiff_out,1.0,128);
		worm_detection_results->generate_non_worm_training_set_visualization(i,im);
		im.pump(file_sink,128);
		image_id++;
	}
}
#include "ns_image_easy_io.h"






void ns_worm_learner::load_mask(const std::string & filename,bool draw_to_screen){
	//masks must be tiff
	std::string extension = ns_dir::extract_extension(filename);
	if (extension != "tiff" && extension != "tif")
		throw ns_ex("Invalid file type.");
	current_mask_filename = ns_dir::extract_filename(filename);
	ns_image_server_captured_image im;
	try{
		int offset;
		im.from_filename(current_mask_filename,offset);
	}
	catch(ns_ex & ex){
		cout << "Could not guess what sample this comes from.\n";
	}
	ns_tiff_image_input_file<ns_8_bit> tiff_in;
	tiff_in.open_file(filename);

	ns_image_stream_file_source<ns_8_bit> file_source(tiff_in);
	if (file_source.properties().components != 1)
		throw ns_ex("Masks must be grayscale images!");
	mask_analyzer.register_visualization_output(current_image);

	ns_image_stream_splitter<ns_8_bit,
			ns_image_stream_static_offset_buffer<ns_8_bit>,
			ns_image_standard,
			ns_image_mask_analyzer<ns_8_bit > >
		splitter(4096);
	
	splitter.bind(current_mask, mask_analyzer);

	file_source.pump(splitter,4096);
}


void ns_worm_learner::load_file(const std::string & filename){
	//area_handler.clear_boxes();
	current_image_lock.wait_to_acquire(__FILE__,__LINE__);
	try{
		load_file(filename,current_image);
		current_image_lock.release();
	}
	catch(...){
		current_image_lock.release();
	}
}
void ns_worm_learner::save_current_image(const std::string & filename){		

	std::string extension = ns_dir::extract_extension(filename);
	//open jpeg
	if (extension == "jpg"){
		ns_jpeg_image_output_file<ns_8_bit> jpeg_out;
		ns_image_stream_file_sink<ns_8_bit > file_sink(filename,jpeg_out, NS_DEFAULT_JPEG_COMPRESSION,1024);
		current_image.pump(file_sink,128);
	}
	//open tiff
	else if (extension == "tif" || extension == "tiff"){
		ns_tiff_image_output_file<ns_8_bit> tiff_out;
		ns_image_stream_file_sink<ns_8_bit> file_sink(filename,tiff_out, 1.0,1024);
		current_image.pump(file_sink,1024);
	}
	if (extension == "jp2"){
		ns_ojp2k_image_output_file<ns_8_bit> jp2k_out;
		ns_image_stream_file_sink<ns_8_bit > file_sink(filename,jp2k_out,NS_DEFAULT_JP2K_COMPRESSION,1024);
		current_image.pump(file_sink,1024);
	}

}	

void ns_worm_learner::draw(){
	bool animation_state(ns_set_animation_state(false));

	current_image_lock.wait_to_acquire(__FILE__,__LINE__);
	try{
		switch(behavior_mode){
			case ns_worm_learner::ns_draw_boxes:
					draw_image(-1,-1,current_image);
					break;
				case ns_worm_learner::ns_annotate_death_times_in_time_aligned_posture:
				case ns_worm_learner::ns_annotate_death_times_in_death_aligned_posture:
				case ns_worm_learner::ns_annotate_storyboard_region:
				case ns_worm_learner::ns_annotate_storyboard_sample:
				case ns_worm_learner::ns_annotate_storyboard_experiment:
					current_annotater->display_current_frame();
					break;
				case ns_worm_learner::ns_annotate_death_times_in_region:

					break;
				default:
					throw ns_ex("Unkown mode");
			}
		
			current_image_lock.release();
		}
		catch(...){
			
			current_image_lock.release();
			ns_set_animation_state(animation_state);
			throw;

		}
		
	ns_set_animation_state(animation_state);
		
//	else{
	//	cerr << "Skipping Draw.\n";
	//}
}


std::ostream & operator << (std::ostream & o, const ns_death_time_annotation & a){
	if (a.time.period_start_was_not_observed)
		o << "(,";
	else o << "[" << ns_format_time_string_for_human(a.time.period_start) << ",";
	if (a.time.period_end_was_not_observed)
		o << ")";
	else o << ns_format_time_string_for_human(a.time.period_end) << "]";
	return o;
}
std::string ns_extract_scanner_name_from_filename(const std::string & filename){
	string device_name;
	//find first equals sign
	unsigned int i;
	for (i = 0; i < filename.size(); i++)
		if (filename[i]=='=') break;
	i++;	
	//find second equals sign
	for (; i < filename.size(); i++)
		if (filename[i]=='=') break;
	i++;
	//grab all the text until the next equals sign or the end of the filename.
	for (; i < filename.size(); i++){
		if (filename[i]=='=') break;
		device_name += filename[i];
	}
	return device_name;
}
void ns_worm_learner::output_area_info(const std::string & filename){
	string device_name = ns_extract_scanner_name_from_filename(current_clipboard_filename);

	if (filename != ""){
		ofstream out(filename.c_str());
		if (out.fail())
			throw ns_ex("Could not open ") << filename;
		unsigned long image_resolution(current_image.properties().resolution);


		out << ns_format_time_string_for_human(ns_current_time()) << ": Sample boundary specifications for the device \"" << device_name << "\" were generated from the source image " << current_clipboard_filename;
		if (image_resolution != 0) {
			out << "@ " << image_resolution << " dpi\n\n";	
		}
		else 
		if (image_resolution == 0){
			out << "\n\n";
			cerr << "Could not deduce source image resolution; assuming it is 300 dpi\n\n";
			out << "Could not deduce source image resolution; assuming it is 300 dpi\n\n";
			image_resolution = 300;
		}


		out << "====Specification in Inches====\n";
		area_handler.output_boxes(out,device_name,image_resolution,"in");
		//out << "\n====Specification in Source Image Pixels====\n";
		//area_handler.output_boxes(out,device_name,1,"pix");
	}
}

void ns_worm_learner::clear_areas(){
	area_handler.clear_boxes();
}

void ns_worm_learner::touch_worm_window_pixel(const ns_button_press & press){
	if (press.screen_position.x < 4|| press.screen_position.y < 4 ||
		press.screen_position.x+4 >= worm_window.gl_image_size.x*worm_window.display_rescale_factor|| press.screen_position.y+4 >= worm_window.gl_image_size.y*worm_window.display_rescale_factor)
		return;
	float z = ((float)worm_window.pre_gl_downsample)/worm_window.image_zoom;
	ns_button_press p(press);
	p.image_position = p.screen_position*z;
	touch_worm_window_pixel_internal(p);
}

bool ns_worm_learner::register_worm_window_key_press(int key, const bool shift_key_held,const bool control_key_held,const bool alt_key_held){
	if (key == ']'){
		worm_window.display_rescale_factor+=.1;
		return true;
	}
	else if (key == '['){
		worm_window.display_rescale_factor-=.1;
		if (worm_window.display_rescale_factor <= 0)
			worm_window.display_rescale_factor = .1;
		return true;
	}
																
	else if (key == FL_Left || key== 'a'){
		ns_worm_learner::navigate_solo_worm_annotation(ns_death_time_solo_posture_annotater::ns_back,true);
		return true;
	}
	else if (key == FL_Right || key == 'd'){
		ns_worm_learner::navigate_solo_worm_annotation(ns_death_time_solo_posture_annotater::ns_forward,true);
		return true;
	}
	else if (key == 's' && control_key_held){
		death_time_solo_annotater.register_click(ns_vector_2i(0,0),ns_death_time_solo_posture_annotater::ns_output_images);
		return true;
	}
	else if (shift_key_held && key == '=' || key == '+'){
		death_time_solo_annotater.register_click(ns_vector_2i(0,0),ns_death_time_solo_posture_annotater::ns_increase_contrast);
		return true;
	}
	else if (key == '-'){
		death_time_solo_annotater.register_click(ns_vector_2i(0,0),ns_death_time_solo_posture_annotater::ns_decrease_contrast);
		return true;
	}
	else if (key == 'i') {
		death_time_solo_annotater.telemetry.show(!death_time_solo_annotater.telemetry.show());
		return true;
	}
	else if (key == 'v') {
		solo_annotation_visualization_type = death_time_solo_annotater.step_visualization_type();
		return true;
	}
	return false;
}


void ns_worm_learner::touch_main_window_pixel(const ns_button_press & press){
	if (press.screen_position.x < 4|| press.screen_position.y < 4 ||
		press.screen_position.x+4 >= main_window.gl_image_size.x*main_window.display_rescale_factor|| press.screen_position.y+4 >= main_window.gl_image_size.y*main_window.display_rescale_factor)
		return;
	float z = ((float)main_window.pre_gl_downsample)/(float)main_window.image_zoom/main_window.display_rescale_factor;
	ns_button_press p(press);
	p.image_position = p.screen_position*z;
	p.image_distance_from_click_location= p.screen_distance_from_click_location*z;
	//cerr << "Touching Raw " << p.screen_position << "\n";
	//cerr << "Zoom is " << z << "\n";dr
	//cerr << "Translating to " << p.image_position << "\n";
	touch_main_window_pixel_internal(p);
}

bool ns_worm_learner::register_main_window_key_press(int key, const bool shift_key_held,const bool control_key_held,const bool alt_key_held){

	if (shift_key_held && key == '=' || key == '+'){
		dynamic_range_rescale+=.1;
		main_window.redraw_screen();
		return true;
	}
	else if (key == '-'){
		dynamic_range_rescale-=.1;
		if(dynamic_range_rescale<=0)
			dynamic_range_rescale=.1;
		main_window.redraw_screen();
		return true;
	}
	else if (key == ']'){
		main_window.display_rescale_factor+=.1;
		main_window.redraw_screen();
		return true;
	}
	else if (key == '['){
		main_window.display_rescale_factor-=.1;
		if (main_window.display_rescale_factor <= 0)
			main_window.display_rescale_factor = .1;
		main_window.redraw_screen();
		return true;
	}
	else if (key == 'i') {
		storyboard_annotater.overlay_worm_ids();
		main_window.redraw_screen();
		return true;
	}

	switch(behavior_mode){
		case ns_worm_learner::ns_draw_boxes:{
			if (key == 's' && control_key_held){
				save_current_area_selections();
				return true;
			}
			break;
		}
		case ns_worm_learner::ns_annotate_death_times_in_time_aligned_posture:
		case ns_worm_learner::ns_annotate_death_times_in_death_aligned_posture:
		case ns_worm_learner::ns_annotate_storyboard_region:
		case ns_worm_learner::ns_annotate_storyboard_sample:
		case ns_worm_learner::ns_annotate_storyboard_experiment:
																				  {
			if (key == FL_Left || key== 'a'){
				navigate_death_time_annotation(ns_image_series_annotater::ns_back,true);
				return true;
			}
			else if (key == FL_Right || key == 'd'){
				navigate_death_time_annotation(ns_image_series_annotater::ns_forward,true);
				return true;
			}
			else if (key == 'S' || key == '.'){
			  //ns_acquire_lock_for_scope lock(persistant_sql_lock, __FILE__, __LINE__);
				save_death_time_annotations(get_sql_connection());
				//	lock.release();
			}
		}
	}
	return false;
}

bool ns_worm_learner::prompt_to_save_death_time_annotations(){
	if (current_annotater == 0)
		throw ns_ex("No Annotater Specified!");
	if (current_annotater->data_saved()){
		std::cout << "No Save Required.\n";
		return true;
	}
	ns_choice_dialog dialog;
	dialog.title = "There are unsaved annotations in your storyboard.  What should be done?";
	dialog.option_1 = "Save";
	dialog.option_2 = "Discard Unsaved";
	dialog.option_3 = "Cancel";
	ns_run_in_main_thread<ns_choice_dialog> b(&dialog);
	if (dialog.result == 1){

	  //ns_acquire_lock_for_scope lock(persistant_sql_lock, __FILE__, __LINE__);
		save_death_time_annotations(get_sql_connection());
		//	lock.release();
		return true;
	}
	if (dialog.result == 2)
		return true;
	return false;
}
void ns_worm_learner::save_death_time_annotations(ns_sql & sql){
	
	ns_death_time_annotation_set set;

	/*if (current_behavior_mode() == ns_worm_learner::ns_annotate_storyboard_experiment ||
		current_behavior_mode() == ns_worm_learner::ns_annotate_storyboard_region ||
		current_behavior_mode() == ns_worm_learner::ns_annotate_storyboard_sample){
			std::vector<ns_death_time_annotation> orphans;
			death_time_solo_annotater.add_annotations_to_set(set,orphans);
			//discard orphans
	}*/
	current_annotater->save_annotations(set);
	std::set<ns_64_bit> region_ids;
	
	if (behavior_mode == ns_worm_learner::ns_annotate_storyboard_experiment) {
		sql << "UPDATE sample_region_image_info as r, capture_samples as s SET "
			<< "r.latest_by_hand_annotation_timestamp = UNIX_TIMESTAMP(NOW()) WHERE r.sample_id = s.id AND s.experiment_id = " << data_selector.current_experiment_id();
		sql.send_query();
	}
	else {
		sql << "UPDATE sample_region_image_info SET "
			<< "latest_by_hand_annotation_timestamp = UNIX_TIMESTAMP(NOW()) WHERE id = " << data_selector.current_region().region_id;
		sql.send_query();
	}

};

void ns_worm_learner::touch_main_window_pixel_internal(const ns_button_press & press){
	current_image_lock.wait_to_acquire(__FILE__,__LINE__);


	switch(behavior_mode){
		case ns_worm_learner::ns_draw_boxes:{
			const unsigned int h(current_image.properties().height);
			const unsigned int w(current_image.properties().width);
			const char c(current_image.properties().components);
			if (press.image_position.x < (unsigned int)c || press.image_position.x + (unsigned int)c >= w) return;
			if (press.image_position.y < (unsigned int)c || press.image_position.y +(unsigned int)c>= h) return;
			if (press.control_key_held)
						area_handler.click(ns_area_handler::ns_move_all_boxes,
						press.image_distance_from_click_location,
						press.screen_distance_from_click_location,
						main_window.gl_buffer ,current_image,main_window.pre_gl_downsample,main_window.image_zoom);
			else{
				//Handle out of bounds locations
				if (press.image_position.x < (unsigned int)c || press.image_position.x + (unsigned int)c >= w) return;
				if (press.image_position.y < (unsigned int)c || press.image_position.y +(unsigned int)c>= h) return;
			
				switch(press.click_type){
					case ns_button_press::ns_up:
							area_handler.click(ns_area_handler::ns_deselect_handle,press.image_position,press.screen_position,main_window.gl_buffer ,current_image,main_window.pre_gl_downsample,main_window.image_zoom); break;
						case ns_button_press::ns_down:
							area_handler.click(ns_area_handler::ns_select_handle,press.image_position,press.screen_position,main_window.gl_buffer ,current_image,main_window.pre_gl_downsample,main_window.image_zoom); break;
						case ns_button_press::ns_drag:
							area_handler.click(ns_area_handler::ns_move_handle,press.image_position,press.screen_position,main_window.gl_buffer ,current_image,main_window.pre_gl_downsample,main_window.image_zoom); break;
						break;
				}
			}
			main_window.redraw_screen();
			break;
		}
		case ns_worm_learner::ns_annotate_storyboard_region:
		case ns_worm_learner::ns_annotate_storyboard_sample:
		case ns_worm_learner::ns_annotate_storyboard_experiment:{
			if (press.click_type == ns_button_press::ns_up){
				if (press.shift_key_held)
					current_annotater->register_click(ns_vector_2i(press.image_position.x,press.image_position.y),ns_image_series_annotater::ns_annotate_extra_worm);
				else if (press.control_key_held)
					current_annotater->register_click(ns_vector_2i(press.image_position.x,press.image_position.y),ns_image_series_annotater::ns_censor_all);
				else if (press.right_button || press.control_key_held)
					current_annotater->register_click(ns_vector_2i(press.image_position.x,press.image_position.y),ns_image_series_annotater::ns_censor);
				else
					current_annotater->register_click(ns_vector_2i(press.image_position.x,press.image_position.y),ns_image_series_annotater::ns_load_worm_details);
			
			}
			break;
		}					
																
			
		case ns_worm_learner::ns_annotate_death_times_in_time_aligned_posture:
		case ns_worm_learner::ns_annotate_death_times_in_death_aligned_posture:{
			if (press.click_type == ns_button_press::ns_up){
				if (press.right_button || press.control_key_held)
					current_annotater->register_click(ns_vector_2i(press.image_position.x,press.image_position.y),ns_image_series_annotater::ns_censor);
				else if (press.shift_key_held)
					current_annotater->register_click(ns_vector_2i(press.image_position.x,press.image_position.y),ns_image_series_annotater::ns_annotate_extra_worm);
				else
					current_annotater->register_click(ns_vector_2i(press.image_position.x,press.image_position.y),ns_image_series_annotater::ns_cycle_state);
			}
			break;
		}								
	}
	current_image_lock.release();
}


void ns_worm_learner::touch_worm_window_pixel_internal(const ns_button_press & press){
	current_image_lock.wait_to_acquire(__FILE__,__LINE__);


	if (press.click_type == ns_button_press::ns_up){
		if (press.control_key_held)
			death_time_solo_annotater.register_click(ns_vector_2i(press.image_position.x,press.image_position.y),ns_image_series_annotater::ns_cycle_flags);
		else if (press.shift_key_held)
			death_time_solo_annotater.register_click(ns_vector_2i(press.image_position.x,press.image_position.y),ns_image_series_annotater::ns_annotate_extra_worm);
		else if (press.right_button)
			death_time_solo_annotater.register_click(ns_vector_2i(press.image_position.x, press.image_position.y), ns_image_series_annotater::ns_cycle_state_alt_key_held);
		else
			death_time_solo_annotater.register_click(ns_vector_2i(press.image_position.x,press.image_position.y),ns_image_series_annotater::ns_cycle_state);
	}
	current_image_lock.release();
}

inline ns_8_bit ns_rescale(const ns_8_bit & val,const float & f){
	if(f==1)return val;
	const float g(val*f);
	if (g > 255) return 255;
	return (ns_8_bit)g;
}

void ns_worm_learner::draw_image(const double x, const double y, ns_image_standard & image){
	float	dynamic_stretch_factor = main_window.dynamic_range_rescale_factor;
	ns_acquire_lock_for_scope lock(main_window.display_lock,__FILE__,__LINE__);
	
	ns_image_properties new_image_size = image.properties();


	//cout << "im" << new_image_size.width << "," << new_image_size.height << " : ";
	new_image_size.components = 3;
	//if the image is larger than the display size
	//it should be downsampled before being sent to the video card.
	int resize_x = (int)floor((1.0*new_image_size.width)/maximum_window_size.x);
	int resize_y = (int)floor((1.0*new_image_size.height)/maximum_window_size.y);
	
	if (resize_x >= resize_y)
		main_window.pre_gl_downsample = resize_x;
	else main_window.pre_gl_downsample = resize_y;
	if (main_window.pre_gl_downsample < 1)
		main_window.pre_gl_downsample = 1;
	
	new_image_size.width/=main_window.pre_gl_downsample;
	new_image_size.height/=main_window.pre_gl_downsample;


	double gl_resize(1);
	if (new_image_size.height > maximum_window_size.y)
		gl_resize = ((double)maximum_window_size.y)/new_image_size.height;
	if (new_image_size.width > maximum_window_size.x){
		const double r(((double)maximum_window_size.x)/new_image_size.width);
		if (r < gl_resize)
			gl_resize = r;
	}
	main_window.gl_image_size.y= (unsigned int)floor(new_image_size.height*gl_resize);
	main_window.gl_image_size.x = (unsigned int)floor(new_image_size.width*gl_resize);
	//cout << "gl" << 	main_window.gl_image_size.x << "," << 	main_window.gl_image_size.y << "\n ";

	if (main_window.gl_buffer_properties.width != new_image_size.width || main_window.gl_buffer_properties.height != new_image_size.height || main_window.gl_buffer_properties.components != new_image_size.components){
		
		if (main_window.gl_buffer != 0){
			delete[] main_window.gl_buffer ;
			main_window.gl_buffer = 0;
		}
		
		main_window.gl_buffer = new ns_8_bit[new_image_size.components*new_image_size.height*new_image_size.width];
	
		main_window.gl_buffer_properties = new_image_size;

		area_handler.register_size_change(new_image_size);
	}
	if (image.properties().components == 3){
		for (unsigned int _x = 0; _x < main_window.gl_buffer_properties.width; _x++)
		for (int _y = 0; _y < (int)main_window.gl_buffer_properties.height; _y++){
				for (unsigned int c = 0; c < 3; c++){
					main_window.gl_buffer [new_image_size.width*3*_y + 3*_x + c] = ns_rescale(image[image.properties().height-1 - _y*main_window.pre_gl_downsample ][3*_x*main_window.pre_gl_downsample+c],dynamic_stretch_factor);
				}
		}
	}

	//b&w images
	else if (image.properties().components == 1){
		for (int _y = 0; _y < (int)new_image_size.height; _y++){
			for (unsigned int _x = 0; _x < new_image_size.width; _x++){
				main_window.gl_buffer [new_image_size.width*3*_y + 3*_x + 0] =
				main_window.gl_buffer [new_image_size.width*3*_y + 3*_x + 1] =
				main_window.gl_buffer [new_image_size.width*3*_y + 3*_x + 2] = ns_rescale(image[image.properties().height-1 - _y*main_window.pre_gl_downsample ][_x*main_window.pre_gl_downsample],dynamic_stretch_factor);
			}
		}		
	}
	area_handler.draw_boxes(main_window.gl_buffer ,image.properties(),main_window.pre_gl_downsample,main_window.image_zoom);

	//now we handle the gl scaling
	if (main_window.display_rescale_factor <= 1 && 
		(main_window.gl_image_size.x*main_window.display_rescale_factor > this->maximum_window_size.x ||
		main_window.gl_image_size.y*main_window.display_rescale_factor > this->maximum_window_size.y)){
		cerr << "Cannot resize, as the current window has hit the maximum size specified in the ns_worm_browser.ini file\n";
		main_window.display_rescale_factor = floor(min(maximum_window_size.x/(double)main_window.gl_image_size.x,
												maximum_window_size.y/(double)main_window.gl_image_size.y)*10)/10;
	
	}

	lock.release();
	main_window.redraw_screen();
}


void ns_worm_learner::draw_worm_window_image(ns_image_standard & image){

	ns_acquire_lock_for_scope lock(worm_window.display_lock,__FILE__,__LINE__);


	//if the image is an integer factor larger than the display size
	//it should be downsampled before being sent to the video card.
	worm_window.pre_gl_downsample = image.properties().width / (maximum_window_size.x- death_time_solo_annotater.telemetry.image_size().x);
	if (worm_window.pre_gl_downsample < 1)
		worm_window.pre_gl_downsample = 1;

	ns_image_properties new_image_size = image.properties();
	new_image_size.components = 3;

	new_image_size.width/=worm_window.pre_gl_downsample;
	new_image_size.height/=worm_window.pre_gl_downsample;

	ns_vector_2i buffer_size(new_image_size.width + death_time_solo_annotater.telemetry.image_size().x,
				 (new_image_size.height > death_time_solo_annotater.telemetry.image_size().y) ?
				 new_image_size.height : death_time_solo_annotater.telemetry.image_size().y);

	//we resize the image by an integer factor
	//but it still needs to be resized by the video card
	//by a non-integer factor in order to fit into the desired window size.
	//we calculate this and send it to the video ard
	double gl_resize(1);
	if (buffer_size.y > maximum_window_size.y)
		gl_resize = ((double)maximum_window_size.y)/ buffer_size.y;
	if (buffer_size.x > maximum_window_size.x){
		const double r(((double)maximum_window_size.x)/ buffer_size.x);
		if (r < gl_resize)
			gl_resize = r;
	}
	//don't do this, because it always ends up looking messy.
	gl_resize = 1;

	worm_window.telemetry_size = ns_vector_2i(
		(unsigned int)floor(death_time_solo_annotater.telemetry.image_size().x*gl_resize),
		(unsigned int)floor(death_time_solo_annotater.telemetry.image_size().y*gl_resize));
	
	worm_window.worm_image_size = ns_vector_2i(
		(unsigned int)floor(new_image_size.width*gl_resize),
		(unsigned int)floor(new_image_size.height*gl_resize));

	worm_window.gl_image_size = buffer_size;

	//cerr << "Draw requests a worm window size of " << worm_window.image_size << "\n";

	if (worm_window.gl_buffer_properties.width != worm_window.gl_image_size.x || worm_window.gl_buffer_properties.height != worm_window.gl_image_size.y
		|| worm_window.gl_buffer_properties.components != 3){
		
		if (worm_window.gl_buffer != 0){
			delete[] worm_window.gl_buffer ;
			worm_window.gl_buffer = 0;
		}
		
		worm_window.gl_buffer = new ns_8_bit[3*buffer_size.x*buffer_size.y];
		
		worm_window.gl_buffer_properties.components = 3;
		worm_window.gl_buffer_properties.width = buffer_size.x;
		worm_window.gl_buffer_properties.height = buffer_size.y;
	}
	const unsigned long worm_image_height = (new_image_size.height - death_time_solo_annotater.bottom_margin_position().y-1)/worm_window.pre_gl_downsample;
	if (image.properties().components == 3) {
	  //copy over the top border area of the image (which contains only text and metata) without rescaling


		for (int _y = 0; _y< worm_image_height; _y++) {
			for (unsigned int _x = 0; _x < new_image_size.width; _x++) {
				worm_window.gl_buffer[3 * (worm_window.gl_buffer_properties.width*_y + _x)] =
					image[(image.properties().height - 1 - _y*worm_window.pre_gl_downsample)][3 * _x*worm_window.pre_gl_downsample];
				worm_window.gl_buffer[3 * (worm_window.gl_buffer_properties.width*_y + _x) + 1] =
					image[(image.properties().height - 1 - _y*worm_window.pre_gl_downsample)][3 * _x*worm_window.pre_gl_downsample + 1];
				worm_window.gl_buffer[3 * (worm_window.gl_buffer_properties.width*_y + _x) + 2] =
					image[(image.properties().height - 1 - _y*worm_window.pre_gl_downsample)][3 * _x*worm_window.pre_gl_downsample + 2];
			}
			//	for (unsigned int _x = new_image_size.width; _x < buffer_size.x; _x++)
			//	  for (int c = 0; c < 3; c++)
			//	    worm_window.gl_buffer[3 * (worm_window.gl_buffer_properties.width*_y + _x)+c] = 0; 
		//	worm_window.gl_buffer[3 * (worm_window.gl_buffer_properties.width*_y)+1] = 255;
		}
		//copy over the bottom area of the imate (which contains the image of the worm)
	
	  //cerr << image.properties().height - 1 - _y*worm_window.pre_gl_downsample << "," << 3 * new_image_size.width*worm_window.pre_gl_downsample << " ";
	for (int _y = worm_image_height; _y < new_image_size.height; _y++) {
			for (unsigned int _x = 0; _x < new_image_size.width; _x++) {
		     
			    worm_window.gl_buffer[3 * (worm_window.gl_buffer_properties.width*_y + _x)] = 
					ns_rescale(image[image.properties().height - 1 - _y*worm_window.pre_gl_downsample][3 * _x*worm_window.pre_gl_downsample], worm_window.dynamic_range_rescale_factor);
				worm_window.gl_buffer[3 * (worm_window.gl_buffer_properties.width*_y + _x) + 1] =
					ns_rescale(image[(image.properties().height - 1 - _y*worm_window.pre_gl_downsample)][3 * _x*worm_window.pre_gl_downsample + 1], worm_window.dynamic_range_rescale_factor);
				worm_window.gl_buffer[3 * (worm_window.gl_buffer_properties.width*_y + _x) + 2] =
					ns_rescale(image[(image.properties().height - 1 - _y*worm_window.pre_gl_downsample)][3 * _x*worm_window.pre_gl_downsample + 2], worm_window.dynamic_range_rescale_factor);
			}
		//	worm_window.gl_buffer[3 * (worm_window.gl_buffer_properties.width*_y)] = 255;
		}
	
	//	cout << "("<< image.properties().height << "," << image.properties().width << ")\n";
	}
	//b&w images
	else if (image.properties().components == 1){
		throw ns_ex("Err!");
	}
	if (death_time_solo_annotater.telemetry.show()) {
		try {
			if (!death_time_solo_annotater.movement_quantification_data_loaded()) {

				if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Loading movement quantification data"));
			  death_time_solo_annotater.load_movement_analysis(get_sql_connection());
			}
			death_time_solo_annotater.draw_telemetry(ns_vector_2i(new_image_size.width, 0),
				death_time_solo_annotater.telemetry.image_size(),
				ns_vector_2i(worm_window.gl_buffer_properties.width,
					worm_window.gl_buffer_properties.height),
				worm_window.gl_buffer);

			//clear out bottom margin
			for (int _y = death_time_solo_annotater.telemetry.image_size().y; _y < worm_window.gl_buffer_properties.height; _y++)
				for (unsigned int _x = 3 * new_image_size.width; _x < 3 * worm_window.gl_buffer_properties.width; _x++)
					worm_window.gl_buffer[3 * (worm_window.gl_buffer_properties.height - _y-1)*worm_window.gl_buffer_properties.width + _x] = 0;
		
	//		death_time_solo_annotater.draw_registration_debug(ns_vector_2i(new_prop.width, death_time_solo_annotater.telemetry.image_size().y),
		//		ns_vector_2i(worm_window.gl_buffer_properties.width,
			//		worm_window.gl_buffer_properties.height),
				//worm_window.gl_buffer);
		
		}
		catch (ns_ex & ex) {
			for (int _y = 0; _y < worm_window.gl_buffer_properties.height; _y++) 
				for (unsigned int _x = 3*new_image_size.width; _x < 3*worm_window.gl_buffer_properties.width; _x++) 
					worm_window.gl_buffer[3 * worm_window.gl_buffer_properties.width*_y + _x] = 0;
			cout << ex.text();
		}
		
	}
	
	
	//now we handle the gl scaling
	if (worm_window.display_rescale_factor <= 1 && (worm_window.gl_image_size.x*worm_window.display_rescale_factor > this->maximum_window_size.x ||
		worm_window.gl_image_size.y*worm_window.display_rescale_factor > this->maximum_window_size.y)){
		cerr << "Cannot resize, as the current window has hit the maximum size specified in the ns_worm_browser.ini file\n";
		worm_window.display_rescale_factor = floor(min(maximum_window_size.x/(double)worm_window.gl_image_size.x,
											           maximum_window_size.y/(double)worm_window.gl_image_size.y)*10)/10;
		//cerr << worm_window.display_rescale_factor << " ";
	}


	lock.release();
	worm_window.redraw_screen();
}
void ns_rotate_image_color(const double f,const ns_image_standard & s, ns_image_standard & d){
	d.init(s.properties());
	long h(d.properties().height),
		 w(d.properties().width);
	const double o(cos(f)),
		        i(sin(f));
	//cerr << o << "," << i << "\n";
	ns_vector_2i e(w/2,h/2);
	for (long y = 0; y < h; y++)
		for (long  x = 0; x < w; x++){
			ns_vector_2d p(o*(x-e.x)-i*(y-e.y)+e.x,
						   i*(x-e.x)+o*(y-e.y)+e.y);
			d[y][3*x+0] = s.slow_safe_access(p.y,3*floor(p.x+.5)+0);
			d[y][3*x+1] = s.slow_safe_access(p.y,3*floor(p.x+.5)+1);
			d[y][3*x+2] = s.slow_safe_access(p.y,3*floor(p.x+.5)+2);
		}

}

struct ns_thermometer_measurement_data{
	double temperature_region_value,
		   calibration_region_value;
	double value(){
		if (calibration_region_value == 0)
			throw ns_ex("0 calibration value!");
		return temperature_region_value/calibration_region_value;
	}
	double estimated_temperature;

	static void out_header(const string & name,ostream & o){
		o << name << " TValue,"<< name << " CValue,"<< name << " T/C Ratio,"<< name << " Estimated Temperature";
	}
	void out_data(ostream & o){
		o << temperature_region_value << "," << calibration_region_value << "," << value() << "," << estimated_temperature;
	}
	void zero(){
		temperature_region_value = 0;
		calibration_region_value = 0;
	}
};

class ns_thermometer_image_processor{
public:
	void specificy_masked_image(const ns_image_standard & im){
		temperature_region_position = 
			normalization_region_position = 
				ns_vector_2i(im.properties().width,im.properties().height);
		ns_vector_2i temperature_t(0,0),
					 normalization_t(0,0);

		for (unsigned int y = 0; y < im.properties().height; y++){
			for (unsigned int x = 0; x < im.properties().width; x++){
				if (im[y][x] == 0)
					continue;
				ns_vector_2i * pos(&normalization_region_position),
							 *  t(&normalization_t);
				if (im[y][x] > 200){
					pos = &temperature_region_position;
					t = &temperature_t;
				}
				if (x < pos->x) pos->x = x;
				if (y < pos->y) pos->y = y;

				if (x > t->x) t->x = x;
				if (y > t->y) t->y = y;
			}
		}
		if (temperature_t.x < temperature_region_position.x || temperature_t.y < temperature_region_position.y)
			throw ns_ex("No temperature region specified");
		if (normalization_t.x < normalization_region_position.x || normalization_t.y < normalization_region_position.y)
			throw ns_ex("No calibration region specified");
		temperature_region_size = temperature_t - temperature_region_position +ns_vector_2i(1,1);
		normalization_region_size = normalization_t - normalization_region_position +ns_vector_2i(1,1);
	}
	void specificy_reference_temperature_image(const double absolute_temperature,ns_image_standard & im){		
		calibration_point = calculate_thermometer_measurement_data(im);
		calibration_point_temperature = absolute_temperature;
	}
	void specifiy_known_differential_temperature(const double temperature_differential, ns_image_standard & low, ns_image_standard & high){
		differential_point_low = calculate_thermometer_measurement_data(low);
		differential_point_high = calculate_thermometer_measurement_data(high);
		differential_temperature = temperature_differential;
	}
	ns_thermometer_measurement_data estimate_temperature(ns_image_standard & im){
		ns_thermometer_measurement_data d(calculate_thermometer_measurement_data(im));
		d.estimated_temperature = (d.value()-calibration_point.value())
								  /(differential_point_high.value()-differential_point_low.value())
								  *differential_temperature
								  +calibration_point_temperature;
		return d;
	}
	static void out_calibration_data_header(ostream & o){
		ns_thermometer_measurement_data::out_header("Absolute Calibration",o);
		o << ",";
		ns_thermometer_measurement_data::out_header("Differential Low",o);
		o << ",";
		ns_thermometer_measurement_data::out_header("Differential High",o);
		o << ",";
		o << "Absolute Calibration Temperature (C),Differential Temperature(C)";
	}
	void out_calibration_data(ostream & o){
		calibration_point.out_data(o);
		o << ",";
		differential_point_low.out_data(o);
		o << ",";
		differential_point_high.out_data(o);
		o << ",";
		o << calibration_point_temperature << "," << differential_temperature;
	}
private:
	ns_thermometer_measurement_data calibration_point;
	ns_thermometer_measurement_data differential_point_low,
									differential_point_high;
	double calibration_point_temperature,
		   differential_temperature;
	
	ns_vector_2i temperature_region_position,
				 temperature_region_size;
	ns_vector_2i normalization_region_position,
				 normalization_region_size;

	ns_thermometer_measurement_data calculate_thermometer_measurement_data(const ns_image_standard & im){
		ns_thermometer_measurement_data d;
		d.temperature_region_value = average_region_intensity(temperature_region_position,temperature_region_size,im);
		d.calibration_region_value = average_region_intensity(normalization_region_position,normalization_region_size,im);
		return d;
	}
	static double average_region_intensity(const ns_vector_2i & pos,const ns_vector_2i & size, const ns_image_standard & im){
		if (size.y == 0 || size.x == 0)
			return 0;
		long long sum(0);
		for (unsigned int y = 0; y < size.y; y++){
			for (unsigned int x = 0; x < size.x; x++){
				sum+=im[pos.y+y][pos.x+x];
			}
		}
		return sum/(double)(size.x*size.y);
	}
};
struct ns_thermometer_measurement{
	ns_image_server_captured_image image;
	unsigned long capture_time;
	ns_thermometer_measurement_data temperature;
	static void out_header(ostream & o,const string &name){
		o << "Capture Time, " ;
		ns_thermometer_measurement_data::out_header(name,o);
		o << ", Problem, Problem Text";
	}
	void out_data(ostream & o){
		o << capture_time << ",";
		temperature.out_data(o);
		o << ((file_problem.text().size()>0)?"1":"0") << "," << file_problem.text();
	}
	ns_ex file_problem;
	void set_problem(ns_ex & ex){
		file_problem = ex;
		temperature.zero();
	}

};

class ns_thermometer_experiment{
public:
	string device_name;

	string mask_image_filename,
		   calibrated_temperature_image_filename,
		   low_differential_temperature_image_filename,
		   high_differential_temperature_image_filename;

	static void output_header(ostream & o){
		o << "Device Name, ";
		ns_thermometer_image_processor::out_calibration_data_header(o);
		o << ",";
		ns_thermometer_measurement::out_header(o,"Measured ");
	}
	void output_data(ostream & o){
		for (unsigned int i = 0; i < measurements.size(); i++){
			o << device_name << ",";
			processor.out_calibration_data(o);
			o << ",";
			measurements[i].out_data(o);
			o << "\n";
		}

	}
	ns_thermometer_measurement & find_closest_measurement(const unsigned long t){
		if (measurements.size() == 0)
			throw ns_ex("No measurements loaded for device ") << device_name;
		unsigned long closest_id(0),
			closest_distance(abs((long)(measurements[0].capture_time-t)));
		for (unsigned int i = 1; i < measurements.size(); i++){
			unsigned long d(abs((long)(measurements[i].capture_time-t)));
			if (d < closest_distance){
				closest_id = i;
				closest_distance = d;
			}
		}
		return measurements[closest_id];
	}
	ns_thermometer_measurement_data estimate_temperature(ns_thermometer_measurement & measurement,ns_sql & sql){
		ns_image_standard im;
		try{
			image_server.image_storage.request_from_storage(measurement.image,&sql).input_stream().pump(im,1024);
			measurement.temperature = processor.estimate_temperature(im);
		}
		catch(ns_ex & ex){
			cerr << ex.text() << "\n";
			measurement.set_problem(ex);
		}
		return measurement.temperature;
	
	}
	void estimate_temperatures(ns_sql & sql){
		cerr << "Estimating Temperature...";
		for (unsigned int i = 0; i < measurements.size(); i++){
			cerr << (100*i)/measurements.size() << "%...";
			estimate_temperature(measurements[i],sql);
		}
		cerr << "\n";
	}

	ns_thermometer_image_processor processor;
	vector<ns_thermometer_measurement> measurements;
};
/*
void ns_run_first_thermometer_experiment(){

	vector<ns_thermometer_experiment> scanners(10);
	scanners[0].device_name = "ben";
	scanners[1].device_name = "gold";
	scanners[2].device_name = "jerry";
	scanners[3].device_name = "kiwi";
	scanners[4].device_name = "maki";
	scanners[5].device_name = "opal";
	scanners[6].device_name = "ruby";
	scanners[7].device_name = "smile";
	scanners[8].device_name = "snap";
	scanners[9].device_name = "stone";

	unsigned long experiment_id(201);
	double calibration_temperature = 23.05;

	double low_differential_temperature = 23.5;
	string low_differential_time_string = "09:14:00 08/10/2010";
	double high_differential_temperature = 24.5;
	string high_differential_time_string = "12:10:00 08/10/2010";
	
	unsigned long high_differential_time = ns_time_from_format_string(high_differential_time_string);
	unsigned long low_differential_time = ns_time_from_format_string(low_differential_time_string);

	ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__,__LINE__));
	ns_sql_result result;

	string output_filename("c:\\temperature_results.csv");
	ofstream output(output_filename.c_str());
	if (output.fail())
		throw ns_ex("Could not open") << output_filename;
	ns_thermometer_experiment::output_header(output);
	output << "\n";
	
	//setup all the data
	for (unsigned long i = 0; i < scanners.size(); i++){
		scanners[i].calibrated_temperature_image_filename = "Y:\\image_server_storage\\partition_000\\misc\\thermometer_calibration_data\\";
		scanners[i].calibrated_temperature_image_filename += scanners[i].device_name + "=preview_8_bit.tif";
		scanners[i].mask_image_filename = "Y:\\image_server_storage\\partition_000\\misc\\thermometer_calibration_data\\masks\\";
		scanners[i].mask_image_filename += scanners[i].device_name + "=preview_8_bit copy.tif";

		ns_image_standard im;
		ns_load_image(scanners[i].mask_image_filename,im);
		scanners[i].processor.specificy_masked_image(im);

		ns_load_image(scanners[i].calibrated_temperature_image_filename,im);
		scanners[i].processor.specificy_reference_temperature_image(calibration_temperature,im);


		sql() << "SELECT c.capture_time, c.id,c.image_id FROM captured_images as c, capture_samples as s, capture_schedule as sh "
			"WHERE c.problem = 0 AND c.sample_id = s.id AND s.name = '" << scanners[i].device_name << "_e' AND s.device_name = '" << scanners[i].device_name << "' AND s.experiment_id = " << experiment_id
				 << " AND sh.captured_image_id = c.id AND sh.transferred_to_long_term_storage = 3";
		sql().get_rows(result);
		scanners[i].measurements.resize(result.size());
		cerr << scanners[i].device_name << " has " << result.size() << " measurements\n";
		for (unsigned int j = 0; j < result.size(); j++){
			scanners[i].measurements[j].capture_time = atol(result[j][0].c_str());
			scanners[i].measurements[j].image.captured_images_id = atol(result[j][1].c_str());
			scanners[i].measurements[j].image.capture_images_image_id = atol(result[j][2].c_str());
		}
		ns_thermometer_measurement & high_measurement(scanners[i].find_closest_measurement(high_differential_time));
		ns_thermometer_measurement & low_measurement(scanners[i].find_closest_measurement(low_differential_time));
		
		cerr << scanners[i].device_name << " closest time to " << ns_format_time_string_for_human(high_differential_time) << " is " << ns_format_time_string_for_human(high_measurement.capture_time) <<"\n";
			
		cerr << scanners[i].device_name << " closest time to " << ns_format_time_string_for_human(low_differential_time) << " is " << ns_format_time_string_for_human(low_measurement.capture_time) <<"\n";

		ns_image_standard high,low;
		image_server.image_storage.request_from_storage(high_measurement.image,&sql()).input_stream().pump(high,1024);
		image_server.image_storage.request_from_storage(low_measurement.image,&sql()).input_stream().pump(low,1024);
		scanners[i].processor.specifiy_known_differential_temperature(high_differential_temperature-low_differential_temperature,low,high);									
		scanners[i].estimate_temperatures(sql());
		scanners[i].output_data(output);
	}
	output.close();
};
*/
void ns_worm_learner::draw_animation(const double &t){
	unsigned long offset(10);
//	cerr << t << "\n";
	main_window.display_lock.wait_to_acquire(__FILE__, __LINE__);
	//	return;
	try{
		if (animation.properties().width +offset <= main_window.gl_buffer_properties.width &&
			animation.properties().height +offset <= main_window.gl_buffer_properties.height){
				ns_rotate_image_color(4*t,animation,animation_temp);
				ns_vector_2i pos(main_window.gl_buffer_properties.width-animation.properties().width-1,
								main_window.gl_buffer_properties.height-animation.properties().height-1);

			//	ns_vector_2i pos(main_window.gl_buffer_properties.width-animation.properties().width-1 - offset*(1.0+sin(4*t)),
			//					main_window.gl_buffer_properties.height-animation.properties().height - 2*offset - 1 + offset*(1.0+cos(4*t)));
								
				for (unsigned int y = 1; y < animation.properties().height-1; y++){
					for (unsigned x = 1; x < animation.properties().width-1; x++){
						for (unsigned c = 0; c < 3; c++)
							main_window.gl_buffer [main_window.gl_buffer_properties.width*3*(pos.y+y) + 3*(pos.x+x) + c] = animation_temp[(animation.properties().height- y-1)][3*x+c];
					}
				}
				for (unsigned x = 1; x < animation.properties().width-1; x++){
					for (unsigned c = 0; c < 3; c++){
						main_window.gl_buffer [main_window.gl_buffer_properties.width*3*(pos.y+0) + 3*(pos.x+x) + c] = 125;
						main_window.gl_buffer [main_window.gl_buffer_properties.width*3*(pos.y+animation.properties().height-1) + 3*(pos.x+x) + c] = 125;
					}
				}

				for (unsigned int y = 0; y < animation.properties().height; y++){
						for (unsigned c = 0; c < 3; c++){
							main_window.gl_buffer [main_window.gl_buffer_properties.width*3*(pos.y+y) + 3*(pos.x+0) + c] = 125;
							main_window.gl_buffer [main_window.gl_buffer_properties.width*3*(pos.y+y) + 3*(pos.x+animation.properties().width-1) + c] = 125;
						}
				}
		}
		main_window.display_lock.release();
	}
	catch(...){
		main_window.display_lock.release();
		throw;
	}
	//main_window.redraw_screen();
}

void ns_worm_learner::update_main_window_display(){

	unsigned long	new_gl_image_pane_width = (main_window.gl_image_size.x );
	unsigned long	new_gl_image_pane_height =  (main_window.gl_image_size.y);

	if (new_gl_image_pane_width == 0)
		new_gl_image_pane_width = 100;
	if (new_gl_image_pane_height == 0)
		new_gl_image_pane_width = 100;

//	cerr << "worm_learner has received a request for a gl_window of size " << new_gl_image_pane_width << "x" << new_gl_image_pane_height << "\n";
	float zoom_x =(float)((float)new_gl_image_pane_width)/main_window.gl_buffer_properties.width;
	float zoom_y = (float)((float)new_gl_image_pane_height)/main_window.gl_buffer_properties.height;
	if (zoom_x < .01)
		zoom_x = .01;
	
	
	ns_acquire_lock_for_scope lock(main_window.display_lock,__FILE__,__LINE__);
	//if (zoom_x < zoom_y)
	main_window.image_zoom = zoom_x;
	//else main_window.image_zoom = zoom_y;
		

	main_window.gl_image_size.x = (unsigned int)(main_window.gl_buffer_properties.width * main_window.image_zoom);
	main_window.gl_image_size.y = (unsigned int)(main_window.gl_buffer_properties.height * main_window.image_zoom);
	
	//cerr << "To fit the new dimensions, the ideal window is changed to " << main_window.image_size.x << "x" << main_window.image_size.y<< "\n";
	glPixelZoom(main_window.image_zoom*main_window.display_rescale_factor,main_window.image_zoom*main_window.display_rescale_factor);
	glRasterPos2i((GLint)-1 ,(GLint)-1 );
	glPixelStorei(GL_PACK_ALIGNMENT,1);
	glPixelStorei(GL_UNPACK_ALIGNMENT,1);
	glDrawPixels(main_window.gl_buffer_properties.width, main_window.gl_buffer_properties.height,GL_RGB,GL_UNSIGNED_BYTE,main_window.gl_buffer );
	lock.release();
	
}


void ns_worm_learner::update_worm_window_display(){

	unsigned long new_gl_image_pane_width = (worm_window.gl_image_size.x);
	unsigned long new_gl_image_pane_height =  (worm_window.gl_image_size.y);

	if (new_gl_image_pane_width == 0)
		new_gl_image_pane_width = 100;
	if (new_gl_image_pane_height == 0)
		new_gl_image_pane_height = 100;

	float zoom_x =(float)((float)new_gl_image_pane_width)/worm_window.gl_buffer_properties.width;
	float zoom_y = (float)((float)new_gl_image_pane_height)/worm_window.gl_buffer_properties.height;
	if (zoom_x < .01)
		zoom_x = .01;
	
	ns_acquire_lock_for_scope lock(worm_window.display_lock,__FILE__,__LINE__);

	float cur_resize = (worm_window.gl_buffer_properties.width * worm_window.image_zoom) / (float)worm_window.gl_image_size.x;
	
	worm_window.image_zoom = zoom_x;
	worm_window.gl_image_size.x = (unsigned int)((worm_window.gl_buffer_properties.width * worm_window.image_zoom));
	worm_window.gl_image_size.y= (unsigned int)((worm_window.gl_buffer_properties.height * worm_window.image_zoom));

	worm_window.worm_image_size = worm_window.worm_image_size*cur_resize;
	worm_window.telemetry_size = worm_window.gl_image_size - worm_window.worm_image_size;

	
//	cerr << "update display needs to resize the worm image to " << worm_window.image_size << "\n";
	//xxxx
	glPixelZoom(worm_window.image_zoom*worm_window.display_rescale_factor,worm_window.image_zoom*worm_window.display_rescale_factor);
	glRasterPos2i((GLint)-1 ,(GLint)-1 );
	glPixelStorei(GL_PACK_ALIGNMENT,1);
	glPixelStorei(GL_UNPACK_ALIGNMENT,1);
	glDrawPixels(worm_window.gl_buffer_properties.width, worm_window.gl_buffer_properties.height,GL_RGB,GL_UNSIGNED_BYTE,worm_window.gl_buffer );
	lock.release();
}
void ns_experiment_storyboard_annotater::load_from_storyboard(const ns_region_metadata & strain_to_display_, const ns_censor_masking censor_masking_, ns_experiment_storyboard_spec & spec, ns_worm_learner * worm_learner_) {
	stop_fast_movement();
	clear();
	ns_acquire_lock_for_scope lock(image_buffer_access_lock, __FILE__, __LINE__);
	worm_learner = worm_learner_;
	strain_to_display = strain_to_display_;
	censor_masking = censor_masking_;
	ns_sql &sql(worm_learner_->get_sql_connection());
	excluded_regions.clear();
	if (spec.experiment_id != 0) {
		sql << "SELECT r.id, r.excluded_from_analysis FROM sample_region_image_info as r, capture_samples as s WHERE r.sample_id = s.id AND s.experiment_id = " << spec.experiment_id;
		ns_sql_result res;
		sql.get_rows(res);
		for (unsigned long i = 0; i < res.size(); i++)
			excluded_regions[atol(res[i][0].c_str())] = res[i][1] != "0";

	}
	else if (spec.sample_id != 0) {
		sql << "SELECT r.id, r.excluded_from_analysis FROM sample_region_image_info as r, capture_samples as s WHERE r.sample_id = s.id AND s.id = " << spec.sample_id;
		ns_sql_result res;
		sql.get_rows(res);
		for (unsigned long i = 0; i < res.size(); i++)
			excluded_regions[atol(res[i][0].c_str())] = res[i][1] != "0";

	}
	else if (spec.region_id != 0) {
		sql << "SELECT r.excluded_from_analysis FROM sample_region_image_info as r WHERE r.id = " << spec.region_id;
		ns_sql_result res;
		sql.get_rows(res);
		for (unsigned long i = 0; i < res.size(); i++)
			excluded_regions[spec.region_id] = res[i][0] != "0";
	}
	storyboard_manager.load_metadata_from_db(spec, storyboard, sql);
	unsigned long number_of_nonempty_divisions(0);
	for (unsigned int i = 0; i < storyboard.divisions.size(); i++) {
		if (storyboard.divisions[i].events.size() > 0)
			number_of_nonempty_divisions++;
	}

	divisions.resize(0);
	divisions.resize(number_of_nonempty_divisions);
	unsigned long cur_i(0);
	for (unsigned int i = 0; i < storyboard.divisions.size(); i++) {
		divisions[cur_i].init();
		if (storyboard.divisions[i].events.size() == 0)
			continue;
		divisions[cur_i].division_id = i;
		divisions[cur_i].division = &storyboard.divisions[i];
		divisions[cur_i].resize_factor = resize_factor;
		divisions[cur_i].experiment_annotater = this;

		for (unsigned int j = 0; j < divisions[cur_i].division->events.size(); j++) {

			if (censor_masking == ns_show_all)
				divisions[cur_i].division->events[j].annotation_was_censored_on_loading = false;
			if (censor_masking == ns_hide_censored)
				divisions[cur_i].division->events[j].annotation_was_censored_on_loading = divisions[cur_i].division->events[j].event_annotation.is_excluded();
			if (censor_masking == ns_hide_uncensored)
				divisions[cur_i].division->events[j].annotation_was_censored_on_loading = !divisions[cur_i].division->events[j].event_annotation.is_excluded();
		}

		if (!strain_to_display.device_regression_match_description().empty()) {
			for (unsigned int j = 0; j < divisions[cur_i].division->events.size(); j++) {
				display_events_from_region[divisions[cur_i].division->events[j].event_annotation.region_info_id] = false;
			}
		}
		else {
			for (unsigned int j = 0; j < divisions[cur_i].division->events.size(); j++) {
				display_events_from_region[divisions[cur_i].division->events[j].event_annotation.region_info_id] = true;
			}
		}
		cur_i++;
	}
	if (divisions.size() == 0)
		throw ns_ex("No divisions or animals present in storyboard");

	if (!strain_to_display.device_regression_match_description().empty())
		for (ns_event_display_spec_list::iterator p = display_events_from_region.begin(); p != display_events_from_region.end(); p++) {
			ns_region_metadata m;
			m.load_from_db(p->first, "", sql);
			p->second = (m.device_regression_match_description() == strain_to_display.device_regression_match_description());
		}

	//	cerr << divisions.size() << " divisions loaded.\n Loading images...";
	this->initalize_division_image_population(sql);
	//cerr << "Done.\n";

	//allocate image buffer
	if (previous_images.size() != max_buffer_size || next_images.size() != max_buffer_size) {
		previous_images.resize(max_buffer_size);
		next_images.resize(max_buffer_size);
		for (unsigned int i = 0; i < max_buffer_size; i++) {
			previous_images[i].im = new ns_image_standard();
			next_images[i].im = new ns_image_standard();
		}
	}
	if (current_image.im == 0)
		current_image.im = new ns_image_standard();

	current_timepoint_id = 0;

	divisions[current_timepoint_id].load_image(0, current_image, sql, asynch_load_specification.temp_buffer, resize_factor);
	draw_metadata(&divisions[current_timepoint_id], *current_image.im);
	this->saved_ = true;
	request_refresh();
	lock.release();
}

bool ns_worm_learner::start_death_time_annotation(const ns_behavior_mode m, const ns_experiment_storyboard_spec::ns_storyboard_flavor & f) {
	if (m != ns_annotate_storyboard_experiment && !data_selector.region_selected()) throw ns_ex("No region selected");
	//if (!current_annotater->data_saved())
	//	save_death_time_annotations();
	//if (this->behavior_mode ==	ns_annotate_storyboard_region ||
	//	this->behavior_mode == ns_annotate_storyboard_sample ||
	//	this->behavior_mode == ns_annotate_storyboard_experiment){
	
	current_storyboard_flavor = f;
	
	area_handler.clear_boxes();
	try{
		if (m == ns_worm_learner::ns_annotate_storyboard_region || 
			m == ns_worm_learner::ns_annotate_storyboard_sample || 
			m == ns_worm_learner::ns_annotate_storyboard_experiment){
				current_annotater = &storyboard_annotater;
				storyboard_annotater.clear();

				ns_experiment_storyboard_spec subject;
				switch(m){
					case ns_worm_learner::ns_annotate_storyboard_region:
						subject.region_id = data_selector.current_region().region_id;
						break;
					case ns_worm_learner::ns_annotate_storyboard_sample:
						subject.sample_id = data_selector.current_sample().sample_id;
						break;
					case ns_worm_learner::ns_annotate_storyboard_experiment:
						subject.experiment_id = data_selector.current_experiment_id();
						break;
				}
				subject.event_to_mark = ns_image_server::movement_event_used_for_animal_storyboards();
				subject.set_flavor(f);
				ns_region_metadata metadata;
				if (data_selector.strain_selected())
					metadata = data_selector.current_strain();

				ns_experiment_region_selector::ns_censor_masking c(data_selector.censor_masking());
				ns_experiment_storyboard_annotater::ns_censor_masking c2;

				c2 = (ns_experiment_storyboard_annotater::ns_censor_masking)((int)c);
				
				storyboard_annotater.load_from_storyboard(metadata,c2,subject,this);
				storyboard_annotater.display_current_frame();
				set_behavior_mode(m);
		}
		else{
			current_annotater = &death_time_annotater;
			death_time_annotater.clear();

			unsigned long region_id = data_selector.current_region().region_id;
			ns_update_information_bar(string("Annotating ") + data_selector.current_region().region_name);
			ns_death_time_posture_annotater::ns_alignment_type type;
			switch(m){
				case ns_worm_learner::ns_annotate_death_times_in_time_aligned_posture:
					type = ns_death_time_posture_annotater::ns_time_aligned_images;
					break;
				case ns_worm_learner::ns_annotate_death_times_in_death_aligned_posture:
					type = ns_death_time_posture_annotater::ns_death_aligned_images;
					break;
			}
	
			death_time_annotater.load_region(data_selector.current_region().region_id,type,this);
			death_time_annotater.display_current_frame();
		}
		
			set_behavior_mode(m);
	}
	catch(...){
		stop_death_time_annotation();
		throw;
	}
	
	return true;
}

#ifdef _WIN32
bool ns_load_image_from_resource(int resource_id,const std::string &filename){
	HRSRC hResource = ::FindResource(0, MAKEINTRESOURCE(resource_id), "BIN");
	if (!hResource){
		ns_ex ex;
		ex.append_windows_error();
		throw ex;
	}
	HGLOBAL hresload = ::LoadResource(0, hResource);
	if (hresload==NULL){
		ns_ex ex;
		ex.append_windows_error();
		throw ex;
	}
    const void* pResourceData = ::LockResource(hresload);
    if (!pResourceData){
		ns_ex ex;
		ex.append_windows_error();
		throw ex;
	}
    DWORD imageSize = ::SizeofResource(0, hResource);
    if (!imageSize){
		ns_ex ex;
		ex.append_windows_error();
		throw ex;
	}
	const char * buf(static_cast<const char *>(pResourceData));
	ofstream out(filename.c_str(),ios_base::binary);
	if (out.fail())
		throw ns_ex("Could not open file ") << filename << " for writing";
	for (unsigned int i = 0; i < imageSize; i++)
		out<<buf[i];
	out.close();
	return true;
}
#endif

void ns_worm_learner::display_splash_image(){
	ns_image_standard im;
	#ifdef _WIN32
	string tmp_filename("start_background.tif");
	ns_load_image_from_resource(IDR_BIN1,tmp_filename);
	ns_load_image(tmp_filename,im);
	im.resample(ns_image_properties(600,800,3),current_image);
	ns_dir::delete_file(tmp_filename);
	#else
	// note: using implicit string-literal concatenation after preprocessor substitution of NS_DATA_PATH
	ns_load_image(NS_DATA_PATH "start.tif",im);
	im.resample(ns_image_properties(600,800,3),current_image);
	draw_image(-1,-1,current_image);
	#endif
}
void ns_worm_learner::stop_death_time_annotation(){
	if (behavior_mode == ns_worm_learner::ns_draw_boxes)
		return;
	//request the user save; stop the close operation on cancel
	if (!prompt_to_save_death_time_annotations())
		return;
	death_time_annotater.clear();
	set_behavior_mode(ns_worm_learner::ns_draw_boxes);
	ns_hide_worm_window();
	display_splash_image();
	ns_update_information_bar("");
	death_time_solo_annotater.stop_fast_movement();
	death_time_solo_annotater.clear();
	storyboard_annotater.stop_fast_movement();
	storyboard_annotater.clear();

}

void ns_death_time_posture_annotater::display_current_frame(){
	refresh_requested_ = false;
	ns_acquire_lock_for_scope lock(image_buffer_access_lock,__FILE__,__LINE__);
	worm_learner->draw_image(-1,-1,*current_image.im);
	lock.release();
}

void ns_death_time_solo_posture_annotater::display_current_frame(){
	refresh_requested_ = false;
	//ns_acquire_lock_for_scope lock(image_buffer_access_lock,__FILE__,__LINE__);
	worm_learner->draw_worm_window_image(*current_image.im);
	//lock.release();
}

void ns_experiment_storyboard_annotater::display_current_frame(){
	refresh_requested_ = false;
	ns_acquire_lock_for_scope lock(image_buffer_access_lock,__FILE__,__LINE__);
	if (current_image.im == 0)
		throw ns_ex("No frame loaded!");
	worm_learner->draw_image(-1,-1,*current_image.im);
	lock.release();
}

void ns_worm_learner::navigate_solo_worm_annotation(ns_death_time_solo_posture_annotater::ns_image_series_annotater_action action, bool asynch){
	switch(action){
		case ns_death_time_solo_posture_annotater::ns_none: break;
		case ns_death_time_solo_posture_annotater::ns_forward: 
			if(death_time_solo_annotater.step_forward(ns_hide_worm_window,asynch))
				death_time_solo_annotater.request_refresh();
			break;
		case ns_death_time_solo_posture_annotater::ns_back:	
			if(death_time_solo_annotater.step_back(&ns_hide_worm_window,asynch)) 
				death_time_solo_annotater.request_refresh();
			break;
		case ns_death_time_solo_posture_annotater::ns_fast_forward: death_time_solo_annotater.fast_forward();break;
		case ns_death_time_solo_posture_annotater::ns_fast_back:	death_time_solo_annotater.fast_back();break;
		case ns_death_time_solo_posture_annotater::ns_stop: death_time_solo_annotater.stop_fast_movement();break;
		case ns_death_time_solo_posture_annotater::ns_rewind_to_zero: break;
		case ns_death_time_solo_posture_annotater::ns_step_visualization: 
			solo_annotation_visualization_type = death_time_solo_annotater.step_visualization_type();
			death_time_solo_annotater.request_refresh();
			break;
		case ns_death_time_solo_posture_annotater::ns_step_graph:
			death_time_solo_annotater.step_graph_type();
			//death_time_solo_annotater.display_current_frame();
			death_time_solo_annotater.request_refresh();
			break;
		case ns_death_time_solo_posture_annotater::ns_write_quantification_to_disk: 
			death_time_solo_annotater.register_click(ns_vector_2i(0,0),ns_death_time_solo_posture_annotater::ns_output_images); break;
		default: throw ns_ex("Unkown death time annotater action");
	}
}
void ns_zero_death_interval(ns_death_time_annotation_time_interval & e){
	e.period_start = e.period_end = 0;
	e.period_end_was_not_observed = false;
	e.period_start_was_not_observed = false;
}
void ns_crop_time(const ns_time_path_limits & limits,  const ns_death_time_annotation_time_interval & first_observation_in_path, const ns_death_time_annotation_time_interval & last_observation_in_path,ns_death_time_annotation_time_interval & target){
	if (target.period_end == 0)
		return;
	//only crop to the beginning if the animal didn't become stationary in the first frame!
	if (!limits.interval_before_first_observation.period_start_was_not_observed
		&&
		(target.period_start <=  limits.interval_before_first_observation.period_start ||
		target.period_end <= limits.interval_before_first_observation.period_end))
		target =  limits.interval_before_first_observation;

	if (!limits.last_obsevation_of_plate.period_end_was_not_observed &&
		target.period_end >= last_observation_in_path.period_end)
		target = last_observation_in_path;
}
ns_color_8 ns_annotation_flag_color(ns_death_time_annotation & a){
	if (a.is_excluded())
		return ns_color_8(255,255,255);
	return a.flag.flag_color();
}
void ns_throw_error(){
	cerr << "Error! Could not load frame!\n";
}

void ns_worm_learner::navigate_death_time_annotation(ns_image_series_annotater::ns_image_series_annotater_action action, bool asynch){
	
	switch(action){
		case ns_image_series_annotater::ns_none: break;
		case ns_image_series_annotater::ns_forward: 
			if (current_annotater->step_forward(ns_throw_error, asynch)) {
				current_annotater->request_refresh(); report_changes_made_to_screen();
			}
			break;
		case ns_image_series_annotater::ns_back:	
			if (current_annotater->step_back(ns_throw_error, asynch)) {
				current_annotater->request_refresh(); report_changes_made_to_screen();
			}
			break;
		case ns_image_series_annotater::ns_fast_forward: current_annotater->fast_forward(); report_changes_made_to_screen(); break;
		case ns_image_series_annotater::ns_fast_back:	current_annotater->fast_back(); report_changes_made_to_screen(); break;
		case ns_image_series_annotater::ns_stop: current_annotater->stop_fast_movement(); report_changes_made_to_screen(); break;
		case ns_image_series_annotater::ns_save:{

		  //ns_acquire_lock_for_scope lock(persistant_sql_lock, __FILE__, __LINE__);
			save_death_time_annotations(get_sql_connection());
			//lock.release();

			break;
		}
		default: throw ns_ex("Unkown death time annotater action");
	}
}

void ns_worm_learner::output_distributions_of_detected_objects(const std::string &directory){
	if (worm_detection_results == 0)
		throw ns_ex("No results have been calculated");
	

	

	 const std::vector<const ns_detected_worm_info *> &worms(worm_detection_results->actual_worm_list());

	 const std::vector<const ns_detected_worm_info *> &non_worms(worm_detection_results->non_worm_list());

	 
	 std::vector<ns_detected_worm_stats> worm_stats(worms.size());
	 std::vector<ns_detected_worm_stats> non_worm_stats(non_worms.size());

	for (unsigned int i = 0; i < worms.size(); i++)
		worm_stats[i] = worms[i]->generate_stats();
	for (unsigned int i = 0; i < non_worms.size(); i++)
		non_worm_stats[i] = non_worms[i]->generate_stats();
	
	ns_detected_worm_stats::draw_feature_frequency_distributions(worm_stats, non_worm_stats,current_filename,directory);
}


bool ns_worm_learner::mask_loaded(){
	return current_mask.properties().components != 0;
}

bool ns_a_is_above_or_left_of_b(const ns_vector_2i &a,const ns_vector_2i & b,int extra_buffer){
	return a.x-extra_buffer < b.x || a.y-extra_buffer < b.y;
}

ns_worm_learner::~ns_worm_learner(){
  if (main_window.gl_buffer != 0)
    delete[] main_window.gl_buffer;
  if (worm_window.gl_buffer != 0)
    delete[] worm_window.gl_buffer;
  ns_safe_delete(worm_detection_results);
  model_specifications.resize(0);
}

void ns_worm_learner::train_from_data(const std::string & base_dir){
	ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__,__LINE__));
	training_file_generator.generate_from_curated_set(base_dir,(*model_specification)().model_specification,true,&sql());
	sql.release();
}
const ns_svm_model_specification & ns_worm_learner::get_svm_model_specification(){
	return (*model_specification)().model_specification;
}

extern ns_worm_learner worm_learner;

void ns_area_handler::click(const ns_handle_action & action,const ns_vector_2i & image_pos,const ns_vector_2i & screen_pos,ns_8_bit * screen_buffer, const ns_image_standard & background,const unsigned long image_scaling, const double pixel_scaling){
	switch(action){
		case ns_deselect_handle:{
		//	cerr << "DESELECTING\n";
			if (!moved_since_last_click && !created_new_box_in_current_click){
				for (std::vector<ns_area_box>::iterator b = boxes.begin(); b != boxes.end();){
					if ( (b->screen_coords.top_left - screen_pos).mag() < 4){
				//		cerr << "Removing Top Corner\n";
						remove_box_from_screen_buffer(b,screen_buffer,background,image_scaling,pixel_scaling);
						b = boxes.erase(b);
						current_unfinished_box = boxes.end();
						unfinished_box_exists = false;
						selected_box = boxes.end();
						selected_box_exists = false;
						worm_learner.main_window.redraw_screen();
						moved_since_last_click=false;
						created_new_box_in_current_click=false;
						return;
					}
					else if ( !(b->screen_coords.bottom_right == ns_vector_2i(-1,-1)) && (b->screen_coords.bottom_right - screen_pos).mag() < 4){
						
					//	cerr << "Removing Top Corner " << "\n";
						remove_box_from_screen_buffer(b,screen_buffer,background,image_scaling,pixel_scaling);
						b->image_coords.bottom_right = ns_vector_2i(-1,-1);
						b->screen_coords.bottom_right = ns_vector_2i(-1,-1);
						current_unfinished_box = b;
						unfinished_box_exists = true;
						selected_box = boxes.end();
						selected_box_exists = false;
						draw_boxes(screen_buffer,background.properties(),image_scaling,pixel_scaling);
						worm_learner.main_window.redraw_screen();
						moved_since_last_click=false;
						created_new_box_in_current_click=false;
						return;
					}
					b++;
				}
			}
			//if during this run we created the bottom_right hand corner of a box and finished it
			//we're done and can deselect the box
			selected_box = boxes.end();
			selected_box_exists = false;
			moved_since_last_click=false;
			created_new_box_in_current_click=false;
			break;
		}
		case ns_select_handle:{
		//	cerr << "SELECTING\n";
			for (std::vector<ns_area_box>::iterator b = boxes.begin(); b != boxes.end();b++){
				ns_box::ns_box_location contact_location(b->screen_coords.corner_contact(screen_pos));
				switch(contact_location){
				case ns_box::ns_none: continue;
				case ns_box::ns_top_left:
					selected_box = b;
					selected_box_exists = true;
					if (b->screen_coords.bottom_right  == ns_vector_2i(-1,-1)){
						current_unfinished_box = b;
						unfinished_box_exists = true;
					}
					break;
				case ns_box::ns_bottom_right:
				case ns_box::ns_top_right:
				case ns_box::ns_bottom_left:
					selected_box = b;
					selected_box_exists = true;
					current_unfinished_box = boxes.end();
					unfinished_box_exists = false;
					break;
				}
				cur_box_handle = contact_location;
				return;
			}
							  
			//If we haven't selected an existing point, make a new one.
			if (!selected_box_exists){
				//if we have an unfinished box, finish it
				if (unfinished_box_exists){
					//reject invalid choices that might creep in
					if (current_unfinished_box->screen_coords.top_left.x + 4 > screen_pos.x ||
						current_unfinished_box->screen_coords.top_left.y + 4 > screen_pos.y){
						return;
					}

					remove_box_from_screen_buffer(current_unfinished_box,screen_buffer,background,image_scaling,pixel_scaling);
					current_unfinished_box->image_coords.bottom_right = image_pos;
					current_unfinished_box->screen_coords.bottom_right = screen_pos;
					cur_box_handle = ns_box::ns_bottom_right;
					created_new_box_in_current_click  = true;
					selected_box = current_unfinished_box;
					selected_box_exists  = true;
					current_unfinished_box = boxes.end();
					unfinished_box_exists = false;
				}
				else{
					current_unfinished_box = boxes.insert(boxes.end(),ns_area_box());
					current_unfinished_box->screen_coords.top_left = screen_pos;
					current_unfinished_box->image_coords.top_left = image_pos;
					current_unfinished_box->screen_coords.bottom_right = ns_vector_2i(-1,-1);
					current_unfinished_box->image_coords.bottom_right = ns_vector_2i(-1,-1);
					cur_box_handle = ns_box::ns_top_left;
					selected_box = current_unfinished_box;
					created_new_box_in_current_click  = true;
					selected_box_exists = true;
					unfinished_box_exists = true;
				}
			}			
			draw_boxes(screen_buffer,background.properties(),image_scaling,pixel_scaling);
			
			worm_learner.main_window.redraw_screen();
			return;
		 }
		case ns_move_handle:{
		//	cerr << "MOVING\n";
			if (!selected_box_exists) return;
			moved_since_last_click = true;
			remove_box_from_screen_buffer(selected_box,screen_buffer,background,image_scaling,pixel_scaling);
			selected_box->assign_and_correct_inversions(cur_box_handle,screen_pos,image_pos);
		
			draw_boxes(screen_buffer,background.properties(),image_scaling,pixel_scaling);
			
			worm_learner.main_window.redraw_screen();
			break;
		}
		case ns_move_all_boxes:{
			selected_box = boxes.end();
			for (std::vector<ns_area_box>::iterator b = boxes.begin(); b != boxes.end();b++){
				remove_box_from_screen_buffer(b,screen_buffer,background,image_scaling,pixel_scaling);
				if(ns_a_is_above_or_left_of_b(b->image_coords.bottom_right+image_pos,ns_vector_2i(0,0),4) ||
					ns_a_is_above_or_left_of_b(ns_vector_2i(background.properties().width,background.properties().height),
						b->image_coords.top_left+image_pos,4))
					continue;

		    	moved_since_last_click = true;
				b->image_coords.bottom_right+=image_pos;
				b->image_coords.top_left+=image_pos;
				b->screen_coords.bottom_right+=screen_pos;
				b->screen_coords.top_left+=screen_pos;
						
			}	
			
			draw_boxes(screen_buffer,background.properties(),image_scaling,pixel_scaling);
			worm_learner.main_window.redraw_screen();
			break;
		}

	}
	
}
void ns_area_handler::output_boxes(ostream & out, const string &device_name, const float & resolution, const string & units){
	for (unsigned int i = 0; i < boxes.size(); i++){
		out << "<sample><device>"<< device_name << "</device><scan_area>"
			<< boxes[i].image_coords.top_left.x/resolution << units << ","
			<<  boxes[i].image_coords.top_left.y/resolution << units << ","
			<< (boxes[i].image_coords.bottom_right.x -boxes[i].image_coords.top_left.x)/resolution << units << ","
			<< (boxes[i].image_coords.bottom_right.y -boxes[i].image_coords.top_left.y)/resolution  << units
			<< "</scan_area></sample>\n";
	}
}
void ns_area_handler::clear_boxes(){
	boxes.resize(0);
	current_unfinished_box = boxes.end();
	selected_box = boxes.end();
}
void ns_draw_square_boxes(const ns_vector_2i & loc,ns_8_bit * screen_buffer,const ns_image_properties & properties,const unsigned long size,const double scaling){
	for (unsigned int y = 0; y < size; y++){
		for (unsigned int x = 0; x < size; x++){
				if (properties.height < (1 + loc.y+y))
					throw ns_ex("Yikes!");
				screen_buffer[0] = 0;

				screen_buffer[properties.width*3*(properties.height-1 - loc.y-y)+3*(loc.x+x)] = 200;
				screen_buffer[properties.width*3*(properties.height-1 - loc.y-y)+3*(loc.x+x)+1] = 0;
				screen_buffer[properties.width*3*(properties.height-1 - loc.y-y)+3*(loc.x+x)+2] = 0;
		}
	}
}
void ns_restore_square_boxes(const ns_vector_2i & loc,ns_8_bit * screen_buffer,const ns_image_properties & properties,const unsigned long size,const double scaling, const ns_image_standard & background){

	if (properties.components == 3){
		for (unsigned int y = 0; y < size; y++){
			for (unsigned int x = 0; x < size; x++){
		
				screen_buffer[properties.width*3*(properties.height-1 - y-loc.y)+3*(loc.x+x)] = background[(unsigned long)(scaling*(y+loc.y))][3*((unsigned long)(scaling*(loc.x+x)))];
				screen_buffer[properties.width*3*(properties.height-1 - y-loc.y)+3*(loc.x+x)+1] = background[(unsigned long)(scaling*(y+loc.y))][(3*((unsigned long)(scaling*(loc.x+x))))+1];
				screen_buffer[properties.width*3*(properties.height-1 - y-loc.y)+3*(loc.x+x)+2] = background[(unsigned long)(scaling*(y+loc.y))][(3*((unsigned long)(scaling*(loc.x+x))))+2];
			}
		}
	}
	else{
		for (unsigned int y = 0; y < size; y++){
			for (unsigned int x = 0; x < size; x++){
				screen_buffer[properties.width*3*(properties.height-1 - y-loc.y)+3*(loc.x+x)] = background[(unsigned long)(scaling*(y+loc.y))][(unsigned long)(scaling*(loc.x+x))];
				screen_buffer[properties.width*3*(properties.height-1 - y-loc.y)+3*(loc.x+x)+1] = background[(unsigned long)(scaling*(y+loc.y))][(unsigned long)(scaling*(loc.x+x))];
				screen_buffer[properties.width*3*(properties.height-1 - y-loc.y)+3*(loc.x+x)+2] = background[(unsigned long)(scaling*(y+loc.y))][(unsigned long)(scaling*(loc.x+x))];
			}
		}
	}
}
void ns_area_handler::draw_boxes(ns_8_bit * screen_buffer,const ns_image_properties & properties_,const unsigned long scaling, const double pixel_scaling) const{
	ns_image_properties properties(properties_);
	properties.height/=scaling;
	properties.width/=scaling;
	const unsigned int h(properties.height),
		w(properties.width),
		c(properties.components);
		
	int px(2*ceil(pixel_scaling));

	for (unsigned int i = 0; i < boxes.size(); i++){
		ns_vector_2i a(boxes[i].image_coords.top_left/scaling),
			b(boxes[i].image_coords.bottom_right/scaling);
	//	cerr << "prop: " << h << "x" << w << ":" << c << "\t scaling: " << scaling << ":" << a << " " << a << "\n";
		ns_draw_square_boxes(a,screen_buffer,properties,2*px,scaling);
		if (!(boxes[i].image_coords.bottom_right == ns_vector_2i(-1,-1))){
			ns_draw_square_boxes(ns_vector_2i(b.x-2*px,a.y),screen_buffer,properties,2*px,scaling);
			ns_draw_square_boxes(ns_vector_2i(a.x,b.y-2*px),screen_buffer,properties,2*px,scaling);
			ns_draw_square_boxes(b-ns_vector_2i(2*px,2*px),screen_buffer,properties,2*px,scaling);
		}

		if (boxes[i].image_coords.bottom_right == ns_vector_2i(-1,-1)){
			
			for (unsigned int y = a.y; y + px < h; y+=4*px){
				for (unsigned int y_ = 0; y_ < px; y_++){
					for (unsigned int x = 0; x < px; x++){
						screen_buffer[properties.width*3*((properties.height-1) - (y+y_))+3*(a.x+x)] = 200;
						screen_buffer[properties.width*3*((properties.height-1) - (y+y_))+3*(a.x+x)+1] = 0;
						screen_buffer[properties.width*3*((properties.height-1) - (y+y_))+3*(a.x+x)+2] = 0;
					}
				}
			}
			for (unsigned int y = 0; y < px; y++){
				for (unsigned int x = a.x; x + px < w; x+=4*px){
					for (unsigned int x_ = 0; x_ < px; x_++){
						screen_buffer[properties.width*3*((properties.height-1) - a.y-y)+3*(x+x_)] = 200;
						screen_buffer[properties.width*3*((properties.height-1) - a.y-y)+3*(x+x_)+1] = 0;
						screen_buffer[properties.width*3*((properties.height-1) - a.y-y)+3*(x+x_)+2] = 0;
					}
				}
			}
		}
		else{
			for (unsigned int y = a.y; y + px < b.y; y+=4*px){
				for (unsigned int y_ = 0; y_ < px; y_++){
					for (unsigned int x = 0; x < px; x++){
					screen_buffer[properties.width*3*((properties.height-1) - (y+y_))+3*(a.x+x)] = 200;
					screen_buffer[properties.width*3*((properties.height-1) - (y+y_))+3*(a.x+x)+1] = 0;
					screen_buffer[properties.width*3*((properties.height-1) - (y+y_))+3*(a.x+x)+2] = 0;
				
					screen_buffer[properties.width*3*((properties.height-1) - (y+y_))+3*(b.x+x)] = 200;
					screen_buffer[properties.width*3*((properties.height-1) - (y+y_))+3*(b.x+x)+1] = 0;
					screen_buffer[properties.width*3*((properties.height-1) - (y+y_))+3*(b.x+x)+2] = 0;
					}
				}
			}
			for (unsigned int y = 0; y < px; y++){
				for (unsigned int x = a.x; x + px < b.x; x+=4*px){
					for (unsigned int x_ = 0; x_ < px; x_++){
						screen_buffer[properties.width*3*((properties.height-1) - a.y-y)+3*(x+x_)] = 200;
						screen_buffer[properties.width*3*((properties.height-1) - a.y-y)+3*(x+x_)+1] = 0;
						screen_buffer[properties.width*3*((properties.height-1) - a.y-y)+3*(x+x_)+2] = 0;
				
						screen_buffer[properties.width*3*((properties.height-1) - b.y-y)+3*(x+x_)] = 200;
						screen_buffer[properties.width*3*((properties.height-1) - b.y-y)+3*(x+x_)+1] = 0;
						screen_buffer[properties.width*3*((properties.height-1 )- b.y-y)+3*(x+x_)+2] = 0;
					}
				}
			}
		
		}
	}
}



void ns_area_handler::remove_box_from_screen_buffer(const std::vector<ns_area_box>::iterator cur_box,ns_8_bit * screen_buffer, const ns_image_standard & background,const unsigned long image_scaling,const double pixel_scaling) const{
	
	ns_vector_2i a(cur_box->image_coords.top_left/image_scaling),
				 b(cur_box->image_coords.bottom_right/image_scaling);

	//const int px(ceil(1.0/pixel_scaling));

	ns_image_properties properties(background.properties());	
	properties.width/=image_scaling;
	properties.height/=image_scaling;
	const unsigned int h(properties.height),
	w(properties.width),
	c(properties.components);
	int px(2*ceil(pixel_scaling));

	ns_restore_square_boxes(a,screen_buffer,properties,2*px,image_scaling,background);
	if (!(cur_box->image_coords.bottom_right == ns_vector_2i(-1,-1))){
		ns_restore_square_boxes(ns_vector_2i(b.x-2*px,a.y),screen_buffer,properties,2*px,image_scaling,background);
		ns_restore_square_boxes(ns_vector_2i(a.x,b.y-2*px),screen_buffer,properties,2*px,image_scaling,background);
		ns_restore_square_boxes(b-ns_vector_2i(2*px,2*px),screen_buffer,properties,2*px,image_scaling,background);
	}

	if (properties.components == 3){
		if (cur_box->image_coords.bottom_right == ns_vector_2i(-1,-1)){
			for (unsigned int y = a.y; y < h; y++){
				for (unsigned int x = 0; x < px; x++){
					screen_buffer[properties.width*3*(properties.height-1 - y)+3*(a.x+x)] = background[image_scaling*y][3*(image_scaling*a.x)];
					screen_buffer[properties.width*3*(properties.height-1 - y)+3*(a.x+x)+1] = background[image_scaling*y][3*(image_scaling*a.x)+1];
					screen_buffer[properties.width*3*(properties.height-1 - y)+3*(a.x+x)+2] = background[image_scaling*y][3*(image_scaling*a.x)+2];
				}
			}
			for (unsigned int y= 0; y < px; y++){
				for (unsigned int x = a.x; x < w; x++){
					screen_buffer[properties.width*3*(properties.height-1 - a.y-y)+3*x] = background[image_scaling*(a.y)][3*image_scaling*x];
					screen_buffer[properties.width*3*(properties.height-1 - a.y-y)+3*x+1] = background[image_scaling*(a.y)][3*image_scaling*x+1];
					screen_buffer[properties.width*3*(properties.height-1 - a.y-y)+3*x+2] = background[image_scaling*(a.y)][3*image_scaling*x+2];
				}
			}
		}
		else{
			for (int y = a.y; y < b.y; y++){
				for (unsigned int x = 0; x < px; x++){
				screen_buffer[properties.width*3*(properties.height-1 - y)+3*(a.x+x)] = background[image_scaling*y][3*image_scaling*a.x];
				screen_buffer[properties.width*3*(properties.height-1 - y)+3*(a.x+x)+1] = background[image_scaling*y][3*image_scaling*a.x+1];
				screen_buffer[properties.width*3*(properties.height-1 - y)+3*(a.x+x)+2] = background[image_scaling*y][3*image_scaling*a.x+2];
				
				screen_buffer[properties.width*3*(properties.height-1 - y)+3*(b.x+x)] = background[image_scaling*y][3*image_scaling*b.x];
				screen_buffer[properties.width*3*(properties.height-1 - y)+3*(b.x+x)+1] = background[image_scaling*y][3*image_scaling*b.x+1];
				screen_buffer[properties.width*3*(properties.height-1 - y)+3*(b.x+x)+2] = background[image_scaling*y][3*image_scaling*b.x+2];
				}
			}
			for (int y= 0; y < px; y++){
				for (int x = a.x; x < b.x; x++){
					screen_buffer[properties.width*3*(properties.height-1 - a.y-y)+3*x] = background[image_scaling*a.y][3*image_scaling*x];
					screen_buffer[properties.width*3*(properties.height-1 - a.y-y)+3*x+1] = background[image_scaling*a.y][3*image_scaling*x+1];
					screen_buffer[properties.width*3*(properties.height-1 - a.y-y)+3*x+2] = background[image_scaling*a.y][3*image_scaling*x+2];
				
					screen_buffer[properties.width*3*(properties.height-1 - b.y-y)+3*x] = background[image_scaling*b.y][3*image_scaling*x];
					screen_buffer[properties.width*3*(properties.height-1 - b.y-y)+3*x+1] = background[image_scaling*b.y][3*image_scaling*x+1];
					screen_buffer[properties.width*3*(properties.height-1 - b.y-y)+3*x+2] = background[image_scaling*b.y][3*image_scaling*x+2];
				}
			}
		}
	}
	else{
		if (cur_box->image_coords.bottom_right == ns_vector_2i(-1,-1)){
			for (unsigned int y = a.y; y < h; y++){
				for (unsigned int x = 0; x < px; x++){
					screen_buffer[properties.width*3*(properties.height-1 - y)+3*(a.x+x)] = background[image_scaling*y][image_scaling*a.x];
					screen_buffer[properties.width*3*(properties.height-1 - y)+3*(a.x+x)+1] = background[image_scaling*y][image_scaling*a.x];
					screen_buffer[properties.width*3*(properties.height-1 - y)+3*(a.x+x)+2] = background[image_scaling*y][image_scaling*a.x];
				}
			}
			for (int y = 0; y < px; y++){
				for (unsigned int x = a.x; x < w; x++){
					screen_buffer[properties.width*3*(properties.height-1 - a.y-y)+3*x] = background[image_scaling*a.y][image_scaling*x];
					screen_buffer[properties.width*3*(properties.height-1 - a.y-y)+3*x+1] = background[image_scaling*a.y][image_scaling*x];
					screen_buffer[properties.width*3*(properties.height-1 - a.y-y)+3*x+2] = background[image_scaling*a.y][image_scaling*x];
				}
			}
		}
		else{
			for (int y = a.y; y < b.y; y++){
				for (unsigned int x = 0; x < px; x++){
					screen_buffer[properties.width*3*(properties.height-1 - y)+3*(a.x+x)] =   background[image_scaling*y][image_scaling*a.x];
					screen_buffer[properties.width*3*(properties.height-1 - y)+3*(a.x+x)+1] = background[image_scaling*y][image_scaling*a.x];
					screen_buffer[properties.width*3*(properties.height-1 - y)+3*(a.x+x)+2] = background[image_scaling*y][image_scaling*a.x];
				
					screen_buffer[properties.width*3*(properties.height-1 - y)+3*(b.x+x)] =   background[image_scaling*y][image_scaling*b.x];
					screen_buffer[properties.width*3*(properties.height-1 - y)+3*(b.x+x)+1] = background[image_scaling*y][image_scaling*b.x];
					screen_buffer[properties.width*3*(properties.height-1 - y)+3*(b.x+x)+2] = background[image_scaling*y][image_scaling*b.x];
				}
			}
			for (unsigned int y= 0; y< px; y++){
				for (int x = a.x; x < b.x; x++){
					screen_buffer[properties.width*3*(properties.height-1 - a.y-y)+3*x] =   background[image_scaling*a.y][image_scaling*x];
					screen_buffer[properties.width*3*(properties.height-1 - a.y-y)+3*x+1] = background[image_scaling*a.y][image_scaling*x];
					screen_buffer[properties.width*3*(properties.height-1 - a.y-y)+3*x+2] = background[image_scaling*a.y][image_scaling*x];
				
					screen_buffer[properties.width*3*(properties.height-1 - b.y-y)+3*x] =   background[image_scaling*b.y][image_scaling*x];
					screen_buffer[properties.width*3*(properties.height-1 - b.y-y)+3*x+1] = background[image_scaling*b.y][image_scaling*x];
					screen_buffer[properties.width*3*(properties.height-1 - b.y-y)+3*x+2] = background[image_scaling*b.y][image_scaling*x];
				}
			}
		}
	}
}
void ns_region_growing_segmenter(const ns_image_standard & input, ns_image_standard & output, const unsigned int weak_edge_threshold, const double common_edge_threshold1, const double common_edge_threshold2){
	
	const unsigned long w(input.properties().width);
	const unsigned long h(input.properties().height);

	//assign pixels to groups in 2x2 blocks, and set up edge structures between all regions
	const unsigned long init_regions_per_row((w)/2);
	const unsigned long init_regions_per_column((h)/2);


	cerr << "Allocating region structure\n";
	std::vector<ns_region> regions(init_regions_per_row*init_regions_per_column);

	cerr << "Creating initial regions\n";

	//add top and bottom outside image edge sizes
	for (unsigned int x = 0; x < w/2; x++){
		regions[x].outside_image_edge_perimeter+=2;
		regions[regions.size()-1-x].outside_image_edge_perimeter+=2;
	}	
	//add left and right image edge sizes
	for (unsigned int y = 0; y < h/2; y++){
		regions[y*init_regions_per_row].outside_image_edge_perimeter+=2;
		regions[y*init_regions_per_row + init_regions_per_row - 1].outside_image_edge_perimeter+=2;
	}
	unsigned int region_id = 0;
	for (unsigned int y = 0; y < (h/2)*2; y+=2){
		for (unsigned int x = 0; x < (w/2)*2; x+=2){
			regions[region_id].id = region_id;
			regions[region_id].pixels.push_back(new ns_pixel_set);

			//cerr << "(" << l << "," << t << ")";
			regions[region_id].pixels[0]->push_back(ns_pixel_coord(x  ,y  ));
			regions[region_id].pixels[0]->push_back(ns_pixel_coord(x+1,y  ));
			regions[region_id].pixels[0]->push_back(ns_pixel_coord(x  ,y+1));
			regions[region_id].pixels[0]->push_back(ns_pixel_coord(x+1,y+1));

			regions[region_id].area = 4;
			regions[region_id].total_intensity = input[y][x] + input[y+1][x] + input[y][x+1] + input[y+1][x+1];

			if (x != 0){
				//add left neighbor edge to current region
				std::vector<ns_pixel_set *> *neighbor = &regions[region_id].edges[region_id-1].pixels;
				unsigned int s = (unsigned int)neighbor->size();
				neighbor->resize(s+1,new ns_pixel_set);
				(*neighbor)[s]->push_back(ns_pixel_coord(x,y  ,ns_vertical));
				(*neighbor)[s]->push_back(ns_pixel_coord(x,y+1,ns_vertical));
				//set left edge as neighbor's right edge
				regions[region_id-1].edges[region_id].pixels.push_back((*neighbor)[s]);
			}		
			if (y != 0){
				//add top neighbor edge to current region
				std::vector<ns_pixel_set *> * neighbor = &regions[region_id].edges[region_id-init_regions_per_row].pixels;
				unsigned int s = (unsigned int)neighbor->size();
				neighbor->resize(s+1, new ns_pixel_set);
				(*neighbor)[s]->push_back(ns_pixel_coord(x  ,y-1,ns_horizontal));
				(*neighbor)[s]->push_back(ns_pixel_coord(x+1,y-1,ns_horizontal));
				//set bottom edge as neighbor's top edge region
				regions[region_id-init_regions_per_row].edges[region_id].pixels.push_back((*neighbor)[s]);
			}
			regions[region_id].perimeter_length = 8;
			region_id++;
		}
	}
	//Make far left columns 3x3 if needed
	if (w%2 == 1){
		for (unsigned int y = 0; y < h; y++)
			regions[init_regions_per_row*(y/2+1)-1].pixels[0]->push_back(ns_pixel_coord(w-1,y));
	}	
	//make bottom columns 3x3 if needed
	if (h%2 == 1){
		for (unsigned int x = 0; x < w; x++)
			regions[regions.size()-init_regions_per_row + x/2 -1].pixels[0]->push_back(ns_pixel_coord(x,h-1));
	}

	for (unsigned int i = 0; i < regions.size(); i++){
		for (ns_region_edges_list::iterator p = regions[i].edges.begin();  p != regions[i].edges.end(); p++)
			p->second.calc_weak_pixels(input,weak_edge_threshold);
		regions[i].merged = true;
	}

	cerr << "Starting initial merging\n";
	bool anything_merged = true;
	unsigned int round_number = 1;
	int count = 0;
	while(anything_merged){
		//if (round_number > 0)
		//break;
		anything_merged = false;
		for (unsigned int i = 0; i < regions.size(); i++){
			if (regions[i].deleted)
				continue;
				//regions[i].merged = true;
				//while(regions[i].merged){
					regions[i].merged = false;
					unsigned int edge = 0;
					ns_region_edges_list::iterator p;
					for (p = regions[i].edges.begin();  p != regions[i].edges.end();){
					//	cerr << "Comparing region" << i << " with " << p->first << "\n";
						if (regions[p->first].deleted){
							cerr << "Linked to a dead region.\n";
						}
						unsigned int neighbor_perim = regions[p->first].perimeter_length;
						unsigned int min_perim = regions[i].perimeter_length;
						if (neighbor_perim < regions[i].perimeter_length)
							min_perim = neighbor_perim;
						//if (i == 13+init_regions_per_row*14)
						//	cerr << "p->second.length(): " << p->second.length() << " min_perim: " << min_perim << "\n";
					
						//if ((double)p->second.number_of_weak_pixels / (double)min_perim >= common_edge_threshold1){
						//if ((double)p->second.number_of_weak_pixels / (double)p->second.length() >= common_edge_threshold1){
							if (abs((int)((int)regions[p->first].total_intensity/regions[p->first].area - (int)regions[i].total_intensity/regions[i].area)) < 40){
							regions[i].merge(regions[p->first],regions);
							p = regions[i].edges.begin();
							for (int k = 0; k < (int)edge+1 && p!=regions[i].edges.end(); k++)
								p++;
							regions[i].merged = true;
							anything_merged = true;
							break;
						}
						else p++;
						edge++;
					//}	
				}
			if (count == 1000){
				count = 0;
				cerr << i << "...";
			}
			count++;
		}
		
		unsigned long reg_left = 0;
		for (unsigned int i = 0; i < regions.size(); i++)
			if (!regions[i].deleted)
				reg_left++;

		cout << "Regions left after round " << round_number << " merging: " << reg_left << "\n";
		round_number++;
		
	}

	std::vector<ns_region *> non_zero_regions;
	for (unsigned int i = 0; i < regions.size(); i++)
		if (!regions[i].deleted)
			non_zero_regions.push_back(&regions[i]);

	output.prepare_to_recieve_image(input.properties());

	for (unsigned int i = 0; i < non_zero_regions.size(); i++){
		const char color = (char)((255*i)/non_zero_regions.size());
		for (unsigned int pixel_set = 0; pixel_set < non_zero_regions[i]->pixels.size(); pixel_set++){
			for (unsigned int j = 0; j < non_zero_regions[i]->pixels[pixel_set]->size(); j++){
				ns_pixel_coord c = (*non_zero_regions[i]->pixels[pixel_set])[j];
				if (c.x < w && c.y < h)
					output[c.y][c.x] = color;//non_zero_regions[i]->total_intensity/non_zero_regions[i]->area;
			}
		}

	}


}

bool ns_death_time_solo_posture_annotater::ns_fix_annotation(ns_death_time_annotation & a,ns_analyzed_image_time_path & p){
	if (a.time.period_start == 0 && a.time.period_end == 0)
		return false;
	if (a.time.fully_unbounded()){
		cerr << "Found an unbounded time interval.  Could not fix.\n";
		return false;
	}
	if (a.time.period_start_was_not_observed){
		if (a.time.period_end < p.observation_limits().interval_before_first_observation.period_end){
			cerr << "Identified an unusual first event time, and fixed it.\n";
			a.time = p.observation_limits().interval_before_first_observation;
			return true;
		}
		return false;
	}
	if (a.time.period_end_was_not_observed){
		if (a.time.period_start == p.observation_limits().interval_after_last_observation.period_start)
			return false;
		for (unsigned int i = 0; i < p.element_count(); i++){
			if (p.element(i).absolute_time == a.time.period_start){
				if (i+1 == p.element_count()){
					cerr << "Identified an end-unbounded event time at the start of experiment.  Changing it to the correct interval.\n";
					a.time = p.observation_limits().interval_after_last_observation;
					return true;
				}
				cerr << "Identified an end-unbounded event; changing it to the correct interval.\n";
				a.time.period_start = p.element(i).absolute_time;
				a.time.period_end = p.element(i+1).absolute_time;
				a.time.period_end_was_not_observed = false;
				return true;
			}
		}
	}
	if (a.time.period_start == a.time.period_end){
		for (unsigned int i = 0; i < p.element_count(); i++){
			if (p.element(i).absolute_time == a.time.period_end){
				if (i == 0){
					cerr << "Identified an event time at the start of experiment.  Changing it to the correct interval.\n";
					a.time = p.observation_limits().interval_before_first_observation;
					return true;
				}
				cerr << "Identified an event time; changing it to the correct interval.\n";
				a.time.period_start = p.element(i-1).absolute_time;
				return true;
			}
		}
		cerr << "Could not repair the provided event time, as it occurred at a time during which no observation was made.\n";
		return false;
	}
	if (a.time.period_start > a.time.period_end){
		for (unsigned int i = 0; i < p.element_count(); i++){
			if (p.element(i).absolute_time == a.time.period_start){
				if (i == 0){
					cerr << "Identified a backwards time interval at the start of the experiment.  Changing it to the correct interval.\n";
					a.time = p.observation_limits().interval_before_first_observation;
					return true;
				}
				cerr << "Identified a backwards time interval; changing it to the correct interval.\n";
				a.time.period_end = p.element(i).absolute_time;
				a.time.period_start = p.element(i-1).absolute_time;
				return true;
			}
		}
		cerr << "Could not repair a backwards time interval, as it occurred at a time during which no observation was made.\n";
		return false;
	}
	return false;
}


void ns_death_time_solo_posture_annotater::register_click(const ns_vector_2i & image_position, const ns_click_request & action) {
	ns_acquire_lock_for_scope lock(image_buffer_access_lock, __FILE__, __LINE__);
	const unsigned long hand_bar_group_bottom(bottom_margin_position().y);

	const unsigned long hand_bar_height((current_by_hand_timing_data().animals.size() + 1)*ns_death_time_solo_posture_annotater_timepoint::ns_movement_bar_height);

	ns_vector_2i graph_tl(worm_learner->worm_window.worm_image_size.x, 0);
	ns_vector_2i graph_br = graph_tl + telemetry.image_size();

	const bool click_in_graph_area(image_position.x >= graph_tl.x && image_position.x < graph_br.x &&
		image_position.y >= graph_tl.y && image_position.y < graph_br.y);

		const bool click_in_bar_area(image_position.y >= hand_bar_group_bottom &&
			image_position.y < hand_bar_group_bottom + hand_bar_height);
	bool change_made = false;
	bool click_handled_by_hand_bar_choice(false);
	if (action == ns_cycle_state && (click_in_bar_area || click_in_graph_area )) {
		const unsigned long all_bar_id((image_position.y - hand_bar_group_bottom) / ns_death_time_solo_posture_annotater_timepoint::ns_movement_bar_height);

		if (all_bar_id == 0 || click_in_graph_area) {

			ns_vector_2i bottom_margin_bottom(bottom_margin_position());
			const ns_time_path_limits observation_limit(current_worm->observation_limits());

			bool switch_time(false);
			unsigned long requested_time;
			if (click_in_graph_area) {
				ns_vector_2d graph_position(telemetry.get_graph_value_from_click_position(image_position.x-graph_tl.x, image_position.y- graph_tl.y));
				cerr << graph_position << "\n";
				if (graph_position.x >= 0) {
					switch_time = true;
					requested_time = telemetry.get_graph_time_from_graph_position(graph_position.x);
				}
			}
			else {
				switch_time = true;
				requested_time = current_machine_timing_data->animals[0].get_time_from_movement_diagram_position(image_position.x, bottom_margin_bottom,
					ns_vector_2i(current_worm->element(current_element_id()).image().properties().width, 0),
					observation_limit);
			}
			if (switch_time) {
				clear_cached_images(false);
				set_current_timepoint(requested_time, false);
				{
					ns_image_standard temp_buffer;
					timepoints[current_timepoint_id].load_image(1024, current_image, sql(), temp_buffer,1);
				}
				change_made = true;
			}
			click_handled_by_hand_bar_choice = true;
		}
		else {
			const unsigned long hand_bar_id = all_bar_id - 1;
			if (hand_bar_id > current_by_hand_timing_data().animals.size())
				throw ns_ex("Invalid hand bar");
			if (hand_bar_id != current_animal_id) {
				current_animal_id = hand_bar_id;
				change_made = true;
				click_handled_by_hand_bar_choice = true;
			}
		}
	}
	if (!click_handled_by_hand_bar_choice) {
		switch (action) {
		case ns_cycle_state:
		case ns_cycle_state_alt_key_held:
			current_by_hand_timing_data().animals[current_animal_id].step_event(
			ns_death_timing_data_step_event_specification(
				current_time_interval(), current_worm->element(current_element_id()),
				properties_for_all_animals.region_info_id, properties_for_all_animals.stationary_path_id, current_animal_id), current_worm->observation_limits(),action== ns_cycle_state_alt_key_held);
			change_made = true;
			break;
		case ns_cycle_flags:
			step_error_label(properties_for_all_animals);
			change_made = true;
			break;
		case ns_annotate_extra_worm:
		{
			unsigned long & current_annotated_worm_count(properties_for_all_animals.number_of_worms_at_location_marked_by_hand);

			if (current_by_hand_timing_data().animals.size() >= ns_death_time_annotation::maximum_number_of_worms_at_position) {
				current_by_hand_timing_data().animals.resize(1);
				current_annotated_worm_count = 1;
				current_animal_id = 0;
			}
			else {
				const unsigned long new_animal_id(current_by_hand_timing_data().animals.size());
				current_by_hand_timing_data().animals.resize(new_animal_id + 1);
				current_by_hand_timing_data().animals.rbegin()->set_fast_movement_cessation_time(ns_death_timing_data_step_event_specification(
					ns_death_timing_data_step_event_specification(
						current_time_interval(),
						current_worm->element(current_element_id()),
						properties_for_all_animals.region_info_id,
						properties_for_all_animals.stationary_path_id, new_animal_id)));
				current_by_hand_timing_data().animals.rbegin()->animal_specific_sticky_properties.animal_id_at_position = new_animal_id;
				//add a "object has stopped fast moving" event at first timepoint of new path
				current_by_hand_timing_data().animals.rbegin()->step_event(
					ns_death_timing_data_step_event_specification(
						current_time_interval(), current_worm->element(current_element_id()),
						properties_for_all_animals.region_info_id, properties_for_all_animals.stationary_path_id, new_animal_id), current_worm->observation_limits(),false);
				this->current_animal_id = new_animal_id;
				unsigned long new_sticky_label = current_by_hand_timing_data().animals.size();
				if (current_annotated_worm_count > new_sticky_label)
					new_sticky_label = current_annotated_worm_count;
				properties_for_all_animals.number_of_worms_at_location_marked_by_hand = new_sticky_label;

			}
			//current_by_hand_timing_data->animals[current_animal_id].annotate_extra_worm(); 
			change_made = true;
			break;
		}
		case ns_increase_contrast:
			worm_learner->worm_window.dynamic_range_rescale_factor += .1;
			change_made = true;
			break;
		case ns_decrease_contrast: {
			worm_learner->worm_window.dynamic_range_rescale_factor -= .1;
			if (worm_learner->worm_window.dynamic_range_rescale_factor < .1)
				worm_learner->worm_window.dynamic_range_rescale_factor = .1;
			change_made = true;
			break;
		}

		case ns_output_images: {
			bool in_char(false);
			const string pn(current_region_data->metadata.plate_name());
			std::string plate_name;
			for (unsigned int i = 0; i < pn.size(); i++) {
				if (pn[i] == ':') {
					if (in_char)
						continue;
					else {
						plate_name += "_";
						in_char = true;
						continue;
					}
				}
				plate_name += pn[i];
				in_char = false;
			}
			const string filename(this->current_region_data->metadata.experiment_name + "=" + plate_name + "=" + ns_to_string(properties_for_all_animals.stationary_path_id.group_id));
			if (filename == "")
				break;

			ns_file_chooser d;
			d.dialog_type = Fl_Native_File_Chooser::BROWSE_DIRECTORY;
			d.default_filename = "";
			d.title = "Choose Movement Quantification Output Directory";
			d.act();
			//	ns_run_in_main_thread<ns_file_chooser> run_mt(&d);
			if (d.chosen)
				output_worm_frames(d.result, filename, sql());
			break;
		}
		default: throw ns_ex("ns_death_time_posture_annotater::Unknown click type");
		}
	}
	if (change_made) {
		update_events_to_storyboard();
		lock.release();
		draw_metadata(&timepoints[current_timepoint_id], *current_image.im);
		request_refresh();
	}
	else
		lock.release();


}

void ns_experiment_storyboard_annotater::register_click(const ns_vector_2i & image_position, const ns_click_request & action) {
	if (divisions[current_timepoint_id].division->events.size() == 0)
		return;
	ns_acquire_lock_for_scope lock(image_buffer_access_lock, __FILE__, __LINE__);
	if (action == ns_censor_all) {
		bool new_state = false;
		for (unsigned int i = 0; i < divisions[current_timepoint_id].division->events.size(); i++) {
			if (divisions[current_timepoint_id].division->events[i].event_annotation.flag.specified())
				continue;
			if (!divisions[current_timepoint_id].division->events[i].event_annotation.is_excluded()) {
				new_state = true;
				break;
			}
		}
		for (unsigned int i = 0; i < divisions[current_timepoint_id].division->events.size(); i++) {
			if (divisions[current_timepoint_id].division->events[i].event_annotation.flag.specified())
				continue;
			divisions[current_timepoint_id].division->events[i].event_annotation.excluded = new_state ? ns_death_time_annotation::ns_by_hand_excluded : ns_death_time_annotation::ns_not_excluded;
		}

		for (unsigned int i = 0; i < divisions.size(); i++) {
			if (i == current_timepoint_id)
				continue;
			for (unsigned int j = 0; j < divisions[i].division->events.size(); j++) {
				for (unsigned int k = 0; k < divisions[current_timepoint_id].division->events.size(); k++) {
					if (divisions[current_timepoint_id].division->events[k].event_annotation.flag.specified())
						continue;
					if (divisions[i].division->events[j].event_annotation.stationary_path_id == divisions[current_timepoint_id].division->events[k].event_annotation.stationary_path_id &&
						divisions[i].division->events[j].event_annotation.region_info_id == divisions[current_timepoint_id].division->events[k].event_annotation.region_info_id)
						divisions[i].division->events[j].event_annotation.excluded = new_state ? ns_death_time_annotation::ns_by_hand_excluded : ns_death_time_annotation::ns_not_excluded;
				}
			}
		}
	}
	else if (action == ns_increase_contrast)
		worm_learner->main_window.dynamic_range_rescale_factor += .1;
	else if (action == ns_decrease_contrast) {
		worm_learner->main_window.dynamic_range_rescale_factor -= .1;
		if (worm_learner->main_window.dynamic_range_rescale_factor < .1)
			worm_learner->main_window.dynamic_range_rescale_factor = .1;
	}
	else {
		ns_experiment_storyboard_timepoint_element * worm(divisions[current_timepoint_id].get_worm_at_visualization_position(image_position));
		if (worm == 0) return;
		if (action == ns_load_worm_details) {
			ns_launch_worm_window_for_worm(worm->event_annotation.region_info_id, worm->event_annotation.stationary_path_id, worm->storyboard_absolute_time);
			return;
		}
		std::vector<ns_experiment_storyboard_timepoint_element *> worms(1, worm);
		for (unsigned int i = 0; i < divisions.size(); i++) {
			if (i == current_timepoint_id)
				continue;
			for (unsigned int j = 0; j < divisions[i].division->events.size(); j++) {
				if (divisions[i].division->events[j].event_annotation.stationary_path_id == worm->event_annotation.stationary_path_id &&
					divisions[i].division->events[j].event_annotation.region_info_id == worm->event_annotation.region_info_id)
					worms.push_back(&divisions[i].division->events[j]);
			}
		}
		//	if (worms.size() > 1)
		//		cerr << "More than one record found for worm in storyboard.\n";
		for (unsigned int i = 0; i < worms.size(); i++) {
			switch (action) {
			case ns_cycle_state:
			case ns_censor:
				if (worms[i]->event_annotation.excluded == ns_death_time_annotation::ns_not_excluded &&
					worms[i]->event_annotation.flag.label_short.empty()) {
					worms[i]->event_annotation.excluded = ns_death_time_annotation::ns_by_hand_excluded;
				}
				else if (worms[i]->event_annotation.excluded == ns_death_time_annotation::ns_by_hand_excluded) {
					worms[i]->event_annotation.flag = ns_death_time_annotation_flag::extra_worm_from_multiworm_disambiguation();
					worms[i]->event_annotation.excluded = ns_death_time_annotation::ns_not_excluded;
				}
				else {
					worms[i]->event_annotation.excluded = ns_death_time_annotation::ns_not_excluded;
					worms[i]->event_annotation.flag = ns_death_time_annotation_flag::none();
				}
				//w->annotation.excluded = w->annotation.is_excluded()?ns_death_time_annotation::ns_not_excluded: 
				//	w->annotation.flag = ns_death_time_annotation_flag::none();
				break;
			case ns_annotate_extra_worm:
				if (worms[i]->event_annotation.number_of_worms_at_location_marked_by_hand == 0)
					worms[i]->event_annotation.number_of_worms_at_location_marked_by_hand = 2;  //skip an explicit 1 worm; have that be the last option
				else if (worms[i]->event_annotation.number_of_worms_at_location_marked_by_hand == ns_death_time_annotation::maximum_number_of_worms_at_position)
					worms[i]->event_annotation.number_of_worms_at_location_marked_by_hand = 1;
				else if (worms[i]->event_annotation.number_of_worms_at_location_marked_by_hand == 1)
					worms[i]->event_annotation.number_of_worms_at_location_marked_by_hand = 0;  //skip an explicit 1 worm
				else worms[i]->event_annotation.number_of_worms_at_location_marked_by_hand++;
				break;

			default: throw ns_ex("ns_death_time_posture_annotater::Unknown click type");
			}
		}
	}
	saved_ = false;
	draw_metadata(&divisions[current_timepoint_id], *current_image.im);
	request_refresh();

	lock.release();
}
