#include "ns_worm_detection_constants.h"
#include "ns_detected_worm_info.h"
#include "ns_font.h"
#include "ns_graph.h"
#include "ns_spine_drawer.h"
#include "ns_progress_reporter.h"

#include "ns_image_statistics.h"

using namespace std;

#ifdef NS_RETHRESHOLD_TRAINING_SET
#include "ns_difference_thresholder.h"
#endif
#ifdef NS_USE_BSPLINE
#include "ns_bspline.h"
#endif

#undef NS_DRAW_THRESHOLD_ON_SPINE
#define NS_RECALULCATE_SUBSPINE_BITMAPS
#undef NS_ZHANG_THINNING_ON_SPINE

char ns_sign(const double & t){
	if (t > 0)
		return 1;
	if (t < 0)
		return -1;
	return 0;
}

bool operator<(const ns_packed_collage_position & l, const ns_packed_collage_position & r){
	if (l.pos.y != r.pos.y)
		return l.pos.y < r.pos.y;
	return l.pos.x < r.pos.x;
}



class ns_erosion_intensity_profiler{
public:
	void get_profile(const unsigned long step_size,const ns_image_standard & image, vector<double> & steps){
		ns_image_standard im;
		image.pump(im,1024);
		im_tmp.prepare_to_recieve_image(image.properties());
		steps.resize(0);
		unsigned long k(0);
		while(true){
			current_round_sum = 0;
			current_round_N = 0;
			for (unsigned int i = 0; i < step_size; i++){
				//ns_save_image(string("c:\\c_") + ns_to_string(step_size) + "_" + ns_to_string(k) + "_f.tif",im);
				erode_and_characterize(im);
				k++;
			}
			if (current_round_N == 0)
				break;
			vector<double>::size_type s(steps.size());
			steps.resize(s+1);
			steps[s] = current_round_sum/(double)current_round_N;
		}
	}
private:
	void erode_and_characterize(ns_image_standard & im){
		
		//calculate statistics for the current edge
		for (unsigned int y = 0; y < im.properties().height; y++){
			for (unsigned int x = 0; x < im.properties().width; x++){
				if (im[y][x] != 0 && 
					(y == 0 || y+1  == im.properties().height ||
					 x == 0 || x+1  == im.properties().width ||
					 im[y-1][x  ] == 0 || im[y+1][x  ] == 0 || 
					 im[y  ][x-1] == 0 || im[y  ][x+1] == 0 || 
					 im[y-1][x-1] == 0 || im[y+1][x+1] == 0)){
						current_round_sum+=im[y][x];
						current_round_N++;
						im_tmp[y][x] = 0;
					}
				else im_tmp[y][x] = 1;
			}
		}
		for (unsigned int y = 0; y < im.properties().height; y++){
			for (unsigned int x = 0; x < im.properties().width; x++){
				if (!im_tmp[y][x])
					im[y][x] = 0;
			}
		}
	}
	ns_image_standard im_tmp;
	unsigned long current_round_sum;
	unsigned long current_round_N;
};


ns_detected_worm_stats ns_detected_worm_info::generate_stats() const{
	const float resolution(bitmap().properties().resolution);
	const double percentage_distance_ends(0.1);
	
	ns_detected_worm_stats stats;
	if (worm_shape.nodes.size() == 0 || worm_shape.width.size() == 0){
		for (unsigned int i = 0; i < stats.size(); i++)
			stats[i] = -1;
		stats.not_a_worm = true;
		return stats;
	}
	//bounding box geometry
	stats[ns_stat_bitmap_width] = (double)region_size.x;
	stats[ns_stat_bitmap_height] = (double)region_size.y;
	stats[ns_stat_bitmap_diagonal] = sqrt((double)region_size.x*region_size.x + (double)region_size.y*region_size.y);
	//stats[ns_stat_number_of_spine_nodes] = (unsigned int)worm_shape.nodes.size();
	stats[ns_stat_spine_length] = worm_shape.length;

	//width information
	double d(0);
	int end_left(0),
		end_right(worm_shape.nodes.size()-1);

	for (int i = 0; i < (int)worm_shape.nodes.size()-1; i++){
		if (d >= percentage_distance_ends*worm_shape.length){
			end_left = i;
			break;
		}
		d+=(worm_shape.nodes[i+1]-worm_shape.nodes[i]).mag();
	}
	d = 0;
	for (int i = (int)worm_shape.nodes.size()-1; i > 0; i--){
		if (d >= percentage_distance_ends*worm_shape.length){
			end_right = i;
			break;
		}
		d+=(worm_shape.nodes[i-1]-worm_shape.nodes[i]).mag();
	}


	stats[ns_stat_average_width] = 0;
	stats[ns_stat_max_width] = 0;
	std::vector<double> sorted_widths;
	sorted_widths.reserve(worm_shape.width.size());
	for (int i = end_left; i < end_right; i++){
		//	stats[ns_stat_average_width]+=worm_shape.width[i];
			sorted_widths.push_back(worm_shape.width[i]);
	}
	std::sort(sorted_widths.begin(),sorted_widths.end());
	
	if ( sorted_widths.size() > 6 ){
		stats[ns_stat_average_width] = sorted_widths[sorted_widths.size()/2];
		stats[ns_stat_max_width] = sorted_widths[sorted_widths.size()-3];
		stats[ns_stat_min_width] = sorted_widths[3];
	}

	stats[ns_stat_width_variance] = 0;
	for (int i = end_left; i < end_right; i++)
		stats[ns_stat_width_variance]+=(worm_shape.width[i]-stats[ns_stat_average_width])*(worm_shape.width[i]-stats[ns_stat_average_width]);
		if ( end_left < end_right)
		 stats[ns_stat_width_variance] /= ( end_right - end_left);
	else stats[ns_stat_width_variance] = 0;

	stats[ns_stat_spine_length_to_bitmap_width_ratio] = stats[ns_stat_spine_length] / stats[ns_stat_bitmap_width];
	stats[ns_stat_spine_length_to_bitmap_height_ratio]  = stats[ns_stat_spine_length] / stats[ns_stat_bitmap_height];
	stats[ns_stat_spine_length_to_bitmap_diagonal_ratio] = stats[ns_stat_spine_length] / stats[ns_stat_bitmap_diagonal];
	
	//widths at end information
	int width_half_interval = (int)worm_shape.nodes.size()/20;
	if (width_half_interval > 0){
		stats[ns_stat_width_at_center] = 0;
		for (int i = (int)worm_shape.width.size()/2 - width_half_interval; i <= (int)worm_shape.width.size()/2 + width_half_interval; i++)
			stats[ns_stat_width_at_center]+=worm_shape.width[i];
		stats[ns_stat_width_at_center]/=(2*width_half_interval);
	}
	else if (worm_shape.nodes.size() > 0)
		stats[ns_stat_width_at_center] = worm_shape.width[worm_shape.width.size()/2];
	else stats[ns_stat_width_at_center] = 0;

	if (stats[ns_stat_max_width] != 0)
		stats[ns_stat_spine_length_to_max_width_ratio] = stats[ns_stat_spine_length]/stats[ns_stat_max_width];
	else stats[ns_stat_spine_length_to_max_width_ratio] = 0;
	if (stats[ns_stat_average_width] != 0)
		stats[ns_stat_spine_length_to_average_width] = stats[ns_stat_spine_length]/stats[ns_stat_average_width];
	else stats[ns_stat_spine_length_to_average_width] = 0;

	if (end_right - end_left > 0){
		stats[ns_stat_width_at_end_0] = worm_shape.width[end_left];
		stats[ns_stat_width_at_end_1] = worm_shape.width[end_right];

		if (stats[ns_stat_width_at_center] != 0){
			stats[ns_stat_end_width_to_middle_width_ratio_0] = stats[ns_stat_width_at_end_0]/stats[ns_stat_width_at_center];
			stats[ns_stat_end_width_to_middle_width_ratio_1] = stats[ns_stat_width_at_end_1]/stats[ns_stat_width_at_center];
		}
		else{
			stats[ns_stat_end_width_to_middle_width_ratio_0] = 0;
			stats[ns_stat_end_width_to_middle_width_ratio_1] = 0;
		}
		if (stats[ns_stat_width_at_end_1] != 0)
			stats[ns_stat_end_width_ratio] = stats[ns_stat_width_at_end_0]/stats[ns_stat_width_at_end_1];
		else stats[ns_stat_end_width_ratio] = 0;
	}
	else{
		stats[ns_stat_width_at_end_0] = 0; 
		stats[ns_stat_width_at_end_1] = 0;

		stats[ns_stat_end_width_to_middle_width_ratio_0] = 0;
		stats[ns_stat_end_width_to_middle_width_ratio_1] = 0; 
		stats[ns_stat_end_width_ratio] = 0;
	}
	//curvature
	if (worm_shape.nodes.size() > 3){
		ns_curvature_stats c_stats;
		ns_calculate_curvature_stats(worm_shape.curvature,c_stats);

		stats[ns_stat_average_curvature]= 10*c_stats.displacement_average;
		stats[ns_stat_total_curvature]=10*c_stats.displacement_total;
		stats[ns_stat_curvature_variance]=1000*c_stats.variance;
		stats[ns_stat_max_curvature]=10*c_stats.maximum;
		stats[ns_stat_curvature_x_intercept] = c_stats.x_intercept_count;
	
	}
	else{
		stats[ns_stat_average_curvature] = 0;
		stats[ns_stat_total_curvature] = 0;
		stats[ns_stat_curvature_variance] = 0;
		stats[ns_stat_max_curvature] = 0;
	}

	//distance between ends
	stats[ns_stat_distance_between_ends] = (worm_shape.nodes[0] - *worm_shape.nodes.rbegin()).mag();


	//average intensity
	stats[ns_stat_absolute_intensity_average] = 0;
	stats[ns_stat_absolute_intensity_max] = 0;
	stats[ns_stat_relative_intensity_average] = 0;
	stats[ns_stat_relative_intensity_max] = 0;
	stats[ns_stat_pixel_area]= 0;
	stats[ns_stat_edge_length] = 0;
	ns_histogram<unsigned long long,ns_8_bit> absolute_histogram,
											  relative_histogram;
	
	for (unsigned int y = 0; y < _bitmap->properties().height; y++){
		for (unsigned int x = 0; x < _bitmap->properties().width; x++){
			if ( (*_bitmap )[y][x] ){
				absolute_histogram.increment(( *_absolute_grayscale )[y][x]);
				relative_histogram.increment(( *_relative_grayscale )[y][x]);
				stats[ns_stat_pixel_area]++;
			}
			if ((*_edge_bitmap )[y][x])
				stats[ns_stat_edge_length]++;
		}
	}
	if (stats[ns_stat_pixel_area] != 0)
		stats[ns_stat_edge_to_area_ratio] = stats[ns_stat_edge_length]/stats[ns_stat_pixel_area];
	else stats[ns_stat_edge_to_area_ratio] = 0;
	
	stats[ns_stat_absolute_intensity_roughness_2] = 0;
	stats[ns_stat_relative_intensity_roughness_2] = 0;
	long dd(3),dd_area(0);
	for (int y = dd; y < (long)_bitmap->properties().height-dd; y++){
		for (int x = dd; x < (long)_bitmap->properties().width-dd; x++){
			if ((*_bitmap)[y][x]){
				dd_area++;
				stats[ns_stat_relative_intensity_roughness_2]+=4*(*_relative_grayscale)[y][x] - (*_relative_grayscale)[y-dd][x] - (*_relative_grayscale)[y+dd][x] - (*_relative_grayscale)[y][x-dd]- (*_relative_grayscale)[y][x+dd];
				stats[ns_stat_absolute_intensity_roughness_2]+=4*(*_absolute_grayscale)[y][x] - (*_absolute_grayscale)[y-dd][x] - (*_absolute_grayscale)[y+dd][x] - (*_absolute_grayscale)[y][x-dd]- (*_absolute_grayscale)[y][x+dd];
			}
		}
	}
	if (dd_area > 0){
		stats[ns_stat_absolute_intensity_roughness_2]/=dd_area;
		stats[ns_stat_relative_intensity_roughness_2]/=dd_area;
	}
	
	stats[ns_stat_absolute_intensity_variance] = 0;
	stats[ns_stat_absolute_intensity_skew] = 0;
	stats[ns_stat_absolute_intensity_dark_pixel_area]= 0;
	stats[ns_stat_absolute_intensity_dark_pixel_average]= 0;
	stats[ns_stat_absolute_intensity_roughness_1] = 0;
	stats[ns_stat_relative_intensity_variance] = 0;
	stats[ns_stat_relative_intensity_skew] = 0;
	stats[ns_stat_relative_intensity_dark_pixel_area]= 0;
	stats[ns_stat_relative_intensity_dark_pixel_average]= 0;
	stats[ns_stat_relative_intensity_roughness_1] = 0;
	if (stats[ns_stat_pixel_area] != 0){
		stats[ns_stat_absolute_intensity_average] = absolute_histogram.mean();
		stats[ns_stat_relative_intensity_average] = relative_histogram.mean();
		stats[ns_stat_absolute_intensity_variance] = absolute_histogram.variance(stats[ns_stat_absolute_intensity_average]);
		stats[ns_stat_relative_intensity_variance] = relative_histogram.variance(stats[ns_stat_relative_intensity_average]);
		stats[ns_stat_absolute_intensity_skew] = absolute_histogram.skewness(stats[ns_stat_absolute_intensity_average],stats[ns_stat_absolute_intensity_variance]);
		stats[ns_stat_relative_intensity_skew] = relative_histogram.skewness(stats[ns_stat_relative_intensity_average],stats[ns_stat_relative_intensity_variance]);

		stats[ns_stat_absolute_intensity_roughness_1] = absolute_histogram.entropy();
		stats[ns_stat_relative_intensity_roughness_1] = relative_histogram.entropy();

		stats[ns_stat_spine_length_to_area] = stats[ns_stat_spine_length]/stats[ns_stat_pixel_area];

		//calculate darkest 20%
		stats[ns_stat_absolute_intensity_dark_pixel_average] = absolute_histogram.average_of_ntile(4,5);  //"darkest" pixels in absolute raw images are bright
		stats[ns_stat_relative_intensity_dark_pixel_average] = relative_histogram.average_of_ntile(0,5);	

		//calculate brightest 20%
		stats[ns_stat_absolute_intensity_max] = absolute_histogram.average_of_ntile(0,5);
		stats[ns_stat_relative_intensity_max] = relative_histogram.average_of_ntile(4,5);

		//normalize dark area
		stats[ns_stat_absolute_intensity_dark_pixel_area]/=stats[ns_stat_pixel_area];
		stats[ns_stat_relative_intensity_dark_pixel_area]/=stats[ns_stat_pixel_area];
	
		//calculate brightest 2%
		stats[ns_stat_absolute_intensity_max] = absolute_histogram.average_of_ntile(0,50);
		stats[ns_stat_relative_intensity_max] = absolute_histogram.average_of_ntile(49,50);

	}

	ns_erosion_intensity_profiler profiler;
	std::vector<double> intensity_profile;
	profiler.get_profile(1,relative_grayscale(),intensity_profile);

	stats[ns_stat_intensity_profile_edge] = 0;
	stats[ns_stat_intensity_profile_center]	 = 0;
	stats[ns_stat_intensity_profile_max]	 = 0;
	stats[ns_stat_intensity_profile_variance] = 0;
	if (intensity_profile.size() > 2){
		stats[ns_stat_intensity_profile_edge] = intensity_profile[1];
		stats[ns_stat_intensity_profile_center] = *intensity_profile.rbegin();
		for (unsigned int i = 1; i < intensity_profile.size(); i++){
			if (stats[ns_stat_intensity_profile_max] < intensity_profile[i])
				stats[ns_stat_intensity_profile_max] = intensity_profile[i];
			stats[ns_stat_intensity_profile_variance] += (intensity_profile[i]-stats[ns_stat_relative_intensity_average])*
														 (intensity_profile[i]-stats[ns_stat_relative_intensity_average]);
		}
		stats[ns_stat_intensity_profile_variance]/=(intensity_profile.size()-1);
	}


		
	stats[ns_stat_absolute_intensity_containing_image_region_average] = whole_image_stats.whole_image_region_stats.absolute_intensity_stats.average_intensity;
	stats[ns_stat_relative_intensity_containing_image_region_average] = whole_image_stats.whole_image_region_stats.relative_intensity_stats.average_intensity;

	stats[ns_stat_absolute_intensity_normalized_average] = 0;
	stats[ns_stat_relative_intensity_normalized_average] = 0;
	if (whole_image_stats.whole_image_region_stats.absolute_intensity_stats.average_intensity != 0)
		stats[ns_stat_absolute_intensity_normalized_average] = stats[ns_stat_absolute_intensity_average] / whole_image_stats.whole_image_region_stats.absolute_intensity_stats.average_intensity;
	if (whole_image_stats.whole_image_region_stats.relative_intensity_stats.maximum_intensity != 0)
		stats[ns_stat_relative_intensity_normalized_average] = stats[ns_stat_relative_intensity_average] / whole_image_stats.whole_image_region_stats.relative_intensity_stats.maximum_intensity;

	stats[ns_stat_absolute_intensity_normalized_max] = 	(stats[ns_stat_absolute_intensity_max] - whole_image_stats.whole_image_region_stats.absolute_intensity_stats.average_intensity);
	
	stats[ns_stat_relative_intensity_normalized_max] = whole_image_stats.whole_image_region_stats.relative_intensity_stats.maximum_intensity - stats[ns_stat_relative_intensity_max];

	ns_vector_2d center = worm_center();
	//center+=bitmap_offset_in_context_image;
	stats[ns_stat_absolute_intensity_of_neighborhood] = 0;
	stats[ns_stat_relative_intensity_of_neighborhood] = 0;
	unsigned int non_worm_pixels = 0;
	int l=(int)center.x-200,
		r=(int)center.x+200,
		t=(int)center.y-200,
		b=(int)center.y+200;
	if (l < 0) l = 0;
	if (r >= (int)_worm_context_image->absolute_grayscale.properties().width)  r = (unsigned int)_worm_context_image->absolute_grayscale.properties().width-1;
	if (t < 0) t = 0;
	if (b >= (int)_worm_context_image->absolute_grayscale.properties().height) b = (unsigned int)_worm_context_image->absolute_grayscale.properties().height-1;
	ns_vector_2i bmp_offset(get_region_offset_in_context_image());
	for (int y = t; y <= b; y++){
		for (int x = l; x <= r; x++){
			if (x < (int)bmp_offset.x || x >= (int)bmp_offset.x+(int)_bitmap->properties().width ||
				y < (int)bmp_offset.y || y >= (int)bmp_offset.y+(int)_bitmap->properties().height ||
				!(*_bitmap)[y-(int)bmp_offset.y][x-(int)bmp_offset.x]){

				stats[ns_stat_absolute_intensity_of_neighborhood]+= _worm_context_image->absolute_grayscale[y][x];
				stats[ns_stat_relative_intensity_of_neighborhood]+= _worm_context_image->relative_grayscale[y][x];
				non_worm_pixels++;
			}
		}
	}
	if (non_worm_pixels != 0){
		stats[ns_stat_relative_intensity_of_neighborhood]/= non_worm_pixels;
		stats[ns_stat_absolute_intensity_of_neighborhood]/= non_worm_pixels;
	}
	stats[ns_stat_relative_intensity_distance_from_neighborhood] = stats[ns_stat_relative_intensity_average] - stats[ns_stat_relative_intensity_of_neighborhood];
	
	stats[ns_stat_absolute_intensity_distance_from_neighborhood] = stats[ns_stat_absolute_intensity_average] - stats[ns_stat_absolute_intensity_of_neighborhood];
	
	//worm_shape intensity
	stats[ns_stat_absolute_intensity_spine_average] = 0;
	stats[ns_stat_absolute_intensity_spine_variance] = 0;
	stats[ns_stat_absolute_intensity_normalized_spine_average] = 0;
	stats[ns_stat_relative_intensity_spine_average] = 0;
	stats[ns_stat_relative_intensity_spine_variance] = 0;
	stats[ns_stat_relative_intensity_normalized_spine_average] = 0;

	double worm_node_length(0);
	if (end_right > end_left){
		int dd(6);
		for (unsigned int i = end_left; i < end_right; i++){
			long xs((unsigned int)worm_shape.nodes[i].x-dd),
				 xf((unsigned int)worm_shape.nodes[i].x+dd),
				 ys((unsigned int)worm_shape.nodes[i].y-dd),
				 yf((unsigned int)worm_shape.nodes[i].y+dd);
			if (xs < 0)xs = 0;
			if (ys < 0)ys = 0;
			if (xf >= (long)_absolute_grayscale->properties().width)
				xf = (long)_absolute_grayscale->properties().width-1;
			if (yf >= (long)_absolute_grayscale->properties().height)
				yf = (long)_absolute_grayscale->properties().height-1;
			double abs_tsum(0),
				   rel_tsum(0);
			if (i != 0){
				unsigned long d((worm_shape.nodes[i]-worm_shape.nodes[i-1]).mag());
				for (long y = ys; y <= yf; y++)
					for (long x = xs; x <= xf; x++){
						abs_tsum += ( *_absolute_grayscale )[(unsigned int)worm_shape.nodes[i].y][(unsigned int)worm_shape.nodes[i].x];
						rel_tsum += ( *_relative_grayscale )[(unsigned int)worm_shape.nodes[i].y][(unsigned int)worm_shape.nodes[i].x];
					}
				long area = (xf-xs+1)*(yf-ys+1);
				worm_node_length+=d;
				if (area > 0){
					stats[ns_stat_relative_intensity_spine_average]+=d*rel_tsum/area;
					stats[ns_stat_absolute_intensity_spine_average]+=d*rel_tsum/area;
				}
			}
		}
		if (worm_node_length != 0){
			stats[ns_stat_absolute_intensity_spine_average]/= worm_node_length;
			stats[ns_stat_relative_intensity_spine_average]/= worm_node_length;
		}

		//normalize against average intensity for all regions detected in the image that contained the worm
		
		if (whole_image_stats.whole_image_region_stats.absolute_intensity_stats.average_intensity)
			stats[ns_stat_absolute_intensity_normalized_spine_average] = stats[ns_stat_absolute_intensity_spine_average]/whole_image_stats.whole_image_region_stats.absolute_intensity_stats.average_intensity;
		if (whole_image_stats.whole_image_region_stats.relative_intensity_stats.maximum_intensity)
			stats[ns_stat_relative_intensity_normalized_spine_average] = stats[ns_stat_absolute_intensity_spine_average]/whole_image_stats.whole_image_region_stats.relative_intensity_stats.maximum_intensity;
	
		for (unsigned int i = end_left; i < end_right; i++){
			double val = ( *_absolute_grayscale )[(unsigned int)worm_shape.nodes[i].y][(unsigned int)worm_shape.nodes[i].x] - stats[ns_stat_absolute_intensity_spine_average];
			stats[ns_stat_absolute_intensity_spine_variance] += val*val;
		
			val = ( *_relative_grayscale )[(unsigned int)worm_shape.nodes[i].y][(unsigned int)worm_shape.nodes[i].x] - stats[ns_stat_relative_intensity_spine_average];
			stats[ns_stat_relative_intensity_spine_variance] += val*val;
		}
		stats[ns_stat_absolute_intensity_spine_variance]/= (end_left - end_right);
		stats[ns_stat_relative_intensity_spine_variance]/= (end_left - end_right);
	}

	return stats;
}


