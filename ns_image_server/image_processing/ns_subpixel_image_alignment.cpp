#include "ns_subpixel_image_alignment.h"
#ifdef NS_USE_INTEL_IPP
#include "ns_gaussian_pyramid.h"
#endif
#include "ns_image_easy_io.h"

#undef max
#include <limits>
#include <limits.h>
#include <cmath>
#include <algorithm>
//cholesky matrix inversion from http://www.sci.utah.edu/~wallstedt/LU.htm
// Crout uses unit diagonals for the upper triangle

// Cholesky requires the matrix to be symmetric positive-definite
void ns_Cholesky(int d, double*S, double*D) {
	for (int k = 0; k<d; ++k) {
		double sum = 0.;
		for (int p = 0; p<k; ++p)sum += D[k*d + p] * D[k*d + p];
		D[k*d + k] = sqrt(S[k*d + k] - sum);
		for (int i = k + 1; i<d; ++i) {
			double sum = 0.;
			for (int p = 0; p<k; ++p)sum += D[i*d + p] * D[k*d + p];
			D[i*d + k] = (S[i*d + k] - sum) / D[k*d + k];
		}
	}
}
void ns_solveCholesky(int d, double*LU, double*b, double*x, double *y) {
	for (int i = 0; i<d; ++i) {
		double sum = 0.;
		for (int k = 0; k<i; ++k)sum += LU[i*d + k] * y[k];
		y[i] = (b[i] - sum) / LU[i*d + i];
	}
	for (int i = d - 1; i >= 0; --i) {
		double sum = 0.;
		for (int k = i + 1; k<d; ++k)sum += LU[k*d + i] * x[k];
		x[i] = (y[i] - sum) / LU[i*d + i];
	}
}


void ns_Cholesky(int d, double*S, double*D);
void ns_solveCholesky(int d, double*LU, double*b, double*x, double *y);

