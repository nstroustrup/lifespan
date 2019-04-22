#include "ns_image_statistics.h"
#include <deque>
using namespace std;
#include "ns_image_server.h"
#include "ns_buffered_random_access_image.h"
using namespace std;

void ns_histogram_sql_converter::insert_histogram(const ns_histogram_type & histogram,ns_image_server_sql * sql){
	char h[4*128];
	for (unsigned int i = 0; i < 128; i++){
		unsigned long s(histogram[2*i]+histogram[2*i+1]);
		h[4*i]   = static_cast<char>(s     & 0x000000FF);
		h[4*i+1] = static_cast<char>(s>>8  & 0x000000FF);
		h[4*i+2] = static_cast<char>(s>>16 & 0x000000FF);
		h[4*i+3] = static_cast<char>(s>>24 & 0x000000FF);
	}
	sql->write_data(&(h[0]),4*128);
}
void ns_histogram_sql_converter::extract_histogram(const string & data, std::vector<unsigned long> & histogram){
	if (data.size() != 4*128) throw ns_ex("Improper histogram data size");
	histogram.resize(8*256);
	for (unsigned int i = 0; i < 128; i++){
		histogram[2*i] 	 =
		histogram[2*i+1] = 	(data[4*i] |
							data[4*i+1] << 8 |
							data[4*i+2] << 16 |
							data[4*i+3] << 24)/2;
	}
}

void ns_image_statistics::calculate_statistics_from_histogram(){
	double total_area(size.x*size.y);
	image_statistics.entropy = 0;
	ns_64_bit av_i(0);
	ns_s64_bit var_i(0);
	for (unsigned int i = 0; i < 256; i++){
		double p(histogram[i]/total_area);
		if (p > 0) image_statistics.entropy -= p*log(p);
		av_i += ((ns_64_bit)i)*histogram[i];
	}
	image_statistics.mean = av_i/total_area;

	for (unsigned int i = 0; i < 256; i++){
		ns_s64_bit tmp(i);
		tmp-=(ns_s64_bit)image_statistics.mean;
		var_i+=((ns_s64_bit)histogram[i])*tmp*tmp;
	}
	image_statistics.variance = var_i/total_area;

	unsigned long percentile((unsigned long)(total_area/100));
	ns_64_bit c(0),c_sum(0);
	for (unsigned int i = 0; i < 256; i++){
		if (c+histogram[i] >= percentile){
			c_sum += ((ns_64_bit)i)*(ns_64_bit)(percentile - c);
			break;
		}
		c_sum+=((ns_64_bit)i)*(ns_64_bit)histogram[i];
		c+=histogram[i];
	}
	image_statistics.bottom_percentile_average = c_sum/(double)percentile;

	c = 0;
	c_sum = 0;
	for (int i = 255; i >= 0; i--){
		if (c+histogram[i] >= percentile){
			c_sum += ((ns_64_bit)i)*(ns_64_bit)(percentile - c);
			break;
		}
		c_sum+=((ns_64_bit)i)*(ns_64_bit)histogram[i];
		c+=histogram[i];
	}
	image_statistics.top_percentile_average = c_sum/(double)percentile;
}

bool ns_image_statistics::submit_to_db(ns_64_bit & id, ns_image_server_sql * sql,bool include_image_stats,bool include_worm_stats){
	bool made_new_db_entry=false;
	if (id != 0){
		//check to see that the record does exist.  If not, we'll make a new one.
		*sql << "SELECT id FROM " << sql->table_prefix() << "image_statistics WHERE id = " << id;
		ns_sql_result res;
		sql->get_rows(res);
		if (res.size() == 0)
			id = 0;
	}
	if (id == 0){
		*sql << "INSERT INTO " << sql->table_prefix() << "image_statistics SET ";
		made_new_db_entry = true;
	}
	else{
		*sql << "UPDATE " << sql->table_prefix() << "image_statistics SET ";
	}
	if (include_image_stats){
		*sql << "size_x='"<<size.x << "',size_y='" << size.y << "',intensity_average='" << image_statistics.mean << "', intensity_entropy='" << image_statistics.entropy << "', intensity_std='"<<sqrt(image_statistics.variance) << "',"
			<< "intensity_top_percentile='" << image_statistics.top_percentile_average << "', intensity_bottom_percentile='" << image_statistics.bottom_percentile_average << "', "
			<< "histogram='";
		
		ns_histogram_sql_converter::insert_histogram(histogram,sql);
		*sql << "' ";
		if (include_worm_stats) *sql << ",";
	}
	if (include_worm_stats){
		*sql << "worm_count='"<<worm_statistics.count<<"',worm_area_mean='"<<worm_statistics.area_mean<<"',worm_area_variance='"<<worm_statistics.area_variance<<"',"
		    << "worm_length_mean='"<<worm_statistics.length_mean<<"',worm_length_variance='"<<worm_statistics.length_variance<<"',worm_width_mean='"<<worm_statistics.width_mean<<"',"
			<< "worm_width_variance='"<<worm_statistics.width_variance<<"',worm_intensity_mean='"<<worm_statistics.absolute_intensity.mean<< "',worm_intensity_variance='"<<worm_statistics.absolute_intensity.variance<<"',"
			<< "non_worm_object_count='"<<non_worm_statistics.count<<"',"
			<< "non_worm_object_area_mean='"<<non_worm_statistics.area_mean<<"',non_worm_object_area_variance='"<<non_worm_statistics.area_variance<<"',"
			<< "non_worm_object_intensity_mean='"<<non_worm_statistics.absolute_intensity.mean<<"',non_worm_object_intensity_variance='"<<non_worm_statistics.absolute_intensity.variance<<"' ";
		if (!include_image_stats) *sql << ", histogram=''"; // histogram is a NOT NULL column with no default value so we have to provide an empty one.
	}
	if (id == 0)
		db_id = id = sql->send_query_get_id();
	else{
		*sql << "WHERE id = " << id;
		sql->send_query();
		db_id = id;
	}
	return made_new_db_entry;
}