ns_detected_worm_info::~ns_detected_worm_info(){
	ns_safe_delete(_bitmap);
	ns_safe_delete(_edge_bitmap);	
	ns_safe_delete(_absolute_grayscale);
	ns_safe_delete(_relative_grayscale);
	ns_safe_delete(_spine_visualization);
	ns_safe_delete(_worm_context_image);
	ns_safe_delete(_bitmap_of_worm_cluster);
}

ns_detected_worm_info::ns_detected_worm_info(const ns_detected_worm_info & wi){
	_bitmap = new ns_image_bitmap;
	_edge_bitmap = new ns_image_bitmap;
	_absolute_grayscale = new ns_image_standard;
	_relative_grayscale = new ns_image_standard;
	_spine_visualization = new ns_image_standard;
	_worm_context_image = new ns_worm_context_image;
	_bitmap_of_worm_cluster = new ns_image_bitmap;

	copy(wi);
}

void ns_detected_worm_info::copy(const ns_detected_worm_info & wi){
	region_position_in_source_image = wi.region_position_in_source_image;
	context_position_in_source_image = wi.context_position_in_source_image;
	region_size = wi.region_size;
	context_image_size = wi.context_image_size;
	area = wi.area;
	worm_shape = wi.worm_shape;
	edges = wi.edges;


	part_of_a_multiple_worm_cluster = wi.part_of_a_multiple_worm_cluster;
	whole_image_stats = wi.whole_image_stats;
	

	failure_reason << wi.failure_reason.text();
	must_be_a_worm = wi.must_be_a_worm;
	must_not_be_a_worm = wi.must_not_be_a_worm;
	interpolated = wi.interpolated;

	wi.bitmap().pump(*_bitmap,1024);
	wi.edge_bitmap().pump(*_edge_bitmap,1024);
	wi.bitmap_of_worm_cluster().pump(*_bitmap_of_worm_cluster,1024);
	wi.absolute_grayscale().pump(*_absolute_grayscale,1024);
	wi.relative_grayscale().pump(*_relative_grayscale,1024);
	wi.spine_visualization().pump(*_spine_visualization,1024);
	wi._worm_context_image->absolute_grayscale.pump(_worm_context_image->absolute_grayscale,1024);
	wi._worm_context_image->relative_grayscale.pump(_worm_context_image->relative_grayscale,1024);
}


void ns_detected_worm_info::extract_grayscale_from_large_image(const ns_vector_2i & position, const ns_image_standard & absolute_grayscale, const ns_image_standard &relative_grayscale){
//transfer the grayscale copy of the region to it.
	if (relative_grayscale.properties().width < position.x + region_size.x ||
		relative_grayscale.properties().height < position.y + region_size.y)
			throw ns_ex("ns_detected_worm_info::relative grayscale image was too small to include specified region(") << relative_grayscale.properties().width << "," << relative_grayscale.properties().height << ")";
	if (absolute_grayscale.properties().width < position.x + region_size.x ||
		absolute_grayscale.properties().height < position.y + region_size.y)
			throw ns_ex("ns_detected_worm_info::absolute grayscale image was too small to include specified region(") << absolute_grayscale.properties().width << "," << absolute_grayscale.properties().height << ")";
	if (_bitmap->properties().height != region_size.y || _bitmap->properties().width != region_size.x)
		throw ns_ex("ns_detected_worm_info::bitmap does not equal size! ");
	_absolute_grayscale->prepare_to_recieve_image(_bitmap->properties());
	_relative_grayscale->prepare_to_recieve_image(_bitmap->properties());
	//cerr << "late bitmap resolution = " << _grayscale->properties().resolution << "\n";
	//cerr << region.min_x + region.width() << "," << region.min_y + region.height() << " in " <<  grayscale_image.properties().width << "," << grayscale_image.properties().height << " and " << region.width() << "," << region.height() <<  " in " << _bitmap->properties().width << "," << _bitmap->properties().height << "\n";
	for (unsigned int y = 0; y < region_size.y; y++){
		for (unsigned int x = 0; x < region_size.x; x++){
		//	cerr << x << "," << y << "\n";
			(*_absolute_grayscale)[y][x] = absolute_grayscale[position.y+y][position.x+x];
			(*_relative_grayscale)[y][x] = relative_grayscale[position.y+y][position.x+x];
		}
	}
}
void ns_detected_worm_info::build_object_context_image(ns_detected_object & region, const ns_image_standard & absolute_grayscale, const ns_image_standard & relative_grayscale){
	
	//generate an image showing the worm's context
	int context_l = (int)region.offset_in_source_image.x - (int)ns_worm_collage_storage::context_border_size().x,
		context_r = (int)region.offset_in_source_image.x + region.size.x + (int)ns_worm_collage_storage::context_border_size().x,
		context_b = (int)region.offset_in_source_image.y - (int)ns_worm_collage_storage::context_border_size().y,
		context_t = (int)region.offset_in_source_image.y + region.size.y + (int)ns_worm_collage_storage::context_border_size().y;
	if (context_l < 0)
		context_l = 0;
	if ((unsigned int)context_r >= absolute_grayscale.properties().width)
		context_r = absolute_grayscale.properties().width-1;
	if (context_b < 0)
		context_b = 0;
	if ((unsigned int)context_t >= absolute_grayscale.properties().height)
		context_t = absolute_grayscale.properties().height-1;
	ns_image_properties p(_absolute_grayscale->properties());
	p.components = 1;
	//cout << "(" <<grayscale_image.properties().width << "," << grayscale_image.properties().height << ")\n";
//	cout << ">" << region.min_x << " " << region.max_x << ", " << region.min_y << " " << region.max_y << "\n";
//	cout << ":" << context_l << " " << context_r << ", " << context_b << " " << context_t << "\n";
	p.width = context_r - context_l + 1;
	p.height = context_t - context_b + 1;

	if (p.width == 0 || p.height == 0)
		throw ns_ex("ns_detected_worm_info::build_object_context_image::trying to make a context image that should have dimensions 0x0");
	_worm_context_image->absolute_grayscale.prepare_to_recieve_image(p);
	_worm_context_image->relative_grayscale.prepare_to_recieve_image(p);
	for (unsigned int y = 0; y < p.height; y++)
		for (unsigned int x = 0; x < p.width; x++){
			_worm_context_image->relative_grayscale[y][x] = relative_grayscale[context_b+y][context_l+x];
			_worm_context_image->absolute_grayscale[y][x] = absolute_grayscale[context_b+y][context_l+x];

		}
	context_position_in_source_image = ns_vector_2i(context_l,context_b);
	context_image_size = ns_vector_2i(p.width,p.height);
}

inline bool ns_intersects_hull(const ns_vector_2d & a, const ns_vector_2d & b, const std::vector<ns_edge_2d> & edges){
	ns_edge_2d edge(a,b);
	for (unsigned int i = 0; i < edges.size(); i++){
		ns_vector_2d isec;
		//the (b-isec).squared() >= 1 term handes pixels that lie on a diagonal edge
		if (ns_intersect_ss(edge, edges[i], isec) && (b-isec).squared() >= 1)
			return true;
	}
	return false;
}

struct ns_tip_halo{
	std::vector<ns_vector_2i> tip_associated_points[2];
	std::vector<ns_vector_2i> tip_associated_boundary_points[2];
};

inline ns_32_bit ns_encode_pix_info(const unsigned char worm_id, const unsigned long segment_node_id){
	return (((ns_32_bit)worm_id) << 24) | (0x00FFFFFF&(ns_32_bit)(segment_node_id+3));
}

inline unsigned char ns_worm_id_from_pix(const ns_32_bit & pix){
	return (unsigned char)(pix >> 24);
}
inline unsigned long ns_segment_node_id_from_pix(const ns_32_bit & pix){
	return (unsigned long)(0x00FFFFFF&pix) - 3;
}

inline bool ns_set_pixel_appropriately(const long & x,const long & y,const long & _x,const long & _y, ns_image_whole<ns_32_bit> & ids, ns_image_bitmap & changed, const unsigned long & node_distance_cutoff, const std::vector<ns_detected_worm_info *> & worms){
	if (ids[y+_y][x+_x] == 0 || ids[y+_y][x+_x] == 2)
		return false;
	if (ids[y+_y][x+_x] == 1){
		ids[y+_y][x+_x] = ids[y][x]; 
		changed[y+_y][x+_x]=true;
		return true;
	}

	//mark pixels where different worms touch
	if (ns_worm_id_from_pix(ids[y+_y][x+_x]) != ns_worm_id_from_pix(ids[y][x])){
		if ( !(worms[ns_worm_id_from_pix(ids[y+_y][x+_x])]->worm_shape.node_is_near_shared(ns_segment_node_id_from_pix(ids[y+_y][x+_x])) &&
			worms[ns_worm_id_from_pix(ids[y][x])]->worm_shape.node_is_near_shared(ns_segment_node_id_from_pix(ids[y][x])))){
			ids[y+_y][x+_x]= 2;
			return false;
		}
		else return false;
	}	
	//mark pixels where different segments of the same worm touch
	if ( node_distance_cutoff < (unsigned long)abs((long)ns_segment_node_id_from_pix(ids[y+_y][x+_x]) - (long)ns_segment_node_id_from_pix(ids[y][x]))){
		ids[y+_y][x+_x]= 2;
		return false;
	}
	return false;
}