class ns_gradient_shift {
public:
	ns_gradient_shift();
	void clear();
	/*void preallocate(const ns_image_properties &p) {
	grad_x.resize(p);
	grad_y.resize(p);
	diff.resize(p);
	}*/
	//we need to correctly handle initial offset
	template<class T1, class T2>
	//estimate the shift between the images given that s2 is shifted initial_offset over from s1
	ns_vector_2d calc_gradient_shift(const T1 & im1, const T2 & im2,
		const ns_vector_2d &tl_, const ns_vector_2d & br_, const ns_vector_2d & initial_offset, bool & saturated_offset, const float * im_2_histogram_stretch_factors = 0, const bool only_vertical = 0) {


		//bounds check the additional shift.
		ns_vector_2d tl(tl_), br(br_);
		if (tl.x - initial_offset.x < 0) tl.x = initial_offset.x;
		if (tl.y - initial_offset.y < 0) tl.y = initial_offset.y;

		if (br.x - initial_offset.x > im2.properties().width) br.x = im2.properties().width + initial_offset.x;
		if (br.y - initial_offset.y > im2.properties().height) br.y = im2.properties().height + initial_offset.y;
		if (br.x - initial_offset.x > im1.properties().width) br.x = im1.properties().width + initial_offset.x;
		if (br.y - initial_offset.y > im1.properties().height) br.y = im1.properties().height + initial_offset.y;
		if (floor(br.x) <= ceil(tl.x) + 2 || floor(br.y) <= ceil(tl.y) + 2) { //the plus 2s reflect the fact we're running a 3 width kernel and ignoring the 1 pixel-wide boundaries
														saturated_offset = true;
			return initial_offset;
			//throw ns_ex("Invalid shift during gradient registration: (") << initial_offset.x << "," << initial_offset.y << ") {" << tl_ << ";" << br_ << "} [" << im1.properties().width << ", " << im1.properties().height << "]";
		}
		ns_image_properties p(floor(br.y) - ceil(tl.y) - 2, floor(br.x) - ceil(tl.x) - 2, 1);

		

		//todo: alculation of the image gradient can likely be accelerated using the following intel IPP functions
		//WarpBilinear
		//GradientVectorScharr
		//ippiSub_8u

		A[0] = A[1] = A[2] = A[3] = b[0] = b[1] = 0;
		double grad_x, grad_y, diff;
		if (im_2_histogram_stretch_factors == 0) {
			if (only_vertical) {
				for (int y = 0; y < p.height; y++) {
					for (int x = 0; x < p.width; x++) {
						//we want to apply the offset to im2, so we pull pixels offset in the opposite direction of the desired shift
						const double x_(x - initial_offset.x + tl.x + 1),  //the plus one is because the grad_x is 2 smaller than im2, because we ignore the boundaries of the kernel
							y_(y - initial_offset.y + tl.y + 1);
						grad_x = .5*(im2.sample_d(y_, x_ + 1) - im2.sample_d(y_, x_ - 1));
						grad_y = .5*(im2.sample_d(y_ + 1, x_) - im2.sample_d(y_ - 1, x_));

						diff = im2.sample_d(y_, x_) - im1.sample_d(y + tl.y + 1, x + tl.x + 1);
						A[0] += grad_x * grad_x;
						A[1] += grad_x * grad_y;
						A[3] += grad_y * grad_y;
					//	b[0] -= grad_x * diff;
						b[1] -= grad_y * diff;
					}
				}
			}
			else {
				for (int y = 0; y < p.height; y++) {
					for (int x = 0; x < p.width; x++) {
						//we want to apply the offset to im2, so we pull pixels offset in the opposite direction of the desired shift
						const double x_(x - initial_offset.x + tl.x + 1),  //the plus one is because the grad_x is 2 smaller than im2, because we ignore the boundaries of the kernel
							y_(y - initial_offset.y + tl.y + 1);
						grad_x = .5*(im2.sample_d(y_, x_ + 1) - im2.sample_d(y_, x_ - 1));
						grad_y = .5*(im2.sample_d(y_ + 1, x_) - im2.sample_d(y_ - 1, x_));

						diff = im2.sample_d(y_, x_) - im1.sample_d(y + tl.y + 1, x + tl.x + 1);
						A[0] += grad_x * grad_x;
						A[1] += grad_x * grad_y;
						A[3] += grad_y * grad_y;
						b[0] -= grad_x * diff;
						b[1] -= grad_y * diff;
					}
				}
			}
		}
		else {
			if (only_vertical) {
				for (int y = 0; y < p.height; y++) {
					for (int x = 0; x < p.width; x++) {
						//we want to apply the offset to im2, so we pull pixels offset in the opposite direction of the desired shift
						const double x_(x - initial_offset.x + tl.x + 1),  //the plus one is because the grad_x is 2 smaller than im2, because we ignore the boundaries of the kernel
							y_(y - initial_offset.y + tl.y + 1);
						grad_x = .5*(im2.sample_d_scaled(y_, x_ + 1, im_2_histogram_stretch_factors) - im2.sample_d_scaled(y_, x_ - 1, im_2_histogram_stretch_factors));
						grad_y = .5*(im2.sample_d_scaled(y_ + 1, x_, im_2_histogram_stretch_factors) - im2.sample_d_scaled(y_ - 1, x_, im_2_histogram_stretch_factors));

						diff = im2.sample_d_scaled(y_, x_, im_2_histogram_stretch_factors) - im1.sample_d(y + tl.y + 1, x + tl.x + 1);
						A[0] += grad_x * grad_x;
						A[1] += grad_x * grad_y;
						A[3] += grad_y * grad_y;
					//	b[0] -= grad_x * diff;
						b[1] -= grad_y * diff;
					}
				}
			}
			else {
				for (int y = 0; y < p.height; y++) {
					for (int x = 0; x < p.width; x++) {
						//we want to apply the offset to im2, so we pull pixels offset in the opposite direction of the desired shift
						const double x_(x - initial_offset.x + tl.x + 1),  //the plus one is because the grad_x is 2 smaller than im2, because we ignore the boundaries of the kernel
							y_(y - initial_offset.y + tl.y + 1);
						grad_x = .5*(im2.sample_d_scaled(y_, x_ + 1, im_2_histogram_stretch_factors) - im2.sample_d_scaled(y_, x_ - 1, im_2_histogram_stretch_factors));
						grad_y = .5*(im2.sample_d_scaled(y_ + 1, x_, im_2_histogram_stretch_factors) - im2.sample_d_scaled(y_ - 1, x_, im_2_histogram_stretch_factors));

						diff = im2.sample_d_scaled(y_, x_, im_2_histogram_stretch_factors) - im1.sample_d(y + tl.y + 1, x + tl.x + 1);
						A[0] += grad_x * grad_x;
						A[1] += grad_x * grad_y;
						A[3] += grad_y * grad_y;
						b[0] -= grad_x * diff;
						b[1] -= grad_y * diff;
					}
				}
			}
		}

		saturated_offset = false;
		return calc_shifts_from_grad();
	}
private:
	ns_vector_2d calc_shifts_from_grad();
	//ns_image_whole<double> grad_x, grad_y, diff;
	double A[4]; //2x2 matrix
	double b[2];//2x1 vector
	double LU[4];//2x2 matrix (Cholesky decomposition of A)


};


ns_gradient_shift::ns_gradient_shift() {
	/*	diff.use_more_memory_to_avoid_reallocations();
	grad_x.use_more_memory_to_avoid_reallocations();
	grad_y.use_more_memory_to_avoid_reallocations();*/
}
void ns_gradient_shift::clear() {
	/*diff.clear();
	grad_x.clear();
	grad_y.clear();*/
}

ns_vector_2d ns_gradient_shift::calc_shifts_from_grad() {

	A[2] = A[1];

	//calculate the determinant to test that matrix is singular
	//if it is, give up.
	if (A[0] * A[3] - A[1] * A[2] == 0)
		return ns_vector_2d(0, 0);

	double tmp[4];//should only need two doubles but upped while hunting for a memory corruption bug.

	ns_Cholesky(2, A, LU);

	//solve LU*C = b for C.
	double C[2];
	ns_solveCholesky(2, LU, b, C, tmp);

	//if (std::isnan(-C[0]) || std::isnan(-C[1]))
	//	throw ns_ex("Yikes");
	return ns_vector_2d(-C[0], -C[1]);
}