void ns_image_statistics::calculate_statistics_from_image(ns_image_server_image & im,ns_sql & sql){
	if (im.filename.size() == 0 || im.path.size() == 0)
		im.load_from_db(im.id,&sql);
	ns_image_storage_source_handle<ns_8_bit> h(image_server.image_storage.request_from_storage(im,&sql));
	ns_image_buffered_random_access_input_image<ns_8_bit, ns_image_storage_source<ns_8_bit> > in(1024);
	in.assign_buffer_source(h.input_stream());
	size.x = in.properties().width;
	size.y = in.properties().height;
	for (int y = 0; y < size.y; y++){
		for (int x = 0; x < size.x; x++){
				histogram[in[y][x]]++;
		}
	}
	calculate_statistics_from_histogram();
}

std::string ns_image_statistics::produce_sql_query_stub(){
	return "image_statistics.size_x,image_statistics.size_y,image_statistics.intensity_entropy,"
		   "image_statistics.intensity_average,image_statistics.intensity_std,image_statistics.intensity_bottom_percentile,image_statistics.intensity_top_percentile,"
		   "worm_count,worm_area_mean,worm_area_variance,worm_length_mean,worm_length_variance,worm_width_mean,worm_width_variance,worm_intensity_mean,worm_intensity_variance,non_worm_object_count,"
		   "non_worm_object_area_mean,non_worm_object_area_variance,non_worm_object_intensity_mean,non_worm_object_intensity_variance ";

}
unsigned long ns_image_statistics::sql_query_stub_field_count(){
	return 21;
}

void ns_image_statistics::from_sql_result(const ns_sql_result_row & res){
	if (res.size() < sql_query_stub_field_count()) throw ns_ex("Invalid query result size: ") << res.size();
	size.x = atol(res[0].c_str());
	size.y = atol(res[1].c_str());
	image_statistics.entropy = atof(res[2].c_str());
	image_statistics.mean= atof(res[3].c_str());
	image_statistics.variance = pow(atof(res[4].c_str()),2);
	image_statistics.bottom_percentile_average = atof(res[5].c_str());
	image_statistics.top_percentile_average = atof(res[6].c_str());
	worm_statistics.count= atol(res[7].c_str());
	worm_statistics.area_mean= atof(res[8].c_str());
	worm_statistics.area_variance= atof(res[9].c_str());
	worm_statistics.length_mean= atof(res[10].c_str());
	worm_statistics.length_variance= atof(res[11].c_str());
	worm_statistics.width_mean= atof(res[12].c_str());
	worm_statistics.width_variance= atof(res[13].c_str());
	worm_statistics.absolute_intensity.mean= atof(res[14].c_str());
	worm_statistics.absolute_intensity.variance= atof(res[15].c_str());
	non_worm_statistics.count= atol(res[16].c_str());
	non_worm_statistics.area_mean= atof(res[17].c_str());
	non_worm_statistics.area_variance= atof(res[18].c_str());
	non_worm_statistics.absolute_intensity.mean= atof(res[19].c_str());
	non_worm_statistics.absolute_intensity.variance= atof(res[20].c_str());
}

//adapted from https://people.cs.uct.ac.za/~ksmith/articles/sliding_window_minimum.html#sliding-window-minimum-algorithm
void ns_sliding_window_min(const std::vector<double> & data, int half_K, std::vector<double> & output) {
	const int K(half_K + half_K + 1);
	output.resize(data.size());
	std::deque< std::pair<double, int> > window;
	for (int i = 0; i < half_K; i++) {
		while (!window.empty() && window.back().first >= data[i])
			window.pop_back();
		window.push_back(std::make_pair(data[i], i));
	}

	for (int i = half_K; i < data.size(); i++) {
		while (!window.empty() && window.back().first >= data[i])
			window.pop_back();
		window.push_back(std::make_pair(data[i], i));

		while (window.front().second <= i - K)
			window.pop_front();
		output[i - half_K] = window.front().first;
	}
	for (int i = data.size(); i < data.size() + half_K; i++) {
		while (window.front().second <= i - K)
			window.pop_front();
		output[i - half_K] = window.front().first;
	}
}
void ns_sliding_window_max(const std::vector<double> & data, int half_K, std::vector<double> & output) {
	const int K(half_K + half_K + 1);
	output.resize(data.size());
	std::deque< std::pair<double, int> > window;
	for (int i = 0; i < half_K; i++) {
		while (!window.empty() && window.back().first <= data[i])
			window.pop_back();
		window.push_back(std::make_pair(data[i], i));
	}
	for (int i = half_K; i < data.size(); i++) {
		while (!window.empty() && window.back().first <= data[i])
			window.pop_back();
		window.push_back(std::make_pair(data[i], i));

		while (window.front().second <= i - K)
			window.pop_front();
		output[i] = window.front().first;
	}
	for (int i = data.size(); i < data.size() + half_K; i++) {
		while (window.front().second <= i - K)
			window.pop_front();
		output[i - half_K] = window.front().first;
	}
}