//faster implementation of ns_divvy_up_bitmap_among_solutions
void ns_divvy_up_bitmap_among_solutions_alt(const ns_image_bitmap & bmp, std::vector<ns_detected_worm_info *> & worms){
	ns_image_whole<ns_32_bit> ids;
	ns_image_bitmap changed;
	ids.init(bmp.properties());
	changed.init(bmp.properties());
	for (unsigned int y = 0; y <ids.properties().height; y++)
		for (unsigned int x = 0; x <ids.properties().width; x++)
			ids[y][x] = (ns_32_bit)(bmp[y][x]==1);
	unsigned long t(ids.properties().height - 1);
	unsigned long r(ids.properties().width - 1);
	if (ids.properties().height == 0 || ids.properties().width == 0)
		throw ns_ex("ns_divvy_up_bitmap_among_solutions_alt::Empty bitmap received!");

	for (unsigned int i = 0; i < worms.size(); i++){
		for (unsigned int j = 0; j < worms[i]->worm_shape.nodes.size(); j++){
		//	if ((unsigned long)worms[i]->worm_shape.nodes[j].y > t || (unsigned long)worms[i]->worm_shape.nodes[j].x > r)
		//		throw ns_ex("ns_divvy_up_bitmap_among_solutions_alt::Out of bound node location found in worm shape specification (") << 
		//			worms[i]->worm_shape.nodes[j].x  << "," << worms[i]->worm_shape.nodes[j].y << ") in an image of dimensions (" << 
		//			r+1 << "," << t+1 << ")";
			ids[(unsigned long)worms[i]->worm_shape.nodes[j].y][(unsigned long)worms[i]->worm_shape.nodes[j].x] = ns_encode_pix_info(i,j);
		//	if (ns_worm_id_from_pix(ids[(unsigned long)worms[i]->worm_shape.nodes[j].y][(unsigned long)worms[i]->worm_shape.nodes[j].x]) >= worms.size())
		//		throw ns_ex("ns_divvy_up_bitmap_among_solutions_alt::Invalid worm id encoding!");
		//	if (ns_segment_node_id_from_pix(ids[(unsigned long)worms[i]->worm_shape.nodes[j].y][(unsigned long)worms[i]->worm_shape.nodes[j].x]) >= worms[i]->worm_shape.nodes.size())
		//		throw ns_ex("ns_divvy_up_bitmap_among_solutions_alt::Invalid worm segment id encoding!");	
		}

	}
			
	bool expansion_complete(false);
	unsigned int rounds(0);

	ns_image_standard out;
	out.init(ids.properties());

	while(!expansion_complete){

		if (rounds > ids.properties().height + ids.properties().width)
			throw ns_ex("ERR");
		expansion_complete = true;
 
		// vis
	/*	std::string fname = "c:\\tt\\exp_";
	
		for (unsigned int y = 1; y <ids.properties().height-1; y++){
			for (unsigned int x = 1; x <ids.properties().width-1; x++){
				out[y][x] = 127*(ids[y][x] >= 1) + 64*(ids[y][x] > 1) + 64*(ids[y][x] == 2);
			}
		}
		for (unsigned int i = 0; i < worms.size(); i++){
			for (unsigned int j = 0; j < worms[i]->worm_shape.nodes.size(); j++)
				out[(unsigned long)worms[i]->worm_shape.nodes[j].y][(unsigned long)worms[i]->worm_shape.nodes[j].x] = 0;
			}
		fname += ns_to_string(td) + "_" + ns_to_string(rounds) + ".tif";
		ns_save_image(fname,out);
		*/
		// /vis
		const unsigned int percent_of_node_to_break(7);
		for (unsigned int y = 1; y < t; y++)
			for (unsigned int x = 1; x < r; x++)
				changed[y][x] = false;

		for (unsigned int y = 1; y < t; y++){
			for (unsigned int x = 1; x < r; x++){
				if (changed[y][x] || ids[y][x] < 3)
					continue;
				const unsigned long nodes_to_break((unsigned long)worms[ns_worm_id_from_pix(ids[y][x])]->worm_shape.nodes.size()/percent_of_node_to_break);
		
				//the expansion is complete if no actions are taken looking at any of the neighboring pixels
				bool action_taken[4] = {
										ns_set_pixel_appropriately(x,y,1,0,ids,changed,nodes_to_break,worms),
										ns_set_pixel_appropriately(x,y,-1,0,ids,changed,nodes_to_break,worms),
										ns_set_pixel_appropriately(x,y,0,-1,ids,changed,nodes_to_break,worms),
										ns_set_pixel_appropriately(x,y,0,1,ids,changed,nodes_to_break,worms)
				};
				expansion_complete =	expansion_complete && !action_taken[0] && !action_taken[1] && !action_taken[2] && !action_taken[3];
				/*if (ids[y-1][x] == 1){ids[y-1][x] = ids[y][x]; expansion_complete = false; changed[y-1][x]=true;}
				if (ids[y+1][x] == 1){ids[y+1][x] = ids[y][x]; expansion_complete = false; changed[y+1][x]=true;}
				if (ids[y][x-1] == 1){ids[y][x-1] = ids[y][x]; expansion_complete = false; changed[y][x-1]=true;}
				if (ids[y][x+1] == 1){ids[y][x+1] = ids[y][x]; expansion_complete = false; changed[y][x+1]=true;}*/
			}
		}
		
		
		rounds++;
		//cerr << "Round " << rounds << "\n";

	}		

	std::vector<char> worm_has_shared_segment(worms.size(),0);
	for (unsigned int i = 0; i < worms.size(); i++){
		for (unsigned int j = 0; j < worms[i]->worm_shape.nodes.size(); j++)
			if (worms[i]->worm_shape.node_is_shared(j)){
				worm_has_shared_segment[i] = true;
				break;
			}
	}


	//expand the pixels that mark the boundary between contiguous regions
	//(if we don't expand them they aren't picked up by the edge detection algorithm)
	for (unsigned int y = 1; y <t; y++)
			for (unsigned int x = 1; x <r; x++)
				changed[y][x] = false;
	for (unsigned int y = 1; y <t; y++){
			for (unsigned int x = 1; x < r; x++){
				if (changed[y][x] || ids[y][x] != 2) continue;
				if (ids[y-1][x] > 2){changed[y-1][x] = 1; ids[y-1][x] = 2;}
				if (ids[y+1][x] > 2){changed[y+1][x] = 1; ids[y+1][x] = 2;}
				if (ids[y][x-1] > 2){changed[y][x-1] = 1; ids[y][x-1] = 2;}
				if (ids[y][x+1] > 2){changed[y][x+1] = 1; ids[y][x+1] = 2;}
			}
	}
	for (unsigned int i = 0; i < worms.size(); i++){
		for (unsigned int y = 0; y <ids.properties().height; y++){
			for (unsigned int x = 0; x <ids.properties().width; x++){
				worms[i]->bitmap()[y][x] = (ids[y][x] > 2 && (
										ns_worm_id_from_pix(ids[y][x])==i ||  //if the pixel belongs to the worm, add it to the bitmap
																			  //if the pixel doesn't belong to the worm, but belongs to a shared segment
																			  //of another worm, add it.  This works because there is only one shared segment per object cluster
										worm_has_shared_segment[i] && worms[ns_worm_id_from_pix(ids[y][x])]->worm_shape.node_is_shared(ns_segment_node_id_from_pix(ids[y][x])))
										);
			}
		}
	}

	//these are temp variables used 
	std::stack<ns_vector_2i> temp_flood_fill_stack;
	ns_image_bitmap  temp;
	temp.use_more_memory_to_avoid_reallocations();
	std::vector<ns_vector_2d> output_coordinates;
	std::vector<ns_vector_2d> holes;
	std::vector<ns_edge_ui> edge_list;
	//rebuild edge bitmaps;
	for (unsigned int i = 0; i < worms.size(); i++)
		ns_calculate_res_aware_edges(worms[i]->bitmap(), worms[i]->edge_bitmap(),
			output_coordinates,holes,edge_list,
			worms[i]->edges,temp_flood_fill_stack,temp);
}
void ns_divvy_up_bitmap_among_solutions(const ns_image_bitmap & bmp,std::vector<ns_detected_worm_info *> & worms){
	ns_image_properties p(bmp.properties());

	std::vector<ns_tip_halo> tip_halos(worms.size());
	for (unsigned int y = 0; y < p.height; y++){
		for (unsigned int x = 0; x < p.width; x++){
			if (!bmp[y][x]) continue;

			double min_distance_to_cur_pix = DBL_MAX;
			unsigned long closest_worm_id = 0;
			double min_distance_to_cur_pix_without_line_of_sight = DBL_MAX;
			unsigned long closest_worm_without_line_of_sight_id = 0;

			for (unsigned int i = 0; i < worms.size(); i++){
				//find the closest point on this worm to the specified pixel
				double min_distance = DBL_MAX;
				unsigned int min_node_id;
				for (unsigned int j = 0; j < worms[i]->worm_shape.nodes.size(); j++){
					double d = (worms[i]->worm_shape.nodes[j]-ns_vector_2d(x,y)).squared();
					if (d < min_distance){
						min_distance = d;
						min_node_id = j;
					}
				}
				if (min_distance == DBL_MAX)
					continue;

				//if we can find a solution that has direct line of site, use it!
				if (min_distance < min_distance_to_cur_pix && !ns_intersects_hull(worms[i]->worm_shape.nodes[min_node_id],ns_vector_2d(x,y),worms[i]->edges)){
					//recognize all pixels closest to the ends of segments;
					if (min_node_id == 0)
						tip_halos[i].tip_associated_points[0].push_back(ns_vector_2i(x,y));
					else if (min_node_id == worms[i]->worm_shape.nodes.size()-1)
						tip_halos[i].tip_associated_points[1].push_back(ns_vector_2i(x,y));
					min_distance_to_cur_pix = min_distance;
					closest_worm_id = i;
				}
				else if (min_distance < min_distance_to_cur_pix_without_line_of_sight){
					min_distance_to_cur_pix_without_line_of_sight = min_distance;
					closest_worm_without_line_of_sight_id = i;
				}
			}
			for (unsigned int i = 0; i < worms.size(); i++)
				worms[i]->bitmap()[y][x] = 0;

			if (min_distance_to_cur_pix != DBL_MAX)
				worms[closest_worm_id]->bitmap()[y][x] = 1;
			else if (min_distance_to_cur_pix_without_line_of_sight != DBL_MAX)
				worms[closest_worm_without_line_of_sight_id]->bitmap()[y][x] = 1;
		}
	}
	
	for (unsigned int i = 0; i < worms.size(); i++){
		ns_vector_2i off[4] = {ns_vector_2i(1,0),ns_vector_2i(0,-1),ns_vector_2i(-1,0),ns_vector_2i(0,1)};
		
		unsigned long cutoff_start[2] = {0, (unsigned long)worms[i]->worm_shape.nodes.size()- (unsigned long)worms[i]->worm_shape.nodes.size()/4};
		unsigned long cutoff_end[2] = {(unsigned long)worms[i]->worm_shape.nodes.size()/4, (unsigned long)worms[i]->worm_shape.nodes.size()};

		ns_image_properties prop(worms[i]->bitmap().properties());
		for (unsigned int e = 0; e < 2; e++){
			for (unsigned int j = 0; j < tip_halos[i].tip_associated_points[e].size(); j++){
				bool on_edge(false);
				for (unsigned int k = 0; k < 4; k++){
					double min_dist = DBL_MAX;
					double min_dist_id = 0;
					for (long l = (long)!e; l < (long)worms[i]->worm_shape.nodes.size()-(long)e; l++){
						ns_vector_2i pt= tip_halos[i].tip_associated_points[e][j]+off[k];
						if (pt.y < 0 || pt.y >= (int)prop.height) continue;
						if (pt.x < 0 || pt.x >= (int)prop.width) continue;
						if (!worms[i]->bitmap()[pt.y][pt.x]) continue;

						double d = (pt - worms[i]->worm_shape.nodes[l]).mag();
						if (d < min_dist){
							min_dist = d;
							min_dist_id = l;
						}
					}
					if (min_dist != DBL_MAX && (cutoff_start[e] > min_dist_id || min_dist_id > cutoff_end[e])){
						on_edge = true;
						break;
					}
				}
				if (on_edge)
					tip_halos[i].tip_associated_boundary_points[e].push_back(tip_halos[i].tip_associated_points[e][j]);
				}
		}
	}


	//fill in tip_halos
	for (unsigned int i = 0; i < worms.size(); i++){
		for (unsigned int j = 0; j < tip_halos[i].tip_associated_boundary_points[0].size(); j++)
			worms[i]->bitmap()[tip_halos[i].tip_associated_boundary_points[0][j].y][tip_halos[i].tip_associated_boundary_points[0][j].x] = 0;
		for (unsigned int j = 0; j < tip_halos[i].tip_associated_boundary_points[1].size(); j++)
			worms[i]->bitmap()[tip_halos[i].tip_associated_boundary_points[1][j].y][tip_halos[i].tip_associated_boundary_points[1][j].x] = 0;
	}

	std::stack<ns_vector_2i> temp_flood_fill_stack;
	ns_image_bitmap temp;
	temp.use_more_memory_to_avoid_reallocations();
	//rebuild edge bitmaps;
	for (unsigned int i = 0; i < worms.size(); i++){
		ns_detected_object new_object;
		worms[i]->bitmap().pump(new_object.bitmap(),1024);
		new_object.calculate_edges(temp_flood_fill_stack,temp);
		new_object.edge_bitmap().pump(worms[i]->edge_bitmap(),512);
		worms[i]->edges = new_object.edges;
	}
}
void ns_detected_worm_info::finalize_stats_from_shape(){
	area = 0;
	//mask the grayscale image accordingly, and calculate the area
	for (unsigned int y = 0; y < bitmap().properties().height; y++){
		for (unsigned int x = 0; x < bitmap().properties().width; x++)
			if (!bitmap()[y][x]){
				 absolute_grayscale()[y][x] = 0;
				 relative_grayscale()[y][x] = 0;

			}
			else area++;
	}
}
unsigned int ns_detected_worm_info::from_segment_cluster_solution(
	ns_detected_object & region, std::vector<ns_detected_worm_info> & worms, unsigned int offset, std::vector<std::vector<ns_detected_worm_info *> > & mutually_exclusive_groups, 
	const ns_image_standard & relative_grayscale_image, const ns_image_standard & absolute_grayscale_image, const ns_grayscale_image_type & type, const ns_visualization_type visualization_type){
	if (region.segment_cluster_solutions.mutually_exclusive_solution_groups.size() == 0)
		return 0;
	if (region.segment_cluster_solutions.mutually_exclusive_solution_groups[0].size() == 0)
		return 0;

	//we have at least one worm, trasfer the region bitmap to it.
	delete worms[offset]._bitmap;
	delete worms[offset]._edge_bitmap;
	worms[offset]._bitmap = region.transfer_bitmap();
	worms[offset]._edge_bitmap = region.transfer_edge_bitmap();
	worms[offset].must_be_a_worm = region.must_be_a_worm;
	worms[offset].must_not_be_a_worm = region.must_not_be_a_worm;

	worms[offset].region_size = region.size;
	worms[offset].region_position_in_source_image = region.offset_in_source_image;

	//build context image if requested
	ns_vector_2i worm_position_in_grayscale(0,0);
	if(type == ns_large_source_grayscale_images_provided)
		worm_position_in_grayscale = worms[offset].region_position_in_source_image;
	worms[offset].extract_grayscale_from_large_image(worm_position_in_grayscale,absolute_grayscale_image,relative_grayscale_image);

	if (type==ns_large_source_grayscale_images_provided)
		worms[offset].build_object_context_image(region,absolute_grayscale_image,relative_grayscale_image);
	else{
		worms[offset].context_position_in_source_image = worms[offset].region_position_in_source_image;
		worms[offset].context_image_size = worms[offset].region_size;
	}

	//now, copy over bitmap information to each putative worm in the region
	ns_image_bitmap whole_worm_bitmap;
	ns_image_standard whole_worm_absolute_grayscale;
	ns_image_standard whole_worm_relative_grayscale;
	ns_worm_context_image whole_worm_context;
	ns_image_bitmap whole_worm_edge_bitmap;
	ns_image_bitmap whole_worm_bitmap_of_worm_cluster;
	worms[offset].bitmap().pump(whole_worm_bitmap,512);
	worms[offset].bitmap().pump(worms[offset].bitmap_of_worm_cluster(),512);
	worms[offset].bitmap().pump(whole_worm_bitmap_of_worm_cluster,512);
	worms[offset].absolute_grayscale().pump(whole_worm_absolute_grayscale,512);
	worms[offset].relative_grayscale().pump(whole_worm_relative_grayscale,512);
	worms[offset]._worm_context_image->absolute_grayscale.pump(whole_worm_context.absolute_grayscale,512);
	worms[offset]._worm_context_image->relative_grayscale.pump(whole_worm_context.relative_grayscale,512);
	worms[offset]._worm_context_image->combined_image.pump(whole_worm_context.combined_image,512);
	worms[offset].edge_bitmap().pump(whole_worm_edge_bitmap,512);
	ns_vector_2i whole_worm_offset_in_context_image = worms[offset].get_region_offset_in_context_image();
	ns_vector_2i whole_worm_context_size = worms[offset].context_image_size;
	ns_object_hand_annotation_data whole_worm_annotations(worms[offset].hand_annotations);

	
	//first we build a list of detected_worm_info mutually exclusive groups from the region specification
	mutually_exclusive_groups.resize(region.segment_cluster_solutions.mutually_exclusive_solution_groups.size());

	unsigned int number_of_worms_added = 0;
	for (unsigned int g = 0; g < region.segment_cluster_solutions.mutually_exclusive_solution_groups.size(); g++){
		
		if (region.segment_cluster_solutions.mutually_exclusive_solution_groups[g].size() == 0) 
			throw ns_ex("ns_detected_worm_info::from_segment_cluster_solution::No worms could be constructed from provided detected_object");
		mutually_exclusive_groups[g].resize(region.segment_cluster_solutions.mutually_exclusive_solution_groups[g].size());

		for (unsigned int n = 0; n < region.segment_cluster_solutions.mutually_exclusive_solution_groups[g].size(); n++){
			if (offset+number_of_worms_added >= worms.size())
				throw ns_ex("ns_detected_worm_info::From region allocation error.");
			ns_detected_worm_info & worm = worms[offset+number_of_worms_added];
			worm.hand_annotations = whole_worm_annotations;
			worm.region_size = region.size;
			worm.context_image_size = whole_worm_context_size;
			worm.region_position_in_source_image = region.offset_in_source_image;
			worm.context_position_in_source_image =  region.offset_in_source_image - whole_worm_offset_in_context_image;
			worm.edges = region.edges;
			//create copy of bitmap for new detected worm.
			if (number_of_worms_added != 0){
				whole_worm_bitmap.pump(*worm._bitmap,1024);
				whole_worm_absolute_grayscale.pump(*worm._absolute_grayscale,1024);
				whole_worm_relative_grayscale.pump(*worm._relative_grayscale,1024);
				whole_worm_context.absolute_grayscale.pump(worm._worm_context_image->absolute_grayscale,1024);
				whole_worm_context.relative_grayscale.pump(worm._worm_context_image->relative_grayscale,1024);
				whole_worm_context.combined_image.pump(worm._worm_context_image->combined_image,1024);
				whole_worm_edge_bitmap.pump(*worm._edge_bitmap,1024);		
				whole_worm_bitmap_of_worm_cluster.pump(*worm._bitmap_of_worm_cluster,1024);
				worm.must_be_a_worm = worms[offset].must_be_a_worm;
				worm.must_not_be_a_worm = worms[offset].must_not_be_a_worm;
			}
			if (region.segment_cluster_solutions.mutually_exclusive_solution_groups[g].size() > 0)
				worm.part_of_a_multiple_worm_cluster = true;
			worm.worm_shape.from_segment_cluster_solution(region.segment_cluster_solutions.mutually_exclusive_solution_groups[g][n]);

			for (unsigned long i = 0; i < (unsigned long) worm.worm_shape.nodes.size(); i++){
				//crop bspline to inside the bitmap
				if (worm.worm_shape.nodes[i].x < 0) worm.worm_shape.nodes[i].x = 0;
				if (worm.worm_shape.nodes[i].y < 0) worm.worm_shape.nodes[i].y = 0;
				if (worm.worm_shape.nodes[i].x >= worm.bitmap().properties().width)
					worm.worm_shape.nodes[i].x = worm.bitmap().properties().width-1;
				if (worm.worm_shape.nodes[i].y >= worm.bitmap().properties().height)
					worm.worm_shape.nodes[i].y = worm.bitmap().properties().height-1;

			}

			mutually_exclusive_groups[g][n] = &worm;
			number_of_worms_added++;
		}
	}
	

	//now we handle divvying up the complex segments' bitmaps into their solutions
	for (unsigned int g = 0; g < mutually_exclusive_groups.size(); g++){
		
		#ifdef NS_RECALULCATE_SUBSPINE_BITMAPS
		//bool multiple_worm_cluster = !(mutually_exclusive_groups.size() == 1 && mutually_exclusive_groups[g].size() == 1);
		ns_divvy_up_bitmap_among_solutions_alt(whole_worm_bitmap,mutually_exclusive_groups[g]);
		#endif
		for (unsigned int n = 0; n < mutually_exclusive_groups[g].size(); n++){
			mutually_exclusive_groups[g][n]->worm_shape.finalize_worm_and_calculate_width(mutually_exclusive_groups[g][n]->edges);
			for (unsigned int j = 0; j < mutually_exclusive_groups[g][n]->worm_shape.nodes.size(); j++){
				if (!mutually_exclusive_groups[g][n]->bitmap()[(unsigned long)mutually_exclusive_groups[g][n]->worm_shape.nodes[j].y][(unsigned long)mutually_exclusive_groups[g][n]->worm_shape.nodes[j].x]){
					mutually_exclusive_groups[g][n]->worm_shape.width[j]= 0;
					mutually_exclusive_groups[g][n]->worm_shape.normal_0[j] = ns_vector_2d(0,0);
					mutually_exclusive_groups[g][n]->worm_shape.normal_1[j] = ns_vector_2d(0,0);
				}
			}
			mutually_exclusive_groups[g][n]->finalize_stats_from_shape();
		}
	}

	ns_image_standard reg_vis;
	ns_image_properties reg_vis_prop;
	//now draw some graphics
	if (visualization_type != ns_vis_none){
		for (unsigned int g = 0; g < mutually_exclusive_groups.size(); g++){
			for (unsigned int n = 0; n < mutually_exclusive_groups[g].size(); n++){
				ns_detected_worm_info & worm = *mutually_exclusive_groups[g][n];
				ns_worm_shape & sp = worm.worm_shape;
				ns_segment_cluster_solution_group & sn = region.segment_cluster_solutions.mutually_exclusive_solution_groups[g];

				#ifdef NS_ZHANG_THINNING_ON_SPINE		
					ns_image_bitmap zhang_image;
					ns_zhang_thinning(worms[offset+worm_id_offset].bitmap(),zhang_image);
				#endif

				#ifdef NS_DRAW_THRESHOLD_ON_SPINE
					reg_vis_prop =  worm.grayscale().properties();
					reg_vis_prop.components = 3;
					reg_vis.prepare_to_recieve_image(reg_vis_prop);
					for (unsigned int y = 0; y < reg_vis_prop.height; y++)
						for (unsigned int x = 0; x < reg_vis_prop.width; x++){
							reg_vis[y][3*x] = worm.grayscale()[y][x];
							reg_vis[y][3*x+1] = worm.bitmap()[y][x] ? 255/2 : worm.grayscale()[y][x];
							reg_vis[y][3*x+2] = worm.bitmap()[y][x] ? 255   : worm.grayscale()[y][x];
							#ifdef NS_ZHANG_THINNING_ON_SPINE	
							if (zhang_image[y][x]!=0){
								reg_vis[y][3*x] = 0;
								reg_vis[y][3*x+1] = 255;
								reg_vis[y][3*x+2] = 0;
							}
							#endif
						}
				#else		
					reg_vis_prop =  worm.absolute_grayscale().properties();
					reg_vis.prepare_to_recieve_image(reg_vis_prop);	
					if (reg_vis_prop.height != 0 && reg_vis_prop.width != 0){
						//find darkest, lightest pixel
						int min, max;
						int lightest = 200;
						min = max = worm.absolute_grayscale()[0][0];
						for (unsigned int y = 0; y < reg_vis_prop.height; y++)
							for (unsigned int x = 0; x < reg_vis_prop.width; x++){
								int val = worm.absolute_grayscale()[y][x];
								if (val < min)
									min = val;
								if (val > max)
									max = val;
							}
						if (max == min)
							max = min+1;
						for (unsigned int y = 0; y < reg_vis_prop.height; y++)
							for (unsigned int x = 0; x < reg_vis_prop.width; x++){
								reg_vis[y][x] = (ns_8_bit)((((float)lightest)*((float)(worm.absolute_grayscale()[y][x]-min)))/((float)(max-min)));
								reg_vis[y][x]*=worm.bitmap()[y][x];
								#ifdef NS_ZHANG_THINNING_ON_SPINE	
									if (zhang_image[y][x]!=0){
										reg_vis[y][x] = 0;
									}
								#endif
							}
					}
				#endif
				ns_spine_drawer spine_drawer;
				if (visualization_type == ns_vis_raster || visualization_type==ns_vis_both){
					spine_drawer.draw_spine(reg_vis,region.segment_cluster,sp,*(worm._spine_visualization),ns_worm_detection_constants::get(ns_worm_detection_constant::spine_visualization_resize_factor,whole_worm_bitmap.properties().resolution));
					#ifdef NS_DRAW_SPINE_NORMALS
					spine_drawer.draw_normals(sp,*(worm._spine_visualization),ns_worm_detection_constants::get(ns_worm_detection_constant::spine_visualization_resize_factor,whole_worm_bitmap.properties().resolution));
					#endif
				}
				if (visualization_type == ns_vis_svg || visualization_type==ns_vis_both){
					spine_drawer.draw_spine(reg_vis,region.segment_cluster,sp,worm._spine_svg_visualization);
					spine_drawer.draw_normals(sp,worm._spine_svg_visualization);
					std::vector<ns_vector_2d> edges(2*worms[offset].edges.size());
					for (unsigned long k = 0; k < (unsigned long)worms[offset].edges.size(); k++){
						edges[2*k] = worms[offset].edges[k].vertex[0];
						edges[2*k+1] = worms[offset].edges[k].vertex[1];
					}
					spine_drawer.draw_edges(worm._spine_svg_visualization,edges);
				}
			}
		}
	}
	return number_of_worms_added;
}