#ifdef NS_USE_INTEL_IPP


ns_stretch_registration::ns_stretch_registration():gradient_shift(0){
}
ns_stretch_registration::~ns_stretch_registration() {
	ns_safe_delete(gradient_shift);
}

double ns_stretch_registration::calculate(const ns_image_standard & im1, const ns_image_standard & im2,
									    const ns_vector_2i & tl, const ns_vector_2i & br,
										const ns_vector_2d &initial_offset, ns_stretch_registration_line_offsets & new_line_offsets, const float * histogram_matching_factors) {
	if (gradient_shift == 0)
		gradient_shift = new ns_gradient_shift();
	const int half_height(50);
	const double max_offset(4);
	new_line_offsets.p.resize(0);
	new_line_offsets.p.resize(br.y - tl.y, 0);
	//iterate to find more optimal shifts
	ns_calc_best_alignment_fast f(ns_vector_2d(4, 4), ns_vector_2i(4, 4), ns_vector_2i(4, 4));
	ns_gaussian_pyramid p1, p2;

	ns_vector_2d last_shift(0, 0);
	for (unsigned int rep = 0; rep < 1; rep++) {

		double sum_x_shifts(0);
		last_shift.y = 0;
		int line_count(0);

		for (unsigned int y = tl.y + half_height; y < br.y - half_height; y += 4) {
			const int yp = y - tl.y;
			bool saturated_offset;
			long top(y + half_height);
			if (top + 1 >= im1.properties().height)
				top = im1.properties().height - 2;
			ns_vector_2i tl(tl.x, y - half_height), br(br.x - 1, top);
			p1.calculate(im1, tl, br - tl);
			p2.calculate(im2, tl, br - tl);
			ns_vector_2d sh = f(
				ns_vector_2d(initial_offset.x, initial_offset.y + new_line_offsets.p[yp]),
				ns_vector_2d(4, 4), &p1, &p2, saturated_offset,true);
			/*ns_vector_2d sh = gradient_shift->calc_gradient_shift(im1, im2,
				ns_vector_2i(tl.x, y - half_height),
				ns_vector_2i(br.x - 1, top),  //subtract 1 to work around sample issue on edges
				ns_vector_2d(initial_offset.x, initial_offset.y +  new_line_offsets.p[yp]),
				saturated_offset,
				histogram_matching_factors);*/
			if (saturated_offset) {
				last_shift.y = 0;
			}
			else {
				if (sh.y < -max_offset) sh.y = -max_offset;
				else if (sh.y > max_offset) sh.y = max_offset;

				if (sh.x < -max_offset) sh.x = -max_offset;
				else if (sh.x > max_offset) sh.x = max_offset;

				new_line_offsets.p[yp] = sh.y + new_line_offsets.p[yp];
				//last_shift.y = sh.y;

			//	sum_x_shifts += sh.x;
				line_count++;
			}
		}
		last_shift.x = sum_x_shifts / line_count;
	}

	//fill in skipped values
	for (unsigned int y = 0; y < half_height; y++)
		new_line_offsets.p[y] = new_line_offsets.p[half_height];
	double ld;
	unsigned int y;
	for (y = tl.y + half_height; y < br.y - half_height-4; y += 4) {
		const double d0 = new_line_offsets.p[y],
				     d1 = new_line_offsets.p[y + 4];
		ld = d1;
		new_line_offsets.p[y + 1] = .75*d0 + .25*d1;
		new_line_offsets.p[y + 2] = .5*d0 + .5*d1;
		new_line_offsets.p[y + 3] = .25*d0 + .75*d1;
	}
	for (; y < new_line_offsets.p.size(); y++)
		new_line_offsets.p[y] = ld;
	return last_shift.x;
}



template<class T>
class ns_sort_first {
public:
	bool operator() (const T & a, const T & b) const { return a.first < b.first; }
};