void ns_detected_worm_stats::from_string(std::istream & str){
	std::string tmp;
	
	for (unsigned int i = 0; i < (unsigned int)ns_stat_number_of_stats; i++){
		if (_model->included_statistics[i] == 0) continue;
		//first to characters are #: representing the feature number suffix
		char a(' ');
		while(a != ':'){
			str.get(a);
			if (str.fail())
				throw ns_ex("Parsing Error");
		}
		double scaled_val;
		str >> scaled_val;
		statistics[i] = scaled_val;//(_model->statistics_ranges[i].max - _model->statistics_ranges[i].min)*scaled_val +_model->statistics_ranges[i].min;
	}
	std::string b;
	getline(str,b);
}

void ns_detected_worm_stats::from_normalized_string(std::istream & str){
	from_string(str);
	for (unsigned int i = 0; i < (unsigned int)ns_stat_number_of_stats; i++){
		//cerr << _model->statistics_ranges[i].min << "-" << _model->statistics_ranges[i].max << "\n";
		statistics[i] = (_model->statistics_ranges[i].max - _model->statistics_ranges[i].min)*statistics[i] +_model->statistics_ranges[i].min;
	}
}


void ns_detected_worm_stats::output_html_worm_summary(ostream & out){
	out << "<table><tr><td valign = top><img src=\"" << debug_filename << "\" border = 0></td><td>\n";
	out << "\t<table>";
	for (unsigned int i = 0; i < (unsigned int)ns_stat_number_of_stats; i++){
			out << "\t<tr><td>" << ns_classifier_label((ns_detected_worm_classifier)i) << "</td>";
			out << "<td>" << statistics[i] << "</td></tr>\n";
	}
	out << "\t</table>\n";
	out << "</td></tr></table>\n";
}
void ns_detected_worm_stats::output_csv_data(const ns_64_bit region_id, const unsigned long capture_time, const ns_vector_2i & position, const ns_vector_2i & size,const ns_object_hand_annotation_data & hand_data,ostream & out){
	out << region_id << "," << capture_time << "," 
		<< position.x << "," << position.y << "," 
		<< size.x << "," << size.y << ",";
	hand_data.out_data(out);
	for (unsigned int i = 0; i < (unsigned int)ns_stat_number_of_stats; i++){
		out << "," << statistics[i];
	}
}

void  ns_detected_worm_stats::output_csv_header(ostream & out){
	out << "Source Region ID,"
		<< "Capture Time,"
		<< "Position in Source Image X,"
		<< "Position in Source Image Y,"
		<< "Size in Source Image X,"
		<< "Size in Source Image Y,";
	ns_object_hand_annotation_data::out_header(out);
	for (unsigned int i = 0; i < (unsigned int)ns_stat_number_of_stats; i++)
		out << "," << ns_classifier_abbreviation((ns_detected_worm_classifier)i);
}

ns_vector_2d ns_detected_worm_info::worm_center() const{
	ns_vector_2d center(0,0);
	for (unsigned int i = 0; i < worm_shape.nodes.size(); i++){
		center += worm_shape.nodes[i];
	}
	center/=worm_shape.nodes.size();
	return center;
}

void ns_detected_worm_info::draw_orientation_vector(ns_image_standard & out) const{
	unsigned int length = out.properties().width/2;
	if (out.properties().width > out.properties().height)
		length = out.properties().height/2;
	for (unsigned int y = 0; y < out.properties().height; y++)
		for (unsigned int x = 0; x < out.properties().width*out.properties().components; x++)
			out[y][x] = 0;
	double angle = worm_shape.angular_orientation();
	ns_vector_2i vec(length*(int)cos(angle),length*(int)sin(angle));
	ns_vector_2i cen(out.properties().width/2,out.properties().height/2);
	out.draw_line_color(cen,vec+cen,ns_color_8(255,255,255));
	out.draw_line_color(cen+ns_vector_2i(2,2),cen-ns_vector_2i(2,2),ns_color_8(255,255,255));
	out.draw_line_color(cen+ns_vector_2i(2,-2),cen-ns_vector_2i(2,-2),ns_color_8(255,255,255));
	out.draw_line_color(cen+ns_vector_2i(0,2),cen-ns_vector_2i(0,2),ns_color_8(255,255,255));
	out.draw_line_color(cen+ns_vector_2i(2,0),cen-ns_vector_2i(2,0),ns_color_8(255,255,255));
}

void ns_detected_worm_info::make_curvature_graph(ns_graph & graph){
	ns_graph_object curvature(ns_graph_object::ns_graph_dependant_variable);
	if (worm_shape.width.size() > 3){
		curvature.x.resize(worm_shape.curvature.size());
		curvature.y.resize(worm_shape.curvature.size());
		for (unsigned int i = 0; i < curvature.x.size(); i++)
			curvature.x[i] = i;
		for (unsigned int i = 0; i < curvature.y.size(); i++)
			curvature.y[i] = worm_shape.curvature[i];
		/*if (bitmap().properties().resolution <= 1201)
			ns_calculate_curvature<ns_worm_detection_constants::spine_smoothing_radius_1200>(worm_shape.nodes,curvature.y);
		else
			ns_calculate_curvature<ns_worm_detection_constants::spine_smoothing_radius_3200>(worm_shape.nodes,curvature.y);*/
	}
	for (unsigned int i = 0; i < curvature.y.size(); i++)
		curvature.y[i]*=100;
	/*widths.properties.area_fill.draw=false;
	widths.properties.area_fill.color=ns_color_8(0,50,150);
	widths.properties.area_fill.opacity = 1.0;*/
	curvature.properties.point.color=ns_color_8(0,50,150);
	curvature.properties.line.draw=true;
	curvature.properties.line.color=ns_color_8(150,150,150);
	curvature.properties.line.opacity = 1.0;
	curvature.properties.line.width= 1;
	graph.contents.push_back(curvature);
}
ns_image_standard * ns_detected_worm_info::curvature_graph(){
	ns_image_standard * im = new ns_image_standard;
	ns_image_properties prop;
	prop.components = 3;
	prop.height = 200;
	prop.width = 200;
	im->init(prop);
	ns_graph graph;
	make_curvature_graph(graph);
	
	graph.set_graph_display_options("Curvature");
	graph.draw(*im);
	return im;
}
void ns_detected_worm_info::curvature_graph(ns_svg & svg){
	
	ns_graph graph;
	make_curvature_graph(graph);	
	ns_graph_axes axis; 
	axis.boundary(0) = 0;
	axis.boundary(1) = 120;
	axis.boundary(2) = -5;
	axis.boundary(3) = 5;
	graph.set_graph_display_options("Curvature",axis);
	graph.draw(svg);
}

void ns_detected_worm_info::make_width_graph(ns_graph &graph){
	ns_graph_object widths(ns_graph_object::ns_graph_dependant_variable);

	widths.x.resize(worm_shape.width.size());
	widths.y.resize(worm_shape.width.size());
	for (unsigned int i = 0; i < widths.y.size(); i++){
		widths.x[i] = i;
		widths.y[i] = worm_shape.width[i];
	}
	widths.properties.text_size = 6;
	widths.properties.point.color=ns_color_8(0,50,150);
	widths.properties.line.draw=true;
	widths.properties.line.color=ns_color_8(150,150,150);
	widths.properties.line.opacity = 1.0;
	widths.properties.line.width= 1;
	graph.contents.push_back(widths);
}

void ns_detected_worm_info::width_graph(ns_svg & svg){
	
	ns_graph graph;
	make_width_graph(graph);
	graph.set_graph_display_options("Width");	
	graph.draw(svg);
}
ns_image_standard * ns_detected_worm_info::width_graph(){
	ns_image_standard * im = new ns_image_standard;
	ns_image_properties prop;
	prop.components = 3;
	prop.height = 200;
	prop.width = 200;
	im->init(prop);
	ns_graph graph;
	make_width_graph(graph);
	graph.set_graph_display_options("Width");
	graph.draw(*im);
	return im;
}


bool ns_detected_worm_info::is_a_worm(){
	if (!is_a_worm_set)
		throw ns_ex("ns_detected_worm_info::Requesting cached worm status before it has been calculated");
	return is_a_worm_;
}

bool ns_detected_worm_info::is_a_worm(const ns_svm_model_specification & model){

	#ifdef NS_USE_MACHINE_LEARNING
		#ifdef ALLOW_ALL_SPINE_PERMUTATIONS
		return true;
		#endif
		ns_detected_worm_stats stats(generate_stats());
		stats.specifiy_model(model);

		if (stats.not_a_worm){
			is_a_worm_ = false;
			is_a_worm_set = true;
			return false;
		}

		if (must_be_a_worm){
			is_a_worm_ = true;
			is_a_worm_set = true;
			return true;
		}
		if (must_not_be_a_worm){
			is_a_worm_ = false;
			is_a_worm_set = true;
			failure_reason << "Manual Override";
			return false;
		}


		#ifdef NS_USE_TINSVM
			std::string st = stats.parameter_string();
			is_a_worm_ = model.model.classify(st.c_str()) > 0;
		#else
			svm_node * node = stats.produce_vector();

			double val = svm_predict(model.model,node);
			stats.delete_vector(node);
			//cerr << val << "\n";
			is_a_worm_ = (val > 0);
			if (!is_a_worm_)
				failure_reason << "Failed SVM";
		#endif
			is_a_worm_set = true;
			return is_a_worm_;

	#else

		ns_detected_worm_stats stats = this->generate_stats();
		//return true;
		const unsigned int end_d(4);
		if (stats.number_of_spine_nodes==0){
			failure_reason = "no_nodes";
			return false;
		}

		if (stats.number_of_spine_nodes < end_d*2){
			failure_reason = "few_nodes";
			return false;
		}

		if (!ns_worm_detection_constants::correct_spine_length(stats.spine_length)){
			failure_reason << "length(" << ns_to_string_short(stats.spine_length) << ")";
			return false;
		}


		//minimum maximum size cutoffs

		if (stats.average_width >= ns_worm_detection_constants::max_average_worm_width()){
			failure_reason << "avg_width(" << ns_to_string_short(stats.average_width) << ")";
			return false;
		}
		if (stats.average_width <= ns_worm_detection_constants::min_average_worm_width()){
			failure_reason << "avg_width(" << ns_to_string_short(stats.average_width) << ")";
			return false;
		}
		if (stats.max_width <= ns_worm_detection_constants::min_maximum_worm_width()){
			failure_reason << "max_width(" << ns_to_string_short(stats.max_width) << ")";
			return false;
		}
		if (stats.max_width >= ns_worm_detection_constants::max_maximum_worm_width()){
			failure_reason << "max_width(" << ns_to_string_short(stats.max_width) << ")";
			return false;
		}

		//no bulges
		if (stats.spine_length_to_max_width_ratio < 2){
			failure_reason << "mw(" << ns_to_string_short(stats.max_width) << ")aw(" << ns_to_string_short(stats.average_width) << ")";
			return false;
		}

		//not thicker than they are long
		if (stats.spine_length_to_average_width < 1.5){
			failure_reason << "mw(" << ns_to_string_short(stats.max_width) << ")sl(" << ns_to_string_short(stats.spine_length) << ")";
			return false;
		}


		if (stats.end_width_to_middle_width_ratio[0] > 2 || stats.end_width_to_middle_width_ratio[0] < .5 ||
			stats.end_width_to_middle_width_ratio[1] > 2 || stats.end_width_to_middle_width_ratio[1] < .5){
			failure_reason << "w(" << ns_to_string_short(stats.width_at_end[0]) << "," << ns_to_string_short(stats.width_at_center) << "," << ns_to_string_short(stats.width_at_end[1])<< ")";
			return false;
		}
		//check to see if the worm is roughly symmetric
		if (stats.end_width_ratio > 1.5 || stats.end_width_ratio < .666){
			failure_reason << "w(" << ns_to_string_short(stats.width_at_end[0]) << "," << ns_to_string_short(stats.width_at_end[1]) << ")";
			return false;
		}
		//remove worms that don't take up most of their region bitmap
		//(ie they are very thin and have been broken up into several regions
		//during worm_shape creation
		if (stats.spine_length_to_bitmap_width_ratio < 2){
			//cerr << "Failing on worm_shape length relative to region worm_shape(" << worm_shape.length << ") vs bitmap_width(" << bitmap().properties().width << ")\n";
			failure_reason << "regw(" << ns_to_string_short(worm_shape.length) << ")bw(" << bitmap().properties().width << ")\n";
			return false;
		}
		if (stats.spine_length_to_bitmap_height_ratio < 2){
			failure_reason << "regh(" << ns_to_string_short(worm_shape.length) << ")bl(" << bitmap().properties().height << ")\n";
			//cerr << "Failing on worm_shape length relative to region worm_shape(" << worm_shape.length << ") vs bitmap_length(" << bitmap().properties().height << ")\n";
			return false;
		}
		if (stats.spine_length_to_bitmap_diagonal_ratio < .5){
			failure_reason << "reg_d(" << ns_to_string_short(stats.bitmap_diagonal) << ") sl(" << ns_to_string_short(worm_shape.length) << ")\n";
			//cerr << "Failing on worm_shape length relative to region worm_shape(" << worm_shape.length << ") vs bitmap_length(" << bitmap().properties().height << ")\n";
			return false;
		}



		/*cerr << "w:" << region.worm_shape.primary_segment().length << "\n";
		for (unsigned int i = 0; i < region.worm_shape.primary_segment().nodes.size(); i++){
			cerr << region.worm_shape.primary_segment().nodes[i]->width << ",";
		}
		cerr << "\n";*/
		return true;
	#endif
}

template<class image_type>
inline void ns_fill_circle(const ns_vector_2d & center, const double &radius, image_type & out){
	const unsigned int w(out.properties().width),
					   h(out.properties().height);
	double l = center.x-radius,
		r = center.x+radius,
		b = center.y-radius,
		t = center.y+radius;
	if (l < 0) l = 0;
	else if (l > w-1) l = w-1;
	if (r < 0) r = 0;
	else if (r > w-1) r = w-1;
	if (b < 0) b = 0;
	else if (b > h-1) b = h-1;
	if (t < 0) t = 0;
	else if (t > h-1) t = h-1;
	int li = (int)l,
		ri = (int)ceil(r),
		bi = (int)b,
		ti = (int)ceil(t);

	for (int y = (int)b; y <= (int)t; y++){
		for (int x = (int)l; x <= (int)r; x++)
			if ( (ns_vector_2d(x,y)-center).mag() <= radius)
				out[y][x] = true;
	}
}
template<class image_type>
inline void ns_fill_line(const int & x1, const int & x2, const int & y, image_type & out){
	/*if(x1<0 || x2>=out.properties().width || y<0 || y>=out.properties().height)
		throw ns_ex("ns_fill_line:Invalid line:") << x1 << "," << x2 << "," << y;*/

	int _x1=x1,_x2=x2,_y=y;
	if (_y < 0) _y = 0;
	if ((unsigned int)_y >= out.properties().height)
		_y = out.properties().height-1;
	if (_x1 < 0) _x1 = 0;
	if ((unsigned int)_x1 >= out.properties().width)
		_x1 = out.properties().width-1;
	if (_x2 < 0) _x2 = 0;
	if ((unsigned int)_x2 >= out.properties().width)
		_x2 = out.properties().width-1;

	for (unsigned int i = (unsigned int)_x1; i <= (unsigned int)_x2; i++)
		out[_y][i] = true;
}

//triangle filler algorithm from http://www.geocities.com/wronski12/3d_tutor/tri_fillers.html
template<class image_type>
inline void ns_fill_triangle(const ns_vector_2d & _a, const ns_vector_2d & _b, const ns_vector_2d & _c, image_type & out){
	
	if (_a == _b || _b == _c || _a == _c ||
		_a.x == _b.x && _a.x == _c.x || 
		_a.y == _b.y && _a.x == _c.y)
		return;
	ns_vector_2d a,b,c;
	//sort by y position
	if		(_a.y <= _b.y && _b.y <= _c.y)		{a = _a; b = _b; c = _c;}
	else if (_a.y <= _c.y && _c.y <= _b.y)		{a = _a; b = _c; c = _b;}
	else if (_b.y <= _a.y && _a.y <= _c.y)		{a = _b; b = _a; c = _c;}
	else if (_b.y <= _c.y && _c.y <= _a.y)		{a = _b; b = _c; c = _a;}
	else if (_c.y <= _a.y && _a.y <= _b.y)		{a = _c; b = _a; c = _b;}
	else/* if (_c.y <= _b.y && _b.y <= _a.y)*/	{a = _c; b = _b; c = _a;}
//try{
	ns_vector_2d S, E;
	double dx1,dx2,dx3;
	if (b.y-a.y > 0) dx1=(b.x-a.x)/(b.y-a.y); else dx1=b.x - a.x;
	if (c.y-a.y > 0) dx2=(c.x-a.x)/(c.y-a.y); else dx2=0;
	if (c.y-b.y > 0) dx3=(c.x-b.x)/(c.y-b.y); else dx3=0;

	S=E=a;
	if(dx1 > dx2) {
		for(;S.y<=b.y;S.y++,E.y++,S.x+=dx2,E.x+=dx1)
			ns_fill_line((int)S.x,(int)E.x,(int)S.y,out);
		E=b;
		for(;S.y<=c.y;S.y++,E.y++,S.x+=dx2,E.x+=dx3)
			ns_fill_line((int)S.x,(int)E.x,(int)S.y,out);
	} else {
		for(;S.y<=b.y;S.y++,E.y++,S.x+=dx1,E.x+=dx2)
			ns_fill_line((int)S.x,(int)E.x,(int)S.y,out);
		S=b;
		for(;S.y<=c.y;S.y++,E.y++,S.x+=dx3,E.x+=dx2)
			ns_fill_line((int)S.x,(int)E.x,(int)S.y,out);
	}
//	}
//	catch(ns_ex & ex){
	//	throw ns_ex(ex.text()) << "\nusing points " << _a.x << "," << _a.y << " "<< _b.x << "," << _b.y << " "<< _c.x << "," << _c.y << "\n"
	//												<< a.x << "," << a.y << " "<< b.x << "," << b.y << " "<< c.x << "," << c.y << " ";
	//}
}




void ns_worm_collage_storage::clear(){
	for (unsigned int i = 0; i < context_images.size(); i++)
		ns_safe_delete(context_images[i]);
	for (unsigned int i = 0; i < absolute_region_images.size(); i++)
		ns_safe_delete(absolute_region_images[i]);
	for (unsigned int i = 0; i < relative_region_images.size(); i++)
		ns_safe_delete(relative_region_images[i]);
	for (unsigned int i = 0; i < bitmaps.size(); i++)
		ns_safe_delete(bitmaps[i]);
	context_images.resize(0);
	absolute_region_images.resize(0);
	relative_region_images.resize(0);
	bitmaps.resize(0);
	collage_cache.clear();
}
		
void ns_worm_collage_storage::specifiy_region_sizes(const std::vector<ns_detected_worm_info> & worms){
	region_image_sizes.resize(worms.size());
	region_offsets_in_context_image.resize(worms.size());
	for (unsigned long i = 0; i < worms.size(); i++){
		region_image_sizes[i] = worms[i].region_size;
		region_offsets_in_context_image[i] = worms[i].region_position_in_source_image - worms[i].context_position_in_source_image;

	}
}

const ns_image_standard & ns_worm_collage_storage::generate_collage(const ns_image_standard & absolute_grayscale,const ns_image_standard & relative_grayscale,const ns_image_standard & threshold,const std::vector<ns_detected_worm_info *> & worms){
	if (collage_cache.properties().height != 0){
		return collage_cache;
	}
	std::vector<ns_image_standard > images(worms.size());
	for (unsigned int i = 0; i < worms.size(); i++){
		ns_image_properties prop(relative_grayscale.properties());
		prop.height = worms[i]->region_size.y + 2*context_border_size().y;
		prop.width = worms[i]->region_size.x + 2*context_border_size().x;
		prop.components = 3;
		images[i].init(prop);
		ns_vector_2i tl(worms[i]->region_position_in_source_image - context_border_size());
		ns_vector_2i br(tl+worms[i]->region_size + context_border_size()*2);
		//find cropped
		ns_vector_2i tl_c((tl.x>0)?tl.x:0,
						  (tl.y>0)?tl.y:0),
					 br_c((br.x<relative_grayscale.properties().width)?br.x:(relative_grayscale.properties().width),
						  (br.y<relative_grayscale.properties().height)?br.y:(relative_grayscale.properties().height));
		ns_vector_2i tl_gap(tl_c-tl),
					 br_gap(br-br_c);
					
		//top gap
		for (long y = 0; y < tl_gap.y; y++)
			for (long x = 0; x < 3*prop.width; x++)
				images[i][y][x] = 0;

		for (long y = tl_c.y; y < br_c.y; y++){
			//left gap
			for (long x = 0; x < 3*tl_gap.x; x++)
				images[i][y-tl.y][x] = 0;
			for (long x = tl_c.x; x < br_c.x; x++){
				images[i][y-tl.y][3*(x-tl.x)+0]=absolute_grayscale[y][x];
				images[i][y-tl.y][3*(x-tl.x)+1]=relative_grayscale[y][x];
				images[i][y-tl.y][3*(x-tl.x)+2]=NS_REGION_VIS_ALL_THRESHOLDED_OBJECTS_VALUE*(threshold[y][x]>0);
			}
			//right gap
			for (long x = 3*(br_c.x-tl.x); x < 3*prop.width; x++)
				images[i][y-tl.y][x] = 0;
		}
		//bottom gap
		for (long y = br_c.y-tl.y; y < prop.height; y++)
			for (long x = 0; x < 3*prop.width; x++)
				images[i][y][x] = 0;

	//in channel three, a value of 70 indicates the pixel is in the worm cluster
	//a value of of 255 indicates the pixel is in the specific worm
		for (unsigned int y = 0; y < worms[i]->region_size.y; y++){
			for (unsigned int x = 0; x < worms[i]->region_size.x; x++){
				if (worms[i]->bitmap()[y][x])
					images[i][y+context_border_size().y][3*(x+context_border_size().x)+2] = NS_REGION_VIS_WORM_THRESHOLD_VALUE;
		//		else if (worms[i]->bitmap_of_worm_cluster()[y][x])
		//			images[i][y+context_border_size().y][3*(x+context_border_size().x)] = 70;
			//	else images[i][y+context_border_size().y][3*(x+context_border_size().x)] = 0;
			}
		}
	}

	collage_info = ns_make_collage(images, collage_cache, 128);
	return collage_cache;
}

//void ns_worm_collage_storage::populate_worms_from_db(ns_image_server_captured_image_region & region,std::vector<ns_detected_worm_info> & worms, ns_sql & sql,const bool interpolated){
void ns_worm_collage_storage::load_images_from_db(ns_image_server_captured_image_region & region,const unsigned long expected_number_of_worms,ns_sql & sql,const bool interpolated, const bool only_load_absolute_grayscale){
	
	//interpolated worm region images are stored in a different record than detected regions
	ns_processing_task region_task;
	if (interpolated) region_task = ns_process_region_interpolation_vis;
	else			  region_task = ns_process_region_vis;

	ns_image_server_image im(region.request_processed_image(region_task,sql));
	ns_image_standard worm_collage;
	
	std::vector<ns_image_standard *> images;
	try{
		ns_image_storage_source_handle<ns_8_bit> image = image_server.image_storage.request_from_storage(im,&sql);
		image.input_stream().pump(worm_collage,512);
		ns_extract_images_from_collage(collage_info, worm_collage, images);
		worm_collage.clear();
	}
	catch(ns_ex & ex){
		sql << "DELETE FROM images WHERE id = " << im.id;
		sql.send_query();
		sql << "UPDATE sample_region_images SET " << ns_processing_step_db_column_name(region_task) << "=0, "
			<< ns_processing_step_db_column_name(ns_process_worm_detection) << "=0, "
			<< ns_processing_step_db_column_name(ns_process_worm_detection_labels) << "=0 WHERE id=" << region.region_images_id;
		sql.send_query();
		throw ex;
	}	
	try{

		//For example, 7 worms cannot be evenly divided into multiple rows of multiple worms.
		//Thus, the last row will have to have less worms than those above it.
		//Thus, there will be at least one "empty" block in the worm grid that will show up as a worm
		//after extraction, but not have a matching worm_shape.
		//We can handle this.
		if (images.size() > expected_number_of_worms){
			if (images.size() - expected_number_of_worms >= collage_info.tiles_per_row)
				throw ns_ex("ns_image_worm_detection_results::Region bitmap contains too many images.");
			for (unsigned int i = (unsigned int)expected_number_of_worms; i < (unsigned int)images.size(); i++){
				delete images[i];
				images[i] = 0;
			}
			images.resize(expected_number_of_worms);
		}
		if (images.size() < expected_number_of_worms)
			throw ns_ex("ns_image_worm_detection_results::Region bitmap does not contain enough images to supply all putative worms!");

		if (images.size() != 0 && region_image_sizes.size() == 0)
			throw ns_ex("ns_worm_collage_storage::load_images_from_db()::Region sizes have not been specified. Call specify_region_sizes()");

		if (images.size() == 0)
			return;

		if (images[0]->properties().components != 3)
			throw ns_ex("ns_worm_collage_storage::load_images_from_db()::Encountered a black and white collage when RGB is required.");

		
		context_images.resize(images.size(),0);
		relative_region_images.resize(images.size(),0);
		absolute_region_images.resize(images.size(),0);
		bitmaps.resize(images.size(),0);
		//go through and extract region and context images
		for (unsigned int i = 0; i < images.size(); i++){
			const ns_vector_2i context_size(context_image_sizes(i));
			if (context_size.x > (int)images[i]->properties().width || context_size.y > (int)images[i]->properties().height)
				throw ns_ex("ns_worm_collage_storage::load_images_from_db::Region image (") << images[i]->properties().width << "," << images[i]->properties().height <<
					") is too small to represent the worm (" << context_size.x << "," << context_size.y << ")";

			//copy over context image
			ns_image_properties prop(images[i]->properties());
			prop.components = 1;
			prop.width = context_size.x;
			prop.height = context_size.y;
			context_images[i] = new ns_worm_context_image;
			context_images[i]->absolute_grayscale.init(prop);
			if (!only_load_absolute_grayscale){
				context_images[i]->relative_grayscale.init(prop);
				for (unsigned int y = 0; y < prop.height; y++){
					for (unsigned int x = 0; x < prop.width; x++){
						context_images[i]->absolute_grayscale[y][x] = (*images[i])[y][3*x];
						context_images[i]->relative_grayscale[y][x] = (*images[i])[y][3*x+1];
					}
				}
			}
			else{
				for (unsigned int y = 0; y < prop.height; y++){
					for (unsigned int x = 0; x < prop.width; x++){
						context_images[i]->absolute_grayscale[y][x] = (*images[i])[y][3*x];
					}
				}
			}
			if (!only_load_absolute_grayscale){

				//copy over region image
				prop.width = region_image_sizes[i].x;
				prop.height = region_image_sizes[i].y;
				absolute_region_images[i] = new ns_image_standard;
				relative_region_images[i] = new ns_image_standard;
				bitmaps[i] = new ns_image_standard;
				absolute_region_images[i]->init(prop);
				relative_region_images[i]->init(prop);
				bitmaps[i]->init(prop);
				//use red chanel (precalculated-threshold) to mask green channel (grayscale value of image)
				for (unsigned int y = 0; y < prop.height; y++){
					for (unsigned int x = 0; x < prop.width; x++){
						(*absolute_region_images[i])[y][x] = (*images[i])[y+region_offsets_in_context_image[i].y][3*(x+region_offsets_in_context_image[i].x)];
						(*relative_region_images[i])[y][x] = (*images[i])[y+region_offsets_in_context_image[i].y][3*(x+region_offsets_in_context_image[i].x)+1];
						(*bitmaps[i])[y][x] =				 (*images[i])[y+region_offsets_in_context_image[i].y][3*(x+region_offsets_in_context_image[i].x)+2];

					}
				}
			}
			delete images[i];
			images[i] = 0;
		}
	}
	catch(...){
		for (unsigned int i = 0; i < images.size(); i++){
			ns_safe_delete(images[i]);
		}
			
		for (unsigned int i = 0; i < context_images.size(); i++){
			ns_safe_delete(context_images[i]);
			ns_safe_delete(absolute_region_images[i]);
			ns_safe_delete(relative_region_images[i]);
			ns_safe_delete(bitmaps[i]);
		}
		throw;
	}
}

void ns_worm_collage_storage::populate_worm_images(std::vector<ns_detected_worm_info> & worms,const bool interpolated, const bool  only_load_context_absolute_grayscle){
	if (context_images.size() != worms.size())
		throw ns_ex("ns_worm_collage_storage::populate_worm_images()::The number of worms in collage and worms[] does not match");
	
	for (unsigned int i = 0; i < worms.size(); i++){
		context_images[i]->absolute_grayscale.pump(worms[i].context_image().absolute_grayscale,1024);
		if (only_load_context_absolute_grayscle)
			continue;
		context_images[i]->relative_grayscale.pump(worms[i].context_image().relative_grayscale,1024);
		absolute_region_images[i]->pump(worms[i].absolute_grayscale(),1024);
		relative_region_images[i]->pump(worms[i].relative_grayscale(),1024);

		ns_image_properties s(bitmaps[i]->properties());
		worms[i].bitmap().prepare_to_recieve_image(s);
		worms[i].bitmap_of_worm_cluster().prepare_to_recieve_image(s);
		for (unsigned int y = 0; y < s.height; y++){
			for (unsigned int x = 0; x < s.width; x++){
				worms[i].bitmap()[y][x] = (*bitmaps[i])[y][x] ==255;
				worms[i].bitmap_of_worm_cluster()[y][x] = (*bitmaps[i])[y][x] >=70;
			}
		}
		ns_create_edge_bitmap(worms[i].bitmap(), worms[i].edge_bitmap());
	}
}



void ns_image_worm_detection_results::load_images_from_db(ns_image_server_captured_image_region & region, ns_sql & sql, bool interpolated, bool only_load_context_absolute_grayscle){
	worm_collage.specifiy_region_sizes(putative_worms);
	worm_collage.load_images_from_db(region,putative_worms.size(),sql,interpolated, only_load_context_absolute_grayscle);
	worm_collage.populate_worm_images(putative_worms,interpolated,only_load_context_absolute_grayscle);
}


void ns_image_worm_detection_results::add_data_to_db_query(ns_sql & sql){
	throw ns_ex("This code hasn't been maintained.");
	//calculate the number of nodes in each worm_shape, as well as the total number of nodes in the image.
	sql << "worm_segment_node_counts='";
	unsigned int total_length = 0;
	std::vector<unsigned int> worm_segment_node_counts(actual_worms.size());
	for (unsigned int i = 0; i < actual_worms.size(); i++){
		worm_segment_node_counts[i] = static_cast<unsigned int>(actual_worms[i]->worm_shape.nodes.size());
		total_length += static_cast<unsigned int>(actual_worms[i]->worm_shape.nodes.size());
	}
	if (worm_segment_node_counts.size() != 0)
		sql.write_data(reinterpret_cast<char *>(&worm_segment_node_counts[0]),static_cast<unsigned int>(actual_worms.size())*sizeof(unsigned int)/sizeof(char));
	sql << "', ";

	//build the worm_shape data buffer.
	sql << "worm_segment_information='";
	std::vector<float> spine_info(6*total_length);
	unsigned int nodes_written = 0;
	for (unsigned int i = 0; i < actual_worms.size(); i++){
		actual_worms[i]->worm_shape.write_to_buffer(&spine_info[0],nodes_written);
		nodes_written += worm_segment_node_counts[i];
	}
	if (spine_info.size() != 0 )
		sql.write_data(reinterpret_cast<char *>(&spine_info[0]),static_cast<unsigned int>(spine_info.size())*sizeof(float)/sizeof(char));
	sql << "', ";

	//write the region position_in_source_image region info
	        
	sql << "worm_region_information='";
	std::vector<unsigned int> worm_region_info(4*actual_worms.size());
	for (unsigned int i = 0; i < actual_worms.size(); i++){
		worm_region_info[4*i] = actual_worms[i]->region_position_in_source_image.x;
		worm_region_info[4*i+1] = actual_worms[i]->region_position_in_source_image.y;
		worm_region_info[4*i+2] = actual_worms[i]->region_size.x;
		worm_region_info[4*i+3] = actual_worms[i]->region_size.y;
	}
	if (worm_region_info.size() != 0)
		sql.write_data(reinterpret_cast<char *>(&worm_region_info[0]),static_cast<unsigned int>(worm_region_info.size())*sizeof(unsigned int)/sizeof(char));
	sql << "', ";

	//cerr << "outputting " << interpolated_worm_areas.size() << "\n";

	sql << "interpolated_worm_areas = '";

	std::vector<unsigned int> interpolated_areas(4*interpolated_worm_areas.size());
	for (unsigned int i = 0; i < interpolated_worm_areas.size(); i++){
		interpolated_areas[4*i+0] = interpolated_worm_areas[i].position_in_source_image.x;
		interpolated_areas[4*i+1] = interpolated_worm_areas[i].position_in_source_image.y;
		interpolated_areas[4*i+2] = interpolated_worm_areas[i].size.x;
		interpolated_areas[4*i+3] = interpolated_worm_areas[i].size.y;
	}
	if (interpolated_areas.size() != 0)
		sql.write_data(reinterpret_cast<char *>(&interpolated_areas[0]),static_cast<unsigned int>(interpolated_areas.size())*sizeof(unsigned int)/sizeof(char));
	sql << "', ";

}



void ns_image_worm_detection_results::save(ns_image_server_captured_image_region & region, const bool interpolated, ns_sql & sql, const bool calculate_stats) {

	ns_image_statistics stats;
	stats.db_id = 0;
	if (calculate_stats) {
		//save a summary of the current image to the image_statistics table
		ns_summarize_stats(actual_worm_list(), stats.worm_statistics);

		sql << "SELECT image_statistics_id FROM sample_region_images WHERE id= " << region.region_images_id;
		ns_sql_result res;
		sql.get_rows(res);
		if (res.size() == 0)
			stats.db_id = 0;
		else stats.db_id = ns_atoi64(res[0][0].c_str());
		stats.submit_to_db(stats.db_id, sql, false, true);
	}

	ns_sql_result res;
	

	//find worm_detection_results record.  If it doesn't exist, make a new one.
	sql << "SELECT worm_detection_results_id, worm_interpolation_results_id FROM sample_region_images WHERE id = " << region.region_images_id;
	sql.get_rows(res);
	if (res.size() != 0) {
		if (!interpolated)	detection_results_id = atol(res[0][0].c_str());
		else				detection_results_id = atol(res[0][1].c_str());
	}
	else detection_results_id = 0;

	if (detection_results_id != 0) {

		sql << "SELECT data_storage_on_disk_id FROM worm_detection_results WHERE id = " << detection_results_id;
		ns_sql_result res2;
		sql.get_rows(res2);
		if (res2.size() == 0)
			data_storage_on_disk.id = 0;
		else data_storage_on_disk.id = ns_atoi64(res2[0][0].c_str());
	}

	region.create_storage_for_worm_results(data_storage_on_disk, interpolated, sql);
	if (detection_results_id != 0) {
		sql << "UPDATE worm_detection_results SET ";
	}
	else {
		data_storage_on_disk.id = 0;
		sql << "INSERT INTO worm_detection_results SET ";
	}

	if (calculate_stats)
		sql << "image_statistics_id=" << stats.db_id << ",";

	sql << "source_image_id = " << source_image_id << ", "
		<< "capture_sample_id = " << capture_sample_id << ", number_of_worms=" << static_cast<unsigned int>(actual_worms.size()) << ", "
		<< "number_of_interpolated_worm_areas = " << (unsigned int)interpolated_worm_areas.size() << ", ";
	sql << "data_storage_on_disk_id=" << data_storage_on_disk.id << ",";

	//write bitmap info
	sql << "bitmap_tiles_per_row = " << worm_collage.info().tiles_per_row << ", "
		<< "bitmap_tile_width = " << worm_collage.info().tile_width << ", "
		<< "bitmap_tile_height = " << worm_collage.info().tile_height;
	sql << ", ";

	//write movement tags
	sql << "worm_movement_tags='";
	string movement_tags;
	for (unsigned int i = 0; i < actual_worms.size(); i++) {
		movement_tags += ns_to_string((unsigned long)actual_worms[i]->movement_state);
		if (i + 1 <  actual_worms.size())
			movement_tags += ",";
	}
	sql << movement_tags << "' ,";
	// Now provide empty values for the required-though-deprecated blob columns:
	sql << "worm_segment_node_counts='', worm_segment_information='', worm_region_information='', worm_fast_movement_mapping=''"
		<< ", worm_slow_movement_mapping='', worm_movement_state='', worm_movement_fast_speed='', worm_movement_slow_speed=''"
		<< ", interpolated_worm_areas=''";

	if (res.size() != 0)
		detection_results_id = sql.send_query_get_id();
	else {
		sql << " WHERE id=" << detection_results_id;
		sql.send_query();
	}

	//update the region record to reflect the new results
	sql << "UPDATE sample_region_images SET ";
	if (!interpolated) sql << "worm_detection_results_id=";
	else			   sql << "worm_interpolation_results_id=";
	sql << detection_results_id << " WHERE id = " << region.region_images_id;
	sql.send_query();

	//OK! Now we have all the database records set.
	//we just need to output everything to disk.

	ns_acquire_for_scope<ofstream> outfile(image_server.image_storage.request_metadata_output(data_storage_on_disk, ns_wrm, true, &sql));
	if (outfile().fail())
		throw ns_ex("Could not make storage for region data file.");
	save_data_to_disk(outfile(), interpolated, sql);
	outfile().close();
	outfile.release();

}
void ns_image_worm_detection_results::save_data_to_disk(std::ofstream & out, const bool interpolated, ns_sql & sql){


		
	out << actual_worms.size() << "," << interpolated_worm_areas.size() << "\n";
		
	//write the region offset region info
	if (actual_worms.size() != 0){

		for (unsigned int i = 0; i < actual_worms.size(); i++){
			if (actual_worms[i]->context_image_size.x == 0)
				throw ns_ex("Empty context Image!");
			out << actual_worms[i]->region_position_in_source_image.x << ","
				<< actual_worms[i]->region_position_in_source_image.y << ","
				<< actual_worms[i]->context_position_in_source_image.x << ","
				<< actual_worms[i]->context_position_in_source_image.y << ","
				<< actual_worms[i]->region_size.x << ","
				<< actual_worms[i]->region_size.y << ","
				<< actual_worms[i]->context_image_size.x << ","
				<< actual_worms[i]->context_image_size.y << "\n";
		
		}
	}
	
	if (interpolated_worm_areas.size() != 0){
		for (unsigned int i = 0; i < interpolated_worm_areas.size(); i++){
			out << interpolated_worm_areas[i].position_in_source_image.x << ","
				<< interpolated_worm_areas[i].position_in_source_image.y << ","
				<< interpolated_worm_areas[i].size.x << ","
				<< interpolated_worm_areas[i].size.y << "\n";
		}
	}


	//std::vector<unsigned int> worm_segment_node_counts(actual_worms.size());
	unsigned int total_length = 0;
	if (actual_worms.size()!= 0){
		for (unsigned int i = 0; i < actual_worms.size(); i++){
			out << actual_worms[i]->worm_shape.nodes.size() << ",";
		
			total_length += static_cast<unsigned int>(actual_worms[i]->worm_shape.nodes.size());
		}
	
		out << "\n";
	}
	//output spine information
	if (total_length != 0){
	
		unsigned int nodes_written = 0;
		for (unsigned int i = 0; i < actual_worms.size(); i++){
			actual_worms[i]->worm_shape.write_to_csv(out);
			out << "\n";
			
		}

	}
}