void ns_stretch_registration::convert_offsets_to_source_positions(const ns_stretch_registration_line_offsets & offsets, ns_stretch_source_mappings & mappings) {
	//first order the source-destination position pairs by their destination position.
	std::vector<std::pair<float, int>> pos(offsets.p.size());
	for (unsigned int i = 0; i < offsets.p.size(); i++) {
		pos[i].first = i + offsets.p[i];  //destination
		pos[i].second = i;  //source
	}
	mappings.p.resize(0);
	mappings.p.resize(offsets.p.size(), 0);
	std::vector<int > new_mapping_counts(offsets.p.size(), 0);
	std::sort(pos.begin(), pos.end(), ns_sort_first<std::pair<float, int> >());
	//now we "invert' the plot, finding the fractional source positions that correspond to each integral destination position

	//i is the destination position--eg the integer position in the new (registered) image
	//for each i, we need to find the best source position in the old (unregistered) image
	for (unsigned long i = 1; i < pos.size(); i++) {
		if (pos[i - 1].first == pos[i].first)
			continue;
		int  y0(ceil(pos[i - 1].first)),  //the smallest integer inbetween pos[i-1] and pos[i]
			y1(floor(pos[i].first));	  //the largest integer inbetween pos[i-1] and pos[i]

		if (y1 >= pos[i].first) {
			y1--;
		}
		if (y1 < 0 || y0 >= offsets.p.size()) //out of bounds position
			continue;

		//if the current segment crosses one or more integer positions, include it.
		if (y1 >= pos[i - 1].first && y0 < pos[i].first) {
			if (y0 < 0) y0 = 0;
			if (y1 + 1 >= offsets.p.size())
				y1 = offsets.p.size() - 1;

			//go through each integer destination position that crosses inbetween the two source positions
			for (unsigned int y = y0; y <= y1; y++) {
				//linearly interpolate between the two source positions to find the intermediate source position at which the integer destination position crosses them
				float dist((y - pos[i - 1].first) / (pos[i].first - pos[i - 1].first));
				mappings.p[y] += (1-dist)*pos[i - 1].second + (dist)*pos[i].second;
				new_mapping_counts[y]++;
			}
		}
	}
	//calculate average source position at each integral destination position and collect the information needed to fill in unspecified edges
	float first_specified_val(0), last_specified_val(0);
	unsigned long first_specified_pos(mappings.p.size()), last_specified_pos(0);
	for (unsigned int i = 0; i < mappings.p.size(); i++) {
		if (new_mapping_counts[i] == 0)
			continue;
		mappings.p[i] /= new_mapping_counts[i];
		if (first_specified_pos > i) {
			first_specified_pos = i;
			first_specified_val = mappings.p[i];
		}
		if (last_specified_pos < i) {
			last_specified_pos = i;
			last_specified_val = mappings.p[i];
		}
	}
	for (unsigned int i = 0; i < first_specified_pos; i++)
		mappings.p[i] = first_specified_val;
	for (unsigned int i = last_specified_pos + 1; i < mappings.p.size(); i++)
		mappings.p[i] = last_specified_val;
}

 void ns_stretch_registration::register_image(const ns_stretch_source_mappings & mappings, const double x_shift, const ns_image_standard & im1, ns_image_standard & im2) {
	 im2.init(im1.properties());
	 for (unsigned int y = 0; y < im2.properties().height; y++) {
		 for (unsigned int x = 0; x < im2.properties().width; x++) {
			 float x_ = x + x_shift;
			 if (x_ < 0) x_ = 0;
			 if (x_ >= im2.properties().width - 1)
				 x_ = im2.properties().width - 1.001;
			 float y_ = mappings.p[y];
			 if (y_ + 1 >= im2.properties().height)
				 y_ = im2.properties().height - 1.001;
			 im2[y][x] = im1.sample_f(y_, x_);
		 }
	 }
}

ns_calc_best_alignment_fast::ns_calc_best_alignment_fast(const ns_vector_2i & max_offset_, const ns_vector_2i &bottom_offset_, const ns_vector_2i &size_offset_) :
	max_offset(max_offset_), image_pyramid(0), state_pyramid(0), gradient_shift(0), bottom_offset(bottom_offset_), size_offset(size_offset_) {
	state_pyramid = new ns_gaussian_pyramid();
	image_pyramid = new ns_gaussian_pyramid();
	gradient_shift = new ns_gradient_shift;
}
ns_calc_best_alignment_fast::~ns_calc_best_alignment_fast() {
	ns_safe_delete(state_pyramid);
	ns_safe_delete(image_pyramid);
	ns_safe_delete(gradient_shift);
}

void ns_calc_best_alignment_fast::clear() {
	state_pyramid->clear();
	image_pyramid->clear();
	gradient_shift->clear();
}



int fast_alignment_debug_id = 0;

float ns_pos_part(float a) { return a > 0 ? a : 0; }
float ns_neg_part(float a) { return a < 0 ? -a : 0; }

#ifdef NS_DEBUG_FAST_IMAGE_REGISTRATION
ofstream  * pyramid_buffer = 0;

struct ns_path_debug_info {
	unsigned long path_id;
	unsigned long group_id;
	unsigned long time;
};
ns_path_debug_info global_path_debug_info;
#endif
void ns_align_two_gaussian_pyramids(ns_gaussian_pyramid * state_pyramid,
	ns_gaussian_pyramid * image_pyramid,
	ns_gradient_shift * gradient_shift,
	const ns_vector_2i & bottom_offset) {

}