void ns_image_worm_detection_results::load_from_db(const bool load_worm_postures,const bool images_comes_from_interpolated_annotations,ns_sql & sql,const bool delete_from_db_on_error){
	if (detection_results_id == 0)
		throw ns_ex("ns_image_worm_detection_results::Attempting to load from db with id=0.");
	sql << "SELECT source_image_id, capture_sample_id, bitmap_tiles_per_row, bitmap_tile_width, bitmap_tile_height, "
		<< "number_of_worms, worm_movement_tags,data_storage_on_disk_id,number_of_interpolated_worm_areas "
		<< "FROM worm_detection_results WHERE id = " << detection_results_id;
	ns_sql_result data;
	sql.get_rows(data);
	if (data.size() == 0){
		throw ns_ex("ns_image_worm_detection_resuls::Could not load specified result") << detection_results_id <<", as it does not exist in the db";
	}
	source_image_id = atol(data[0][0].c_str());
	capture_sample_id = atol(data[0][1].c_str());
	worm_collage.info().tiles_per_row = atol(data[0][2].c_str());
	worm_collage.info().tile_width = atol(data[0][3].c_str());
	worm_collage.info().tile_height = atol(data[0][4].c_str());

	unsigned int number_of_worms = atol(data[0][5].c_str()),
				 number_of_interpolated_worm_areas = atol(data[0][8].c_str());
	
	data_storage_on_disk.id = atol(data[0][7].c_str());

	if (data_storage_on_disk.id == 0){
		throw ns_ex("Database storage of worm information is depreciated!");
		
	}
	else{
		//load from disk
		ns_acquire_for_scope<ifstream> in;

		try{
			in.attach(image_server.image_storage.request_metadata_from_disk(data_storage_on_disk,true,&sql));
		}
		catch(...){
			if (delete_from_db_on_error){
				sql << "UPDATE sample_region_images SET " << ns_processing_step_db_column_name(	ns_process_worm_detection) << "=0,"
					<< ns_processing_step_db_column_name(ns_process_worm_detection_labels) << "=0,"
					<< ns_processing_step_db_column_name(ns_process_worm_detection_with_graph) << "=0,"
					<< ns_processing_step_db_column_name(ns_process_region_vis) << "=0,"
					<< ns_processing_step_db_column_name(ns_process_region_interpolation_vis) << "=0,"
					<< "worm_detection_results_id = 0 WHERE id = " << source_image_id;
				sql.send_query();
				sql << "DELETE FROM worm_detection_results WHERE id = " << detection_results_id;
				sql.send_query();
			}
			throw;
		}
		//this->capture_time = data_storage_on_disk.capture_time;
		unsigned long file_number_of_worms, file_number_of_interpolated_worms;
		if (!ns_read_csv_value(in(),file_number_of_worms))
					throw ns_ex("ns_image_worm_detection_results::load_from_disk()::Malformed file");
		if (!ns_read_csv_value(in(),file_number_of_interpolated_worms))
					throw ns_ex("ns_image_worm_detection_results::load_from_disk()::Malformed file");
		if (number_of_worms != file_number_of_worms)
			throw ns_ex("ns_image_worm_detection_results::load_from_db()::File specified incorrect number of worms: ") << file_number_of_worms << "; db specifies " << number_of_worms;
		if (file_number_of_interpolated_worms != number_of_interpolated_worm_areas)
			throw ns_ex("ns_image_worm_detection_results::load_from_db()::File specified incorrect number of interpolated worms: ") << file_number_of_interpolated_worms << "; db specifies " << number_of_interpolated_worm_areas << "(" << data_storage_on_disk.filename << ")";
		
		putative_worms.resize(number_of_worms);
		actual_worms.resize(putative_worms.size());
		
		if (number_of_worms != 0){
		
			for (unsigned int i = 0; i < putative_worms.size(); i++){
				
				putative_worms[i].interpolated = images_comes_from_interpolated_annotations;
				actual_worms[i] = &putative_worms[i];
				if (!ns_read_csv_value(in(),putative_worms[i].region_position_in_source_image.x))
					throw ns_ex("ns_image_worm_detection_results::load_from_disk()::Malformed file");
				if (!ns_read_csv_value(in(),putative_worms[i].region_position_in_source_image.y))
					throw ns_ex("ns_image_worm_detection_results::load_from_disk()::Malformed file");	
				if (!ns_read_csv_value(in(),putative_worms[i].context_position_in_source_image.x))
					throw ns_ex("ns_image_worm_detection_results::load_from_disk()::Malformed file");
				if (!ns_read_csv_value(in(),putative_worms[i].context_position_in_source_image.y))
					throw ns_ex("ns_image_worm_detection_results::load_from_disk()::Malformed file");
				if (!ns_read_csv_value(in(),putative_worms[i].region_size.x))
					throw ns_ex("ns_image_worm_detection_results::load_from_disk()::Malformed file");
				if (!ns_read_csv_value(in(),putative_worms[i].region_size.y))
					throw ns_ex("ns_image_worm_detection_results::load_from_disk()::Malformed file");
				if (!ns_read_csv_value(in(),putative_worms[i].context_image_size.x))
					throw ns_ex("ns_image_worm_detection_results::load_from_disk()::Malformed file");
				if (!ns_read_csv_value(in(),putative_worms[i].context_image_size.y))
					throw ns_ex("ns_image_worm_detection_results::load_from_disk()::Malformed file");
				if (putative_worms[i].region_size.x == 0||
					putative_worms[i].region_size.y == 0||
					putative_worms[i].context_image_size.x == 0||
					putative_worms[i].context_image_size.y == 0)
					throw ns_ex("Found an empty region!");

			}
		}

		interpolated_worm_areas.resize(number_of_interpolated_worm_areas);
		if (interpolated_worm_areas.size() != 0){
			for (unsigned int i = 0; i < number_of_interpolated_worm_areas; i++){
				if (!ns_read_csv_value(in(),interpolated_worm_areas[i].position_in_source_image.x))
					throw ns_ex("ns_image_worm_detection_results::load_from_disk()::Malformed file");
				if (!ns_read_csv_value(in(),interpolated_worm_areas[i].position_in_source_image.y))
					throw ns_ex("ns_image_worm_detection_results::load_from_disk()::Malformed file");
				if (!ns_read_csv_value(in(),interpolated_worm_areas[i].size.x))
					throw ns_ex("ns_image_worm_detection_results::load_from_disk()::Malformed file");
				if (!ns_read_csv_value(in(),interpolated_worm_areas[i].size.y))
					throw ns_ex("ns_image_worm_detection_results::load_from_disk()::Malformed file");
			}
		}

		for (unsigned int i = 0; i < actual_worms.size(); i++){
			actual_worms[i]->movement_state = ns_movement_not_calculated;
		}


		if (!load_worm_postures){
			in().close();
			in.release();
			return;
		}

		unsigned long total_node_length(0);
		std::vector<unsigned long> worm_segment_node_counts(number_of_worms);
		if (number_of_worms != 0){
			for (unsigned int i = 0; i < number_of_worms; i++){
				if (!ns_read_csv_value(in(),worm_segment_node_counts[i]))
					throw ns_ex("ns_image_worm_detection_results::load_from_disk()::Malformed file");
				total_node_length+=worm_segment_node_counts[i];
			}
		}

		//output code generates spurious comma
		in().get();
		if (total_node_length != 0){
		
			for (unsigned int i = 0; i < number_of_worms; i++){
				putative_worms[i].worm_shape.read_from_csv(in(),worm_segment_node_counts[i]);
				//output code generates spurious comma
				in().get();
				if (putative_worms[i].worm_shape.nodes.size() == 0)
					throw ns_ex("ns_image_worm_detection_results::load_from_db()::Encountered empty worm shape!");
				for (unsigned int j = 0; j < putative_worms[i].worm_shape.nodes.size(); j++){
					if (putative_worms[i].worm_shape.nodes[j].x >= putative_worms[i].region_size.x)
						throw ns_ex("ns_image_worm_detection_results::load_from_db()::Encountered an out of band node!");
					if (putative_worms[i].worm_shape.nodes[j].y >= putative_worms[i].region_size.y)
						throw ns_ex("ns_image_worm_detection_results::load_from_db()::Encountered an out of band node!");

				}
			}
		}
		if (in().fail())
			throw ns_ex("Improper file format: error at EOF");
		in().close();
		in.release();
		
	}

	//if movement tags are unspecified, assume not_assigned.
	if (data[0][6].size() != 0){

		unsigned long current_worm(0);
		string cur_val;
		for (unsigned int i = 0; i < data[0][6].size(); i++){
			if (current_worm >= actual_worms.size())
				throw ns_ex("ns_image_worm_detection_results::Too many worm movement tags specified");
			if (data[0][6][i]==','){
				if (cur_val.size() == 0)
					throw ns_ex("ns_image_worm_detection_results::No value specified for worm ") << current_worm;
				actual_worms[current_worm]->movement_state = (ns_movement_state)atol(cur_val.c_str());
				cur_val.resize(0);
				current_worm++;
				continue;
			}
			cur_val+=data[0][6][i];	
		}
		if (cur_val.size() != 0){
			actual_worms[current_worm]->movement_state = (ns_movement_state)atol(cur_val.c_str());
			current_worm++;
		}

		if (current_worm < number_of_worms)
			throw ns_ex("ns_image_worm_detection_results::Worm movement tags contained only ") << current_worm << " values, whereas "
				<< " there are " << number_of_worms << " worms in the image.";
	}
}

void ns_whole_image_region_intensity_stats::calculate(const ns_image_standard & im,const bool ignore_zero){
	//calculate histogram of intensities
	ns_histogram<unsigned long long,ns_8_bit> histogram;
	for (unsigned int y = 0; y < im.properties().height; ++y){
		for (unsigned int x = 0; x < im.properties().width; ++x){
			histogram.increment(im[y][x]);
		}
	}
	maximum_intensity = histogram.average_of_ntile(9,10,ignore_zero);  //mean of 90th percentile
	minimum_intensity = histogram.average_of_ntile(0,10,ignore_zero);  //mean of 10th percentile
	average_intensity = histogram.mean(ignore_zero);
}

//In order to generate "gold standard" sets from which to train the machine learning module
//objects must be sorted into worm/non-worm categories.
//generate_context_image() produces an image that can be easily sorted into worm/non-worm categories.

void ns_detected_worm_info::generate_training_set_visualization(ns_image_standard & output) const{
	_worm_context_image->generate(part_of_a_multiple_worm_cluster,get_region_offset_in_context_image(),
								_worm_context_image->relative_grayscale,_worm_context_image->absolute_grayscale,
								bitmap_of_worm_cluster(),bitmap(),output);
}
void ns_worm_context_image::generate(const bool part_of_a_worm_cluster,const ns_vector_2i & worm_cluster_bitmap_offset_in_context_image,
						const ns_image_standard & relative_grayscale, const ns_image_standard & absolute_grayscale,
								  const ns_image_bitmap & worm_cluster_bitmap, const ns_image_bitmap & worm_bitmap, ns_image_standard & output){

	ns_image_properties p = absolute_grayscale.properties();
//	cout << p.width << "," << p.height << "\n";
	p.components = 3;
	output.prepare_to_recieve_image(p);
	
	for (unsigned int y = 0; y < p.height; y++){
		for (unsigned int x = 0; x < p.width; x++){
			output[y][3*x+0] = (255-relative_grayscale[y][x])>>1 | 0x80; //stick everything but the lowest bit into channel 0
			output[y][3*x+1] = (((255-relative_grayscale[y][x])>>1) & 0xFE) | ((255-relative_grayscale[y][x]) & 0x01) | 0x80;  //stick the lowest bit into channel 1
			output[y][3*x+2] = absolute_grayscale[y][x];
		
			
		}
	}

	//output threshold information
	//output both the threshold of the chosen region and of the complex worm cluster it was solved from.

	for (unsigned int y = 0; y < worm_bitmap.properties().height; y++){
		for (unsigned int x = 0; x < worm_bitmap.properties().width; x++){
			bool wc = (!part_of_a_worm_cluster && worm_bitmap[y][x]) || worm_cluster_bitmap[y][x];
			bool w = worm_bitmap[y][x];
			if (!(wc^w))
				output[worm_cluster_bitmap_offset_in_context_image.y + y][3*(x+worm_cluster_bitmap_offset_in_context_image.x)] |= 0x80;
			else output[worm_cluster_bitmap_offset_in_context_image.y + y][3*(x+worm_cluster_bitmap_offset_in_context_image.x)] &= 0x7F;
			if (!worm_bitmap[y][x])
				output[worm_cluster_bitmap_offset_in_context_image.y + y][3*(x+worm_cluster_bitmap_offset_in_context_image.x)+1] |= 0x80;
			else output[worm_cluster_bitmap_offset_in_context_image.y + y][3*(x+worm_cluster_bitmap_offset_in_context_image.x)+1] &= 0x7F;

		}
	}
	
	ns_worm_context_image im;
	output.pump(im.combined_image,1024);
	if (0){
		ns_detected_worm_info info;
		info.part_of_a_multiple_worm_cluster = part_of_a_worm_cluster;

		info.from_training_set_visualization(im, ns_vector_2i(worm_bitmap.properties().width,worm_bitmap.properties().height),worm_cluster_bitmap_offset_in_context_image,ns_vector_2i(im.combined_image.properties().width,im.combined_image.properties().height));

		if (absolute_grayscale.properties() != info.context_image().absolute_grayscale.properties())
			throw ns_ex("Properties Mismatch!");
		for (unsigned long y = 0; y < absolute_grayscale.properties().height;y++){
			for (unsigned long x = 0; x < absolute_grayscale.properties().width; x++){
				if (info.context_image().absolute_grayscale[y][x] != absolute_grayscale[y][x])
					throw ns_ex("ns_worm_context_image::generate(): Invalid pixel found in absolute context image:") << (int)info.context_image().absolute_grayscale[y][x] << " vs " << (int)absolute_grayscale[y][x];
				if (info.context_image().relative_grayscale[y][x] != relative_grayscale[y][x])
					throw ns_ex("ns_worm_context_image::generate(): Invalid pixel found in relative context image:") << info.context_image().relative_grayscale[y][x] << " vs " << (int)relative_grayscale[y][x];
			}
		}	
		if (worm_bitmap.properties() != info.bitmap().properties())
			throw ns_ex("Properties Mismatch!");
		for (unsigned long y = 0; y < worm_bitmap.properties().height;y++){
			for (unsigned long x = 0; x < worm_bitmap.properties().width; x++){
				if (info.bitmap()[y][x] != worm_bitmap[y][x])
					throw ns_ex("ns_worm_context_image::generate(): Invalid pixel found in context image bitmap.");
				if (info.bitmap_of_worm_cluster()[y][x] != worm_cluster_bitmap[y][x])
					throw ns_ex("ns_worm_context_image::generate(): Invalid pixel found in context image bitmap.");
			}
		}
	}
}

void ns_worm_context_image::split(ns_image_standard & relative_grayscale, ns_image_standard & absolute_grayscale,
								  ns_image_bitmap & worm_cluster_bitmap, ns_image_bitmap & worm_bitmap){
	ns_image_properties p(combined_image.properties());
	p.components = 1;
	worm_cluster_bitmap.init(p);
	worm_bitmap.init(p);
	absolute_grayscale.init(p);
	relative_grayscale.init(p);

	for (unsigned int y = 0; y < p.height; y++){
		for (unsigned int x = 0; x < p.width; x++){
			relative_grayscale[y][x] =  255-(
										  (combined_image[y][3*x+0]<<1) //get the top seven bits from channel 0
										| (combined_image[y][3*x+1]&0x01)); //and the lowest bit from channel 1

			absolute_grayscale[y][x] =combined_image[y][3*x+2];
			bool r(combined_image[y][3*x+0]&0x80),
				 b(combined_image[y][3*x+1]&0x80);
			worm_bitmap[y][x] = !b;
			worm_cluster_bitmap[y][x] = r^b;
		}
	}
}

void ns_detected_worm_info::from_training_set_visualization(ns_worm_context_image & context_visualization, const ns_vector_2i & region_size_, const ns_vector_2i & region_offset_in_context_image, const ns_vector_2i & context_image_size_){
	//cerr << "Image is " << im.properties().width << "x" << im.properties().height <<  "\n";
	
	context_visualization.split(relative_grayscale(),absolute_grayscale(),bitmap_of_worm_cluster(),bitmap());
	context_image_size = ns_vector_2i(relative_grayscale().properties().width,relative_grayscale().properties().height);
	if (!(context_image_size == context_image_size_))
		throw ns_ex("ns_detected_worm_info::from_training_set_visualization()::Context image size does not match metadata spec");
	
	region_size = region_size_;
	context_visualization.combined_image.pump(_worm_context_image->combined_image,1024);
	relative_grayscale().pump(_worm_context_image->relative_grayscale,1024);
	absolute_grayscale().pump(_worm_context_image->absolute_grayscale,1024);
	crop_images_to_region_size(region_offset_in_context_image);

#ifdef NS_RETHRESHOLD_TRAINING_SET
	ns_image_standard tmp;
	ns_two_stage_difference_thresholder::run<ns_8_bit>(grayscale(), tmp);
	tmp.pump(bitmap_of_worm_cluster(),1024);
#endif
}

void ns_detected_worm_info::crop_images_to_region_size(const ns_vector_2i & region_offset_in_context_image){


	long l(region_offset_in_context_image.x),
		 b(region_offset_in_context_image.y),
		 r(region_offset_in_context_image.x+region_size.x-1),
		 t(region_offset_in_context_image.y+region_size.y-1);
	
	ns_image_properties p(_bitmap_of_worm_cluster->properties());
	p.width = region_size.x;
	p.height = region_size.y;

	ns_image_bitmap * bmp(new ns_image_bitmap);
	//				* edge(new ns_image_bitmap);
	ns_image_standard * gs_absolute(new ns_image_standard),
					  * gs_relative(new ns_image_standard);

	bmp->prepare_to_recieve_image(p);
	//edge->prepare_to_recieve_image(p);
	gs_absolute->prepare_to_recieve_image(p);
	gs_relative->prepare_to_recieve_image(p);
	for (long y = b; y <= t; y++)
		for (long x = l; x <= r; x++)
			(*bmp)[y-b][x-l] = bitmap()[y][x];

	for (long y = b; y <= t; y++)
		for (long x = l; x <= r; x++){
			(*gs_absolute)[y-b][x-l] = absolute_grayscale()[y][x];
			(*gs_relative)[y-b][x-l] = relative_grayscale()[y][x];
		}
	accept_images(bmp,new ns_image_bitmap(),gs_relative,gs_absolute);

	ns_image_bitmap bitmap_of_worm_cluster;
	_bitmap_of_worm_cluster->pump(bitmap_of_worm_cluster,1024);
	_bitmap_of_worm_cluster->prepare_to_recieve_image(p);
	for (long y = b; y <= t; y++)
		for (long x = l; x <= r; x++)
			(*_bitmap_of_worm_cluster)[y-b][x-l] = bitmap_of_worm_cluster[y][x];

}

double ns_detected_worm_stats::transformed_statistic(const ns_detected_worm_classifier & val) const{
	double value = 0;
	for (unsigned int i = 0; i < (int)ns_stat_number_of_stats; i++){
		double trans = statistics[i]-_model->statistics_ranges[i].avg;
		if (_model->statistics_ranges[i].std != 0)
			trans /=_model->statistics_ranges[i].std;
		value += _model->pca_spec.pc_vectors[val][i]*trans; 
	}
	return value;
}

double ns_detected_worm_stats::scaled_statistic(const ns_detected_worm_classifier & val) const{
	if (_model == 0)
		throw ns_ex("ns_detected_worm_stats::No model file specified when requesting scaled statistic");
	if (!_model->statistics_ranges[val].specified)
		throw ns_ex("ns_detected_worm_stats::Using unspecified range:") << val;
	if (_model->pca_spec.pc_vectors.size() != 0)
		return transformed_statistic(val);

	if (_model->statistics_ranges[val].min == _model->statistics_ranges[val].max)
		return 0;
	if (_model->statistics_ranges[val].min > _model->statistics_ranges[val].max) throw ns_ex("ns_detected_worm_stats::range for ") << ns_classifier_abbreviation(val) << " has larger min (" << _model->statistics_ranges[val].min << ") than max(" << _model->statistics_ranges[val].max << ")";
	if (_model->statistics_ranges[val].max == _model->statistics_ranges[val].min){
		
		throw ns_ex("Statistic range for ") << ns_classifier_label(val) << " is 0!";
		return 0;
	}

	return ((*this)[val] - _model->statistics_ranges[val].min)/(_model->statistics_ranges[val].max - _model->statistics_ranges[val].min);

}
std::string ns_detected_worm_stats::parameter_string() const{
	std::string o;
	//XXX
	if (ns_stat_number_of_stats > statistics.size())
		throw ns_ex("WHOA!");
	if (ns_stat_number_of_stats > _model->statistics_ranges.size())
		throw ns_ex("WHOA!");
	for (unsigned int i = 0; i < (unsigned int)ns_stat_number_of_stats; i++){
		if (_model->included_statistics[i] == 0) continue;
		o+= ns_to_string(i+1) + ":" + ns_to_string( scaled_statistic((ns_detected_worm_classifier)i) ) + " ";
	}
	return o;
}

#ifdef NS_USE_MACHINE_LEARNING
	#ifndef NS_USE_TINYSVM
svm_node * ns_detected_worm_stats::produce_vector() const{
		svm_node * node = new svm_node[size()+1];
		unsigned int j = 0;
		for (unsigned int i = 0; i < (unsigned int)ns_stat_number_of_stats;i++){
			if (_model->included_statistics[i] == 0) continue;
			node[j].index = i+1;
			node[j].value = scaled_statistic((ns_detected_worm_classifier)i);
			j++;
		}
		node[j].index = -1;
		node[j].value = 0;

		return node;
	}
	#endif
#endif

void ns_image_worm_detection_results::output_feature_statistics(ostream & o){

	ns_detected_worm_stats::output_csv_header(o);
	o << "\n";
	for (unsigned int i = 0; i < number_of_putative_worms(); i++){

		putative_worms[i].generate_stats().output_csv_data(region_info_id,capture_time,
																	putative_worms[i].region_position_in_source_image,
																	putative_worms[i].region_size,putative_worms[i].hand_annotations,o);
		o << "\n";
	}
}
void ns_detected_worm_stats::draw_feature_frequency_distributions(const std::vector<ns_detected_worm_stats> & worm_stats, const std::vector<ns_detected_worm_stats> & non_worm_stats,const std::string & label,const std::string &output_directory){
	std::string freq_base_dir = output_directory;
	ns_dir::create_directory_recursive(freq_base_dir);
	ofstream raw_stats((freq_base_dir + DIR_CHAR_STR + "stats.csv").c_str());
	
	raw_stats << "Worm ID,Label,Classification";
	for (unsigned int s = 0; s < (unsigned int) ns_stat_number_of_stats; s++){
		raw_stats << "," << ns_classifier_label((ns_detected_worm_classifier)s);
	}
	raw_stats << "\n";
	for (unsigned int i = 0; i < worm_stats.size(); i++){		
		raw_stats << i << "," << label << ",Accepted";
		for (unsigned int s = 0; s < (unsigned int) ns_stat_number_of_stats; s++)
			raw_stats << "," << worm_stats[i][(ns_detected_worm_classifier)s];
		
		raw_stats << "\n";
	}
	for (unsigned int i = 0; i < non_worm_stats.size(); i++){		
		raw_stats << i << "," << label <<",Rejected";
		for (unsigned int s = 0; s < (unsigned int) ns_stat_number_of_stats; s++)
			raw_stats << "," << non_worm_stats[i][(ns_detected_worm_classifier)s];
		
		raw_stats << "\n";
	}
	raw_stats.close();

	if (worm_stats.size() != 0){
		std::vector<ns_detected_worm_stats> merged(worm_stats.size() + non_worm_stats.size());
		for (unsigned int i = 0; i < worm_stats.size(); i++)
			merged[i] = worm_stats[i];
		for (unsigned int i = 0; i < non_worm_stats.size(); i++)
			merged[worm_stats.size() + i] = non_worm_stats[i];
		std::string new_dir = output_directory + DIR_CHAR_STR + "merged";
		ns_dir::create_directory_recursive(new_dir);
		draw_feature_frequency_distributions(std::vector<ns_detected_worm_stats>(),merged,label,new_dir);
	}
	std::vector<ns_graph_object> worm_distributions, non_worm_distributions;
	
	worm_distributions.clear();
	worm_distributions.resize((unsigned int)ns_stat_number_of_stats,ns_graph_object::ns_graph_dependant_variable);
	non_worm_distributions.clear();
	non_worm_distributions.resize((unsigned int)ns_stat_number_of_stats,ns_graph_object::ns_graph_dependant_variable);

	for (unsigned int s = 0; s < (unsigned int) ns_stat_number_of_stats; s++){

		try{
			ns_graph graph;
			ns_graph_object &worm_dat = worm_distributions[(unsigned int)s];
			ns_graph_object &non_worm_dat = non_worm_distributions[(unsigned int)s];

			//ns_graph_object worm_dat(ns_graph_object::ns_graph_dependant_variable);
			//ns_graph_object non_worm_dat(ns_graph_object::ns_graph_dependant_variable);


			for (unsigned int i = 0; i < worm_stats.size(); i++)
				worm_dat.y.push_back(worm_stats[i][(ns_detected_worm_classifier)s]);
			for (unsigned int i = 0; i < non_worm_stats.size(); i++)
				non_worm_dat.y.push_back(non_worm_stats[i][(ns_detected_worm_classifier)s]);

			worm_dat.properties.line.color=ns_color_8(10,255,50);
			worm_dat.properties.point.color=worm_dat.properties.line.color*.5;
			worm_dat.properties.area_fill.color = worm_dat.properties.line.color;
			worm_dat.properties.line.draw = false;
			worm_dat.properties.area_fill.draw = true;
			worm_dat.properties.area_fill.opacity = .5;
			non_worm_dat.properties.line.color=ns_color_8(255,10,50);
			non_worm_dat.properties.point.color=non_worm_dat.properties.line.color*.5;
			non_worm_dat.properties.area_fill.color = non_worm_dat.properties.line.color;
			non_worm_dat.properties.line.draw = false;
			non_worm_dat.properties.area_fill.draw = true;
			non_worm_dat.properties.area_fill.opacity = .5;
			std::vector<const ns_graph_object *> objs;
			objs.push_back(&worm_dat);
			objs.push_back(&non_worm_dat);
		//	if (s == 43 || s == 30)
		//		cerr << "S";
			ns_graph_axes axes(graph.add_frequency_distribution(objs));
			axes.boundary_specified(3) = false;
			ns_image_standard freq_graph;
			ns_image_properties prop;
			prop.width = 900;
			prop.height = 600;
			prop.components = 3;
			freq_graph.prepare_to_recieve_image(prop);
			graph.set_graph_display_options(ns_classifier_label((ns_detected_worm_classifier)s),axes);
			graph.draw(freq_graph);
			
			std::string fn = freq_base_dir + "\\" + ns_classifier_abbreviation((ns_detected_worm_classifier)s) + ".tif";

			ns_tiff_image_output_file<ns_8_bit> im_out;
			ns_image_stream_file_sink<ns_8_bit > file_sink(fn,im_out,1.0,128);
			freq_graph.pump(file_sink,128);
		}
		catch(ns_ex & ex){
			cerr << "draw_feature_frequency_distributions()::Error while plotting distribution of " << ns_classifier_label((ns_detected_worm_classifier)s) << "::" << ex.text() << "\n";
		}
	}
}

void ns_image_worm_detection_results::create_visualization(const unsigned int cross_height, const unsigned int cross_thickness, ns_image_standard & image, const std::string & data_label, const bool mark_crosshairs, const bool draw_labels, const bool draw_non_worms){
	if (image.properties().components != 3)
			throw ns_ex("ns_detected_object_identifier::Visualizations can only be written to color images!");
	long x_bounds[2];
	long y_bounds[2];
	long width = image.properties().width;
	long height = image.properties().height;
	
	long thickness_x_bounds[2];
	long thickness_y_bounds[2];

	
	if (draw_non_worms){
		//make rejected worms red
		for (unsigned int i = 0; i < not_worms.size(); i++){
			//draw centers
			for (int y = 0; y < not_worms[i]->region_size.y; y++){
				for (int x = 0; x < not_worms[i]->region_size.x; x++){
					if (not_worms[i]->bitmap()[y][x]){		
						image[y+not_worms[i]->region_position_in_source_image.y][3*(x+not_worms[i]->region_position_in_source_image.x)] = 255;
						image[y+not_worms[i]->region_position_in_source_image.y][3*(x+not_worms[i]->region_position_in_source_image.x)+1] = image[y][3*(x+not_worms[i]->region_position_in_source_image.x)+1]/2;
						image[y+not_worms[i]->region_position_in_source_image.y][3*(x+not_worms[i]->region_position_in_source_image.x)+2] = image[y][3*(x+not_worms[i]->region_position_in_source_image.x)+2]/2;
					}
				}
			}
		}
	}
	if (draw_non_worms){
		for (unsigned int i = 0; i < actual_worms.size(); i++){
			//draw centers
			for (int y = 0; y < actual_worms[i]->region_size.y; y++){
				for (int x = 0; x < actual_worms[i]->region_size.x; x++){
					if (actual_worms[i]->bitmap()[y][x]){
						image[y+actual_worms[i]->region_position_in_source_image.y][3*(x+actual_worms[i]->region_position_in_source_image.x)] =  image[y][3*(x+actual_worms[i]->region_position_in_source_image.x)]/2;
						image[y+actual_worms[i]->region_position_in_source_image.y][3*(x+actual_worms[i]->region_position_in_source_image.x)+1] = 255;
						image[y+actual_worms[i]->region_position_in_source_image.y][3*(x+actual_worms[i]->region_position_in_source_image.x)+2] = 255;
					}
				}
				
			}
			//draw edges
			for (int y = 0; y < actual_worms[i]->region_size.y; y++){
				for (int x = 0; x < actual_worms[i]->region_size.x; x++){
					if (actual_worms[i]->edge_bitmap()[y][x]){
						image[y+actual_worms[i]->region_position_in_source_image.y][3*(x+actual_worms[i]->region_position_in_source_image.x)] = image[y][3*(x+actual_worms[i]->region_position_in_source_image.x)]/2;
						image[y+actual_worms[i]->region_position_in_source_image.y][3*(x+actual_worms[i]->region_position_in_source_image.x)+1] = 180;
						image[y+actual_worms[i]->region_position_in_source_image.y][3*(x+actual_worms[i]->region_position_in_source_image.x)+2] = 255;
					}
				}
			}
		}		
	}
	else{
		for (unsigned int i = 0; i < actual_worms.size(); i++){
			//draw centers
			for (int y = 0; y < actual_worms[i]->region_size.y; y++){
				for (int x = 0; x < actual_worms[i]->region_size.x; x++){
					if (actual_worms[i]->bitmap()[y][x]){
						image[y+actual_worms[i]->region_position_in_source_image.y][3*(x+actual_worms[i]->region_position_in_source_image.x)] = 255;
						image[y+actual_worms[i]->region_position_in_source_image.y][3*(x+actual_worms[i]->region_position_in_source_image.x)+1] = 255;
						image[y+actual_worms[i]->region_position_in_source_image.y][3*(x+actual_worms[i]->region_position_in_source_image.x)+2] = image[y][3*(x+actual_worms[i]->region_position_in_source_image.x)+2]/2;
					}
				}
				
			}
			//draw edges
			for (int y = 0; y < actual_worms[i]->region_size.y; y++){
				for (int x = 0; x < actual_worms[i]->region_size.x; x++){
					if (actual_worms[i]->edge_bitmap()[y][x]){
						image[y+actual_worms[i]->region_position_in_source_image.y][3*(x+actual_worms[i]->region_position_in_source_image.x)] = 255;
						image[y+actual_worms[i]->region_position_in_source_image.y][3*(x+actual_worms[i]->region_position_in_source_image.x)+1] = 180;
						image[y+actual_worms[i]->region_position_in_source_image.y][3*(x+actual_worms[i]->region_position_in_source_image.x)+2] = image[y][3*(x+actual_worms[i]->region_position_in_source_image.x)+2]/2;
					}
				}
			}
		}		
	}

	for (unsigned int i = 0; i < actual_worms.size(); i++){
		ns_vector_2i spine_vertex[2];
		//draw spine
		if (actual_worms[i]->worm_shape.nodes.size() != 0){
			spine_vertex[0].x = actual_worms[i]->region_position_in_source_image.x + (int)actual_worms[i]->worm_shape.nodes[0].x;
			spine_vertex[0].y = actual_worms[i]->region_position_in_source_image.y + (int)actual_worms[i]->worm_shape.nodes[0].y;
		
			for (unsigned int j = 1; j < actual_worms[i]->worm_shape.nodes.size()-1; j++){  //deliberately leave out ends to differentiate multiple worms
				spine_vertex[1].x = actual_worms[i]->region_position_in_source_image.x + (int)actual_worms[i]->worm_shape.nodes[j].x;
				spine_vertex[1].y = actual_worms[i]->region_position_in_source_image.y + (int)actual_worms[i]->worm_shape.nodes[j].y;
				image.draw_line_color(spine_vertex[0],spine_vertex[1], ns_color_8(50,50,50) );
				spine_vertex[0] = spine_vertex[1];
			}


			ns_vector_2i avg(actual_worms[i]->region_position_in_source_image + actual_worms[i]->worm_shape.nodes[actual_worms[i]->worm_shape.nodes.size()/2]);
			//draw crosses
			if (mark_crosshairs){
				if (avg.x < 0 || avg.y < 0)
					throw ns_ex("ns_detected_worm_info::invalid worm center encountered during crosshair rendering: ") << avg.x << "," << avg.y;
				x_bounds[0] = (long)avg.x - (long)cross_height/2;
				x_bounds[1] = (long)avg.x + (long)cross_height/2;
				y_bounds[0] = (long)avg.y - (long)cross_height/2;
				y_bounds[1] = (long)avg.y + (long)cross_height/2;
				if (x_bounds[0] < 0)		x_bounds[0] = 0;
				if (x_bounds[1] > width-1)	x_bounds[1] = width-1;			
				if (y_bounds[0] < 0)		y_bounds[0] = 0;
				if (y_bounds[1] > height-1) y_bounds[1] = height-1;
				
				thickness_x_bounds[0] = (long)avg.x - (long)cross_thickness/2;
				thickness_x_bounds[1] = (long)avg.x + (long)cross_thickness/2;
				thickness_y_bounds[0] = (long)avg.y - (long)cross_thickness/2;
				thickness_y_bounds[1] = (long)avg.y + (long)cross_thickness/2;

				if (thickness_x_bounds[0] < 0)			thickness_x_bounds[0] = 0;
				if (thickness_x_bounds[1] > width-1)	thickness_x_bounds[1] = width-1;			
				if (thickness_y_bounds[0] < 0)			thickness_y_bounds[0] = 0;
				if (thickness_y_bounds[1] > height-1)	thickness_y_bounds[1] = height-1;
				if (!draw_non_worms){
					//draw horizontal line
					for (unsigned int y = thickness_y_bounds[0]; y < static_cast<unsigned int>(thickness_y_bounds[1]); y++)
						for (unsigned int x = x_bounds[0]; x < static_cast<unsigned int>(x_bounds[1]); x++){
							image[y][x*3] = image[y][x*3]+(255 - image[y][x*3])/2;//127*(y%2)+126;
							image[y][x*3+1]/=2;
							image[y][x*3+2]/=2;
						}
					//draw vertical line
					for (unsigned int y = y_bounds[0]; y < static_cast<unsigned int>(y_bounds[1]); y++)
						for (unsigned int x = thickness_x_bounds[0]; x < static_cast<unsigned int>(thickness_x_bounds[1]); x++){
							image[y][x*3] = image[y][x*3]+(255 - image[y][x*3])/2 ;// 127*(x%2)+126;
							image[y][x*3+1]/=2;
							image[y][x*3+2]/=2;
						}
				}
			}
		}

	}	
	//ouput reasons for rejection of rejected reasons.
	if (draw_labels){
		ns_acquire_lock_for_scope font_lock(font_server.default_font_lock, __FILE__, __LINE__);
		ns_font & font(font_server.get_default_font());
		font.set_height(16);
		ns_color_8 c(180,180,180);
		for(unsigned int i = 0; i < putative_worms.size(); i++){
			ns_vector_2i loc = putative_worms[i].region_position_in_source_image + putative_worms[i].region_size;
			if (loc.x < 20)
				continue;
			font.draw(loc.x, loc.y,
					  c,putative_worms[i].failure_reason.text(),image);
		}
		for (unsigned int i = 0; i < region_labels.size(); i++)
			font.draw(region_labels[i].pos.x, region_labels[i].pos.y,
					  c,region_labels[i].text,image);
		font_lock.release();
	}

	if (data_label.size() != 0){
		unsigned int im_bottom = image.properties().height;
		unsigned int bottom_margin_size = im_bottom/15;
		ns_acquire_lock_for_scope font_lock(font_server.default_font_lock, __FILE__, __LINE__);
		ns_font & font(font_server.get_default_font());
		font.set_height((bottom_margin_size)/3);
	
		image.increase_size(ns_image_properties(im_bottom + bottom_margin_size,image.properties().width,image.properties().components));
		 
		for (unsigned int y = 0; y < bottom_margin_size; y++)
			for (unsigned int x = 0; x < image.properties().width*image.properties().components; x++)
				image[im_bottom + y][x] = 0;

		font.draw(17,im_bottom+bottom_margin_size/2,ns_color_8(200,200,200),data_label,image);
		font_lock.release();
	}

}