ns_vector_2d ns_calc_best_alignment_fast::operator()(const ns_vector_2d & initial_alignment, const ns_vector_2d & max_alignment, const ns_gaussian_pyramid * state_pyramid, const ns_gaussian_pyramid *image_pyramid, bool & saturated_offset, const bool only_vertical) {

	if (state_pyramid->properties() != image_pyramid->properties())
		throw ns_ex("Pyramid sizes don't match: state (") << state_pyramid->properties().width << "," << state_pyramid->properties().height << "," << state_pyramid->properties().components << "," << state_pyramid->properties().resolution << ") "
				                                << "im (" << image_pyramid->properties().width << "," << image_pyramid->properties().height << "," << image_pyramid->properties().components << "," << image_pyramid->properties().resolution << ")";
	const ns_vector_2d tl(bottom_offset),
		br((long)state_pyramid->properties().width - size_offset.x,
		(long)state_pyramid->properties().height - size_offset.y);
	//build an image pyramid
	ns_vector_2d shift[ns_max_pyramid_size];
	//walk down the image pyrmaid.  reg updates as the most accurate registration at the next level of the pyramid, based on the levels above.
	ns_vector_2d reg;

	///xxx
#ifdef NS_DEBUG_FAST_IMAGE_REGISTRATION
	vector<ns_image_whole<float > > grad_x(state_pyramid->num_current_pyramid_levels),
		grad_y(state_pyramid->num_current_pyramid_levels),
		diff(state_pyramid->num_current_pyramid_levels);
	if (pyramid_buffer == 0) {
		pyramid_buffer = new ofstream("c:\\server\\pyramid_data.csv");
		*pyramid_buffer << "group_id,path_id,time,human_time,id,downsample factor,iteration,lowest_resolution_level,A0,A1,A2,A3,B0,B1,L0,L1,L2,L3,C1 cur it,C2 cur it, C1 cumulative, C2 cumulative,Rx,Ry,Sx,Sy,diff\n";
	}
#endif
	unsigned long number_of_levels((state_pyramid->num_current_pyramid_levels < image_pyramid->num_current_pyramid_levels) ? state_pyramid->num_current_pyramid_levels : image_pyramid->num_current_pyramid_levels);
	unsigned long debug_number_of_levels_processed(0);
	bool lowest_resolution_level(true);

	//saves us having to reallocate memory each time we step down the pyramid (look at a larger size)
	//	gradient_shift->preallocate(state_pyramid->image_scaled[0].properties());

	bool err(false);
	for (int i = number_of_levels - 1; i >= 0 && !err; i--) {
		int fold = pow(2, i);
		if (lowest_resolution_level)
			reg = initial_alignment / fold; //start at best guess
		else
			reg = reg * 2;

		try {
			shift[i].x = shift[i].y = 0;
			//search around three times at the lowest resolution to iterate towards the best shift.
			//this lets us identify larger shifts.
			const int num_iterations_this_round((lowest_resolution_level) ? 4 : 1);
			for (unsigned int j = 0; j < num_iterations_this_round; j++) {
				
				ns_vector_2d sh = gradient_shift->calc_gradient_shift(state_pyramid->image_scaled[i],
					image_pyramid->image_scaled[i],
					tl / fold, br / fold, reg + shift[i], saturated_offset,0,only_vertical);
				if (saturated_offset) {
					std::cerr << "Gradient shift saturated at level " << i << "\n";
					err = true;
					break;
				}
				//reject too-large shifts;
				//gradient registration only works well for small
				//deviations around the set point.
				if (fabs(sh.x) > 3 ||
					fabs(sh.y) > 3) {
					if (lowest_resolution_level) {
						saturated_offset = true;
						std::cerr << "Saturated due to too-large shift at level " << i << "\n";
					}
					err = true;
					break;
				}

				shift[i] += sh;
#ifdef NS_DEBUG_FAST_IMAGE_REGISTRATION
				*pyramid_buffer << global_path_debug_info.group_id << "," <<
					global_path_debug_info.path_id << "," <<
					global_path_debug_info.time << "," <<
					ns_format_time_string(global_path_debug_info.time) << "," <<
					fast_alignment_debug_id << "," <<
					fold << "," << j <<
					"," << (lowest_resolution_level ? "lowest" : "not_lowest") << "," <<

					gradient_shift->A[0] << "," <<
					gradient_shift->A[1] << "," <<
					gradient_shift->A[2] << "," <<
					gradient_shift->A[3] << ",";

				*pyramid_buffer <<
					gradient_shift->b[0] << "," <<
					gradient_shift->b[1] << ",";

				*pyramid_buffer <<
					gradient_shift->LU[0] << "," <<
					gradient_shift->LU[1] << "," <<
					gradient_shift->LU[2] << "," <<
					gradient_shift->LU[3] << ",";

				*pyramid_buffer <<
					-sh.x << "," <<
					-sh.y << "," <<
					-shift[i].x << "," <<
					-shift[i].y << "," << (err ? "ERR" : "") << ",";
				*pyramid_buffer << reg.x*fold << "," << reg.y*fold << ",";

				*pyramid_buffer << debug_gold_standard_shift.x << "," << debug_gold_standard_shift.y << "," << (reg - debug_gold_standard_shift / fold).mag() << "\n";
#endif

			}
			lowest_resolution_level = false;
			reg += shift[i];
			
			debug_number_of_levels_processed++;


			if (std::isnan(shift[i].x) || std::isnan(shift[i].y))
				throw ns_ex("Registration produced a nan!");
			
		}
		catch (ns_ex & ex) {

			std::cerr << ex.text() << "\n";

#ifdef NS_DEBUG_FAST_IMAGE_REGISTRATION
			ns_image_properties prop(0, 0, 1);
			for (unsigned int i = 0; i < state_pyramid->num_current_pyramid_levels; i++) {
				unsigned long w(state_pyramid->image_scaled[i].properties().width + image_pyramid->image_scaled[i].properties().width);
				unsigned long h(state_pyramid->image_scaled[i].properties().height);
				if (h < image_pyramid->image_scaled[i].properties().height)
					h = image_pyramid->image_scaled[i].properties().height;
				if (prop.width < w)
					prop.width = w;
				prop.height += h;
			}
			ns_image_whole<float> dbg;
			dbg.init(prop);
			for (unsigned int y = 0; y < prop.height; y++)
				for (unsigned int x = 0; x < prop.width; x++)
					dbg[y][x] = 0;
			unsigned long h(0);
			for (unsigned int i = 0; i < state_pyramid->num_current_pyramid_levels; i++) {
				for (unsigned int y = 0; y < image_pyramid->image_scaled[i].properties().height; y++)
					for (unsigned int x = 0; x < image_pyramid->image_scaled[i].properties().width; x++)
						dbg[h + y][x] = image_pyramid->image_scaled[i].val(y, x);
				unsigned long w(image_pyramid->image_scaled[i].properties().width);
				for (unsigned int y = 0; y < state_pyramid->image_scaled[i].properties().height; y++)
					for (unsigned int x = 0; x < state_pyramid->image_scaled[i].properties().width; x++)
						dbg[h + y][x + w] = state_pyramid->image_scaled[i].val(y, x);
				unsigned long dh(state_pyramid->image_scaled[i].properties().height);
				if (dh < image_pyramid->image_scaled[i].properties().height)
					dh = image_pyramid->image_scaled[i].properties().height;
				h += dh;
			}

			ns_save_image("c:\\server\\pyramid_" + ns_to_string(fast_alignment_debug_id) + ".tif", dbg);

			if (debug_number_of_levels_processed > 0) {
				prop.height = 0;
				prop.width = 3 * (grad_x[number_of_levels - debug_number_of_levels_processed].properties().width + 50);
				prop.components = 3;
				for (int i = 0; i < debug_number_of_levels_processed; i++) {
					prop.height += grad_x[number_of_levels - 1 - i].properties().height + 50;
				}
				dbg.init(prop);
				for (unsigned int y = 0; y < prop.height; y++)
					for (unsigned int x = 0; x < 3 * prop.width; x++)
						dbg[y][x] = 0;
				h = 0;
				for (int ii = 0; ii < debug_number_of_levels_processed; ii++) {
					int i = number_of_levels - 1 - ii;
					unsigned long w(grad_x[i].properties().width + 50);
					for (unsigned int y = 0; y < grad_x[i].properties().height; y++)
						for (unsigned int x = 0; x < grad_x[i].properties().width; x++) {
							dbg[h + y][3 * x] = 255 * ns_pos_part(grad_x[i][y][x]);
							dbg[h + y][3 * x + 1] = 255 * ns_neg_part(grad_x[i][y][x]);

							dbg[h + y][3 * (x + w)] = 255 * ns_pos_part(grad_y[i][y][x]);
							dbg[h + y][3 * (x + w) + 1] = 255 * ns_neg_part(grad_y[i][y][x]);

							dbg[h + y][3 * (x + 2 * w)] = 255 * ns_pos_part(diff[i][y][x]);
							dbg[h + y][3 * (x + 2 * w) + 1] = 255 * ns_neg_part(diff[i][y][x]);

						}
					h += grad_x[i].properties().height + 50;
				}
				ns_save_image("c:\\server\\pyramid_grad_" + ns_to_string(fast_alignment_debug_id) + ".tif", dbg);
			}
#endif
			gradient_shift->clear();
			return initial_alignment;
		}
	}
	if (err) {
		gradient_shift->clear();
		return initial_alignment;
	}
#ifdef NS_DEBUG_FAST_IMAGE_REGISTRATION
	pyramid_buffer->flush();
#endif
	if (minimize_memory_use_)
		gradient_shift->clear();
	fast_alignment_debug_id++;
	if (reg.x < -max_alignment.x) { reg.x = -max_alignment.x; saturated_offset = true; }
	if (reg.y < -max_alignment.y) { reg.y = -max_alignment.y; saturated_offset = true; }
	if (reg.x > max_alignment.x) { reg.x = max_alignment.x; saturated_offset = true; }
	if (reg.y > max_alignment.y) { reg.y = max_alignment.y; saturated_offset = true; }
	return reg;
}
ns_vector_2d ns_calc_best_alignment_fast::operator()(const ns_vector_2d & initial_alignment, const ns_vector_2d & max_alignment, ns_alignment_state & state, const ns_image_standard & image, bool & saturated_offset, const ns_vector_2i & subregion_pos, const ns_vector_2i & subregion_size, const bool only_vertical,const std::string & dbg) {

	saturated_offset = false;
	//we want to register only the region where 1) the current frame is defined (e.g not the empty margins of the path aligned image)
	//and 2) the state consensus is also defined (e.g there have been some pixels measured recently)

	state.current_round_consensus.init(state.consensus.properties());
	ns_vector_2i min_non_zero_consensus(INT_MAX, INT_MAX);
	ns_vector_2i max_non_zero_consensus(0, 0);
	unsigned long count(0);
	for (unsigned int y = subregion_pos.y; y < subregion_pos.y + subregion_size.y; y++) {
		for (unsigned int x = subregion_pos.x; x < subregion_pos.x + subregion_size.x; x++) {
			const bool z(state.consensus_count[y][x] != 0);
			state.current_round_consensus[y][x] = z ? (state.consensus[y][x] / (ns_difference_type)state.consensus_count[y][x]) : 0;

			if (z && state.current_round_consensus[y][x] != 0) {
				count++;
				if (min_non_zero_consensus.x > x) min_non_zero_consensus.x = x;
				if (min_non_zero_consensus.y > y) min_non_zero_consensus.y = y;
				if (max_non_zero_consensus.x < x) max_non_zero_consensus.x = x;
				if (max_non_zero_consensus.y < y) max_non_zero_consensus.y = y;
			}
		}
	}
	//if there is a very small overlap between the state and the current image,  use the whole subregion
	if (count < 16 * 16) {
		min_non_zero_consensus = subregion_pos;
		max_non_zero_consensus = subregion_pos + subregion_size;
	}
	else {
		//find the smallest bounding box in which both conditions specified above are met, and build image pyramids from only that.
		const ns_vector_2i subregion_max(subregion_pos + subregion_size);
		if (subregion_pos.x > min_non_zero_consensus.x) min_non_zero_consensus.x = subregion_pos.x;
		if (subregion_pos.y > min_non_zero_consensus.y) min_non_zero_consensus.y = subregion_pos.y;
		if (subregion_max.x < max_non_zero_consensus.x) max_non_zero_consensus.x = subregion_max.x;
		if (subregion_max.y < max_non_zero_consensus.y) max_non_zero_consensus.y = subregion_max.y;
	}
	const ns_vector_2i non_zero_size(max_non_zero_consensus - min_non_zero_consensus);

	state_pyramid->calculate(state.current_round_consensus, min_non_zero_consensus, non_zero_size);
	image_pyramid->calculate(image, min_non_zero_consensus, non_zero_size);
	/*
	//output a debug image showing alignment
	ns_image_standard cmp;
	ns_image_properties prop(
		state_pyramid->image_scaled[1].properties().height,
		state_pyramid->image_scaled[1].properties().width, 3);
	cmp.init(prop);
	for (unsigned int y = 0; y < prop.height; y++)
		for (unsigned int x = 0; x < prop.width; x++) {
			cmp[y][3*x] = (ns_8_bit)(state_pyramid->image_scaled[1].val(y, x) * 255);
			cmp[y][3*x+1] = (ns_8_bit)(image_pyramid->image_scaled[1].val(y, x) * 255);
			cmp[y][3 * x+2] = 0;
		}
	ns_save_image( "c:\\server\\creg_" + dbg + ".tif",cmp);
	*/
	return (*this)(initial_alignment, max_alignment, state_pyramid, image_pyramid, saturated_offset,only_vertical);

}
#endif