///During object detection, an visualzation of an object's Delauny triangular mesh and spine is made.
///create_spine_visualizations() makes a collage of each worm's visualization.
void ns_image_worm_detection_results::create_spine_visualizations(ns_image_standard  & reciever){

	//now build collage from the individual spine visualizations
	std::vector<const ns_image_standard *> images(actual_worms.size());
	for (unsigned int i = 0; i < actual_worms.size(); i++)
		images[i] = &actual_worms[i]->spine_visualization();

	ns_make_packed_collage(images, reciever);
}

void ns_image_worm_detection_results::create_spine_visualizations(std::vector<ns_svg> & objects){
	objects.reserve(actual_worms.size());
	for (unsigned int i = 0; i < actual_worms.size(); i++)
		objects.push_back(actual_worms[i]->spine_svg_visualization());
}

void ns_image_worm_detection_results::create_edge_visualization(ns_image_standard & reciever){
	std::vector<ns_image_standard> images(putative_worms.size());
	for (unsigned int i = 0; i < putative_worms.size(); i++){
		ns_image_bitmap & bmp(putative_worms[i].edge_bitmap());
		images[i].prepare_to_recieve_image(bmp.properties());
		for (unsigned int y = 0; y < bmp.properties().height; y++)
			for (unsigned int x = 0; x < bmp.properties().width; x++)
				images[i][y][x] = 255*(bmp[y][x]);
	}
	ns_make_packed_collage(images, reciever);
}


///During object detection, an visualzation of an object's Delauny triangular mesh and spine is made.
///create_spine_visualizations() makes a collage of each worm's spine visualization, justaposed
///with graphs of various statistics calculated as a function of arclength along the worm spine (width, curvature, etc)
void ns_image_worm_detection_results::create_spine_visualizations_with_stats(ns_image_standard & reciever){

	if (actual_worms.size() == 0){
		reciever.init(ns_image_properties(1,1,1));
		reciever[0][0]=0;
		return;
	}
	//now build collage from the individual spine visualizations
	ns_collage_tree_node<ns_8_bit> root;
	std::vector< std::vector<ns_image_standard * > > graphs(actual_worms.size());
	for (unsigned int i = 0; i < actual_worms.size(); i++){
		ns_collage_tree_node<ns_8_bit> worm_and_graphs;
		worm_and_graphs.add(actual_worms[i]->spine_visualization());
		graphs[i].push_back(actual_worms[i]->width_graph());
		graphs[i].push_back(actual_worms[i]->curvature_graph());
		ns_collage_tree_node<ns_8_bit> just_graphs;
		just_graphs.add(*graphs[i][graphs[i].size()-2]);
		just_graphs.add(*graphs[i][graphs[i].size()-1]);
		worm_and_graphs.add(just_graphs);
		root.add(worm_and_graphs);
	}

	ns_make_collage(root, reciever,512);

	for (unsigned int i = 0; i < graphs.size(); i++){
		for (unsigned int j = 0; j < graphs[i].size(); j++)
			delete graphs[i][j];
	}
}

void ns_image_worm_detection_results::create_spine_visualizations_with_stats(std::vector<ns_svg> & objects){
	objects.reserve(actual_worms.size());
	for (unsigned int i = 0; i < actual_worms.size(); i++){
		objects.push_back(actual_worms[i]->spine_svg_visualization());
		std::vector<ns_svg>::size_type s= objects.size();
		objects.resize(s+2);
		actual_worms[i]->width_graph(objects[s]);
		actual_worms[i]->curvature_graph(objects[s+1]);
	}
}
void ns_image_worm_detection_results::create_reject_spine_visualizations_with_stats(std::vector<ns_svg> & objects){
	objects.reserve(not_worms.size());
	for (unsigned int i = 0; i < not_worms.size(); i++){
		objects.push_back(not_worms[i]->spine_svg_visualization());
		std::vector<ns_svg>::size_type s= objects.size();
		objects.resize(s+2);
		not_worms[i]->width_graph(objects[s]);
		not_worms[i]->curvature_graph(objects[s+1]);
	}
}
///During object detection, an visualzation of an object's Delauny triangular mesh and spine is made.
///create_spine_visualizations() makes a collage of each non-worm's visualization.
///The resulting collage tends to be very large due to the large number of dirt objects present
void ns_image_worm_detection_results::create_reject_spine_visualizations(ns_image_standard & reciever){

	if (not_worms.size() == 0){
		reciever.init(ns_image_properties(1,1,1));
		reciever[0][0]=0;
		return;
	}
	//now build collage from the individual spine visualizations
	std::vector<const ns_image_standard *> images;
	images.reserve(not_worms.size());
	unsigned int s = 0;
	for (unsigned int i = 0; i < not_worms.size(); i++){
	//	if (not_worms[i]->spine_visualization().properties().width < ns_worm_detection_constants::max_non_worm_output_dimention() &&
	//		not_worms[i]->spine_visualization().properties().height < ns_worm_detection_constants::max_non_worm_output_dimention()){
			images.push_back(&not_worms[i]->spine_visualization());
	//	}
	}
	float resolution = 0;
	if (images.size() != 0)
		resolution = images[0]->properties().resolution;
	ns_make_packed_collage(images, reciever,0,1200,false);
}


void ns_image_worm_detection_results::create_reject_spine_visualizations(std::vector<ns_svg> & objects){
	objects.reserve(not_worms.size());
	for (unsigned int i = 0; i < not_worms.size(); i++)
		objects.push_back(not_worms[i]->spine_svg_visualization());
}

///During object detection, an visualzation of an object's Delauny triangular mesh and spine is made.
///create_spine_visualizations() makes a collage of each non-worm's spine visualization, justaposed
///with graphs of various statistics calculated as a function of arclength along the worm spine (width, curvature, etc)
void ns_image_worm_detection_results::create_reject_spine_visualizations_with_stats(ns_image_standard & reciever){

	if (not_worms.size() == 0){
		reciever.init(ns_image_properties(1,1,1));
		reciever[0][0]=0;
		return;
	}

	//now build collage from the individual spine visualizations
	ns_collage_tree_node<ns_8_bit> root;
	std::vector< std::vector<ns_image_standard * > > graphs(not_worms.size());

	for (unsigned int i = 0; i < not_worms.size(); i++){
		ns_collage_tree_node<ns_8_bit> worm_and_graphs;
		worm_and_graphs.add(not_worms[i]->spine_visualization());
		graphs[i].push_back(not_worms[i]->width_graph());
		graphs[i].push_back(not_worms[i]->curvature_graph());
		ns_collage_tree_node<ns_8_bit> just_graphs;
		just_graphs.add(*graphs[i][graphs[i].size()-1]);
		just_graphs.add(*graphs[i][graphs[i].size()-2]);
		worm_and_graphs.add(just_graphs);
		root.add(worm_and_graphs);
	}


	ns_make_collage(root, reciever,512);
	for (unsigned int i = 0; i < graphs.size(); i++){
			for (unsigned int j = 0; j < graphs[i].size(); j++)
				delete graphs[i][j];
		}
}


unsigned int ns_count_number_of_worms_in_detected_object_group(std::vector<ns_detected_object *> & objects){
	unsigned int number_of_worms(0),
				 number_of_worms_in_region;
for (unsigned int i = 0; i < objects.size(); i++){
		number_of_worms_in_region = 0;
		for (unsigned long j = 0; j < (unsigned long)objects[i]->segment_cluster_solutions.mutually_exclusive_solution_groups.size(); j++)
			number_of_worms_in_region+= (unsigned long)objects[i]->segment_cluster_solutions.mutually_exclusive_solution_groups[j].size();
		number_of_worms+=number_of_worms_in_region;
		if (number_of_worms_in_region == 0)
			objects[i]->label += "no_worm_spines";  //to be used in visualization to explain why the region produced no output
	}
	return number_of_worms;
}
void ns_image_worm_detection_results::process_segment_cluster_solutions(std::vector<ns_detected_object *> & objects, const ns_image_standard &relative_grayscale_source, const ns_image_standard &absolute_grayscale_source, const ns_detected_worm_info::ns_visualization_type visualization_type, const unsigned long maximum_number_of_putative_worms, ns_sql * sql_for_debug_output){

	//first we count the maximum number of worms that our algorithm could produce
	unsigned long number_of_worms = ns_count_number_of_worms_in_detected_object_group(objects);
	//then we allocate memory for the putative worms
	putative_worms.resize(number_of_worms);
	mutually_exclusive_worm_groups.resize(objects.size());

	//then we convert the detected objects to worms

	image_server_const.add_subtext_to_current_event("Solving segment clusters: ", sql_for_debug_output);
	ns_progress_reporter pr(number_of_worms,5);

	unsigned long worms_found = 0;
	for (unsigned long i = 0; i < (unsigned long)objects.size(); i++){
		pr(worms_found);
		worms_found+=ns_detected_worm_info::from_segment_cluster_solution(*objects[i],putative_worms,worms_found,mutually_exclusive_worm_groups[i],relative_grayscale_source,absolute_grayscale_source,ns_detected_worm_info::ns_large_source_grayscale_images_provided,visualization_type);
		if (maximum_number_of_putative_worms != 0 && worms_found > maximum_number_of_putative_worms)
			throw ns_ex("ns_image_worm_detection_results::process_segment_cluster_solutions()::The specified maximum number of putative worms (") << maximum_number_of_putative_worms << ") has been exceeded.";
	}
	pr(worms_found);

	for (unsigned int i = 0; i < objects.size(); i++)
		delete objects[i];
	objects.resize(0);
}


void ns_image_worm_detection_results::set_whole_image_region_stats(const ns_whole_image_region_stats & stats){
	for (unsigned int i = 0; i < putative_worms.size(); i++)
		putative_worms[i].whole_image_stats.whole_image_region_stats = stats;
}
void ns_image_worm_detection_results::calculate_image_region_stats(){
	ns_whole_image_region_stats overall_stats;
	overall_stats.absolute_intensity_stats.minimum_intensity = 255;
	overall_stats.relative_intensity_stats.minimum_intensity = 255;

	//otherwise, calculate the stats for the entire image.
	for (unsigned int i = 0; i < putative_worms.size(); i++){
		const unsigned int h(putative_worms[i].bitmap().properties().height),
							w(putative_worms[i].bitmap().properties().width);
		ns_whole_image_region_stats cur_region_stats;
		unsigned long area = 0;
		for (unsigned int y = 0; y < h; y++){
			for (unsigned int x = 0; x < w; x++){
				area +=putative_worms[i].bitmap()[y][x]?1:0;
				cur_region_stats.absolute_intensity_stats.average_intensity += putative_worms[i].bitmap()[y][x]*putative_worms[i].absolute_grayscale()[y][x];
				cur_region_stats.relative_intensity_stats.average_intensity += putative_worms[i].bitmap()[y][x]*putative_worms[i].relative_grayscale()[y][x];
			}
		}
		if (area != 0){
			cur_region_stats.absolute_intensity_stats.average_intensity/=(area);
			cur_region_stats.relative_intensity_stats.average_intensity/=(area);
		}
		overall_stats.absolute_intensity_stats.average_intensity += cur_region_stats.absolute_intensity_stats.average_intensity;
		overall_stats.relative_intensity_stats.average_intensity += cur_region_stats.relative_intensity_stats.average_intensity;
		if (overall_stats.absolute_intensity_stats.maximum_intensity < cur_region_stats.absolute_intensity_stats.average_intensity)
			overall_stats.absolute_intensity_stats.maximum_intensity = cur_region_stats.absolute_intensity_stats.average_intensity;
		if (overall_stats.absolute_intensity_stats.maximum_intensity < cur_region_stats.relative_intensity_stats.average_intensity)
			overall_stats.relative_intensity_stats.maximum_intensity = cur_region_stats.relative_intensity_stats.average_intensity;
		if (overall_stats.absolute_intensity_stats.minimum_intensity > cur_region_stats.absolute_intensity_stats.average_intensity)
			overall_stats.absolute_intensity_stats.minimum_intensity = cur_region_stats.absolute_intensity_stats.average_intensity;
		if (overall_stats.absolute_intensity_stats.minimum_intensity > cur_region_stats.relative_intensity_stats.average_intensity)
			overall_stats.relative_intensity_stats.minimum_intensity = cur_region_stats.relative_intensity_stats.average_intensity;
	}
	if (putative_worms.size() != 0){
		overall_stats.absolute_intensity_stats.average_intensity/=(unsigned int)putative_worms.size();
		overall_stats.relative_intensity_stats.average_intensity/=(unsigned int)putative_worms.size();
	}
	
	for (unsigned int i = 0; i < putative_worms.size(); i++)
		putative_worms[i].whole_image_stats.worm_region_specific_region_stats = overall_stats;
}

std::map<std::string,unsigned long> ns_image_worm_detection_results:: give_worm_rejection_reasons() const{
	std::map<std::string,unsigned long> reasons;
	for (unsigned long i = 0; i < this->not_worms.size(); i++){
		std::map<std::string,unsigned long>::iterator p = reasons.find(not_worms[i]->failure_reason.text());
		if (p == reasons.end())
			reasons[not_worms[i]->failure_reason.text()] = 1;
		else (p->second)++;
	}
	return reasons;
}
void ns_image_worm_detection_results::sort_putative_worms(const ns_svm_model_specification & model){
	actual_worms.reserve(putative_worms.size());
	not_worms.reserve(putative_worms.size());	

	for (unsigned int i = 0; i < mutually_exclusive_worm_groups.size(); i++){
		std::vector<unsigned int> number_of_worms_in_exclusive_group;
		std::vector<double> total_length_of_worms_in_exclusive_group;

		std::vector< std::vector<ns_detected_worm_info *> > & mutually_exclusive_worms = mutually_exclusive_worm_groups[i];

		if (mutually_exclusive_worms.size() == 0)
			continue;
		number_of_worms_in_exclusive_group.resize(mutually_exclusive_worms.size(),0);
		total_length_of_worms_in_exclusive_group.resize(mutually_exclusive_worms.size(),0);

		//find worms in each mutually exclusive group
		for (unsigned int g = 0; g < mutually_exclusive_worms.size(); g++){
			for (unsigned int w = 0; w < mutually_exclusive_worms[g].size(); w++){
				
				mutually_exclusive_worms[g][w]->hand_annotations.identified_as_a_worm_by_machine = mutually_exclusive_worms[g][w]->is_a_worm(model);
				if (mutually_exclusive_worms[g][w]->hand_annotations.identified_as_a_worm_by_machine){
					total_length_of_worms_in_exclusive_group[g]+= mutually_exclusive_worms[g][w]->worm_shape.length;
					number_of_worms_in_exclusive_group[g]++;
				}
			}
		}
		//find which group has the most number of worms.
		unsigned int largest_number_of_worms = 0;
		double length_of_largest_group = 0;
		unsigned int largest_group_id = 0;
		for (unsigned int g = 0; g < mutually_exclusive_worms.size(); g++){
			if (number_of_worms_in_exclusive_group[g] >= largest_number_of_worms &&
				total_length_of_worms_in_exclusive_group[g] > length_of_largest_group){
				largest_group_id = g;
				length_of_largest_group = total_length_of_worms_in_exclusive_group[g];
				largest_number_of_worms = number_of_worms_in_exclusive_group[g];
			}
		}
		//cout << "Winning group has " << number_of_worms_in_exclusive_group[largest_group_id] << " worms.\n";
		
	
		//add all the worms from the winning group to the detected worm list.
		
		for (unsigned int g = 0; g < mutually_exclusive_worms.size(); g++){
			#ifdef ALLOW_ALL_SPINE_PERMUTATIONS
			if (true){	
			#else
			if (g == largest_group_id){
			#endif
				for (unsigned int w = 0; w <mutually_exclusive_worms[g].size(); w++){
					if (mutually_exclusive_worms[g][w]->hand_annotations.identified_as_a_worm_by_machine)
						actual_worms.push_back(mutually_exclusive_worms[g][w]);
				}
			}
			//mark all animals that weren't chosen as being rejected misambiguated solutions.
			else{
				for (unsigned int w = 0; w <mutually_exclusive_worms[g].size(); w++)
						if (mutually_exclusive_worms[g][w]->hand_annotations.identified_as_a_worm_by_machine)
							mutually_exclusive_worms[g][w]->hand_annotations.identified_as_misdisambiguated_multiple_worms = true;
				
			}
		}

		#ifndef ALLOW_ALL_SPINE_PERMUTATIONS
		for (unsigned int g = 0; g < mutually_exclusive_worms.size(); g++){
			for (unsigned int w = 0; w < mutually_exclusive_worms[g].size(); w++){
				if (g != largest_group_id || !mutually_exclusive_worms[g][w]->hand_annotations.identified_as_a_worm_by_machine)
					not_worms.push_back(mutually_exclusive_worms[g][w]);
			}
		}
		#endif
	}
}



void ns_image_worm_detection_results::clear_images(){
	for (unsigned int i = 0; i < putative_worms.size(); i++){
		putative_worms[i].bitmap().clear();
		putative_worms[i].absolute_grayscale().clear();
		putative_worms[i].relative_grayscale().clear();
		putative_worms[i].context_image().absolute_grayscale.clear();
		putative_worms[i].context_image().relative_grayscale.clear();
		putative_worms[i].context_image().combined_image.clear();
		putative_worms[i].edge_bitmap().clear();
	}
}
void ns_image_worm_detection_results::clear(){
	 source_image_id = 0;
	 capture_sample_id = 0;
	 capture_time = 0;
	 data_storage_on_disk = ns_image_server_image();
	 detection_results_id = 0;
	 mutually_exclusive_worm_groups.clear();
	 interpolated_worm_areas.clear();
	 region_info_id = 0;
	 region_labels.clear();
	 worm_collage.clear();
	 putative_worms.clear();
	 actual_worms.clear();
	 not_worms.clear();
	 worm_collage.clear();
	 data_storage_on_disk = ns_image_server_image();
}

void ns_calculate_res_aware_edges(ns_image_bitmap & im, ns_image_bitmap & edge_bitmap, std::vector<ns_vector_2d> & output_coordinates, 
	std::vector<ns_vector_2d> & holes, std::vector<ns_edge_ui> & edge_list, std::vector<ns_edge_2d> &edges,
	std::stack<ns_vector_2i> & temp_flood_fill_stack, ns_image_bitmap & temp){

	bool find_holes(im.properties().resolution <= 1201);
	ns_find_edge_coordinates(im,edge_bitmap,output_coordinates,holes,edge_list,temp_flood_fill_stack,temp,find_holes);
	edges.resize(edge_list.size());
	for (unsigned int i = 0; i < edge_list.size(); i++)
		edges[i] = ns_edge_2d(output_coordinates[edge_list[i].vertex[0]],output_coordinates[edge_list[i].vertex[1]]);
}
/*
void ns_calculate_res_aware_edges(ns_image_bitmap & im, ns_image_bitmap & edge_bitmap, std::vector<ns_edge_2d> &edges, std::stack<ns_vector_2i> temp_flood_fill_stack, ns_image_bitmap & temp){
	std::vector<ns_vector_2d> tmp1,tmp2;
	std::vector<ns_edge_ui> tmp3;
	 ns_calculate_res_aware_edges(im,edge_bitmap,tmp1,tmp2,tmp3,edges,temp_flood_fill_stack,temp);
}*/