ns_vector_2d ns_calc_best_alignment::operator()(ns_alignment_state & state, const ns_image_standard & image, bool & saturated_offset) {
#ifdef NS_OUTPUT_ALGINMENT_DEBUG
	ofstream o("c:\\tst.txt");
	o << "step,dx,dy,sum,lowest sum\n";
#endif
	ns_difference_type min_diff(std::numeric_limits<ns_difference_type>::max());
	ns_vector_2<float> best_offset(0, 0);
	//xxx
	//return best_offset;
	state.current_round_consensus.init(state.consensus.properties());
	for (unsigned int y = 0; y < state.consensus.properties().height; y++)
		for (unsigned int x = 0; x < state.consensus.properties().width; x++)
			state.current_round_consensus[y][x] = (state.consensus_count[y][x] != 0) ? (state.consensus[y][x] / (ns_difference_type)state.consensus_count[y][x]) : 0;

	//	const ns_vector_2i br_s(image.properties().width-size_offset.x,image.properties().height-size_offset.y);

	//double area_2(((br.y-tl.y+1)/2)*((br.x-tl.x+1)/2));
	const ns_vector_2i tl(bottom_offset),
		br((long)image.properties().width - size_offset.x,
		(long)image.properties().height - size_offset.y);
	ns_difference_type area((br.y - tl.y)*(br.x - tl.x));
	{
		ns_vector_2d offset_range_l(state.registration_offset_average() - local_offset),
			offset_range_h(state.registration_offset_average() + local_offset);
		bool last_triggered_left_search(false),
			last_triggered_right_search(false),
			last_triggered_bottom_search(false),
			last_triggered_top_search(false);
		while (true) {

			saturated_offset = false;
			bool left_saturated(false), top_saturated(false), right_saturated(false), bottom_saturated(false);

			if (offset_range_l.x - 1 + fine_step.x < -max_offset.x) {
				offset_range_l.x = -max_offset.x + 1 - fine_step.x;
				saturated_offset = true;
				left_saturated = true;
			}
			if (offset_range_l.y - 1 + fine_step.y < -max_offset.y) {
				offset_range_l.y = -max_offset.y + 1 - fine_step.y;
				saturated_offset = true;
				top_saturated = true;
			}
			if (offset_range_h.x + 1 - fine_step.x > max_offset.x) {
				offset_range_h.x = max_offset.x - 1 + fine_step.x;
				saturated_offset = true;
				right_saturated = true;
			}
			if (offset_range_h.y + 1 - fine_step.y > max_offset.y) {
				offset_range_h.y = max_offset.y - 1 + fine_step.y;
				saturated_offset = true;
				bottom_saturated = true;
			}


			bool found_new_minimum_this_round(false);
			//very corse first search
			const ns_vector_2i range_l(offset_range_l.x, offset_range_l.y),
				range_h(offset_range_h.x, offset_range_h.y);

			for (int dy = range_l.y; dy <= range_h.y; dy++) {
				for (int dx = range_l.x; dx <= range_h.x; dx++) {
					ns_difference_type sum(0);
					for (long y = tl.y; y < br.y; y++) {
						for (long x = tl.x; x < br.x; x++) {
							sum += fabs(state.current_round_consensus[y + dy][x + dx] - image[y][x]);
						}
					}
					sum /= (area);
					if (min_diff > sum) {
						found_new_minimum_this_round = true;
						min_diff = sum;
						best_offset.y = dy;
						best_offset.x = dx;
					}
#ifdef NS_OUTPUT_ALGINMENT_DEBUG
					o << "coarse," << dx << "," << dy << "," << sum << "," << min_diff << "\n";
#endif
				}
			}
			const double nearness_to_edge_that_triggers_recalculation(2);
			//	bool l(false),r(false),t(false),b(false);
			if (!left_saturated && found_new_minimum_this_round && (abs(best_offset.x - range_l.x) < nearness_to_edge_that_triggers_recalculation)) {
				offset_range_h.x = offset_range_l.x - 1;
				offset_range_l.x -= 2 * local_offset.x - 1;
				//	l=true;
			}
			//A previous bug here (the omission of the ! in front of !top_saturated ) caused
			//movement registration to fail in situations where images were moving up and down a lot,
			//causing animal's lifespan to be overestimated.
			else if (!top_saturated && found_new_minimum_this_round && (abs(best_offset.y - range_l.y) < nearness_to_edge_that_triggers_recalculation)) {
				offset_range_h.y = offset_range_l.y - 1;
				offset_range_l.y -= 2 * local_offset.y - 1;
				//		t=true;
			}
			else if (!right_saturated && found_new_minimum_this_round && (abs(best_offset.x - range_h.x) < nearness_to_edge_that_triggers_recalculation)) {
				offset_range_l.x = offset_range_h.x + 1;
				offset_range_h.x += 2 * local_offset.x + 1;
				//		r=true;
			}
			else if (!bottom_saturated && found_new_minimum_this_round && (abs(best_offset.y - range_h.y) < nearness_to_edge_that_triggers_recalculation)) {
				offset_range_l.y = offset_range_h.y + 1;
				offset_range_h.y += 2 * local_offset.y + 1;
				//	b = true;
			}
			else
				break;
			/*	last_triggered_left_search=l;
			last_triggered_right_search=r;
			last_triggered_bottom_search=b;
			last_triggered_top_search=t;*/
		}
	}
#ifdef NS_DO_SUBPIXEL_REGISTRATION
	//subpixel search
	const ns_vector_2<float> center = best_offset;

	for (ns_difference_type dy = center.y - 1 + corse_step.y; dy < center.y + 1; dy += corse_step.y) {
		for (ns_difference_type dx = center.x - 1 + corse_step.x; dx < center.x + 1; dx += corse_step.x) {
			ns_difference_type sum(0);
			for (long y = tl.y; y < br.y; y++) {
				for (long x = tl.x; x < br.x; x++) {
					//the value of the consensus image is it's mean: consensus[y][x]/consensus_count[y][x]
					sum += fabs(
						state.current_round_consensus.sample_f(y + dy, x + dx)
						- (ns_difference_type)image[y][x]);
				}
			}
			sum /= area;
#ifdef NS_OUTPUT_ALGINMENT_DEBUG
			o << "fine," << dx << "," << dy << "," << sum << "," << min_diff << "\n";
#endif
			if (min_diff > sum) {
				min_diff = sum;
				best_offset.y = dy;
				best_offset.x = dx;
			}
		}
	}

	const ns_vector_2<float> center_2 = best_offset;
	for (ns_difference_type dy = center_2.y - corse_step.y + fine_step.y; dy < center_2.y + corse_step.y; dy += fine_step.y) {
		for (ns_difference_type dx = center_2.x - corse_step.x + fine_step.x; dx < center_2.x + corse_step.x; dx += fine_step.x) {
			ns_difference_type sum(0);
			for (long y = tl.y; y < br.y; y++) {
				for (long x = tl.x; x < br.x; x++) {	//the value of the consensus image is it's mean: consensus[y][x]/consensus_count[y][x]
					sum += fabs(
						state.current_round_consensus.sample_f(y + dy, x + dx) - (ns_difference_type)image[y][x])
						;
				}
			}
			sum /= area;
#ifdef NS_OUTPUT_ALGINMENT_DEBUG
			o << "very fine," << dx << "," << dy << "," << sum << "," << min_diff << "\n";
#endif
			if (min_diff > sum) {
				min_diff = sum;
				best_offset.y = dy;
				best_offset.x = dx;
			}
		}
	}
#endif
	return best_offset;
}
