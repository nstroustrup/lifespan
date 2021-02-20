#ifndef ns_subpixel_image_alignment
#define ns_subpixel_image_alignment
#include "ns_image.h"
#include "ns_vector.h"
#include <queue>

#define USE_INTEL_IPP

struct ns_added_element {
	ns_added_element() {}
	ns_added_element(const unsigned long& id_, const ns_vector_2i& shift_) :shift(shift_), id(id_) {}
	ns_added_element(const unsigned long& id_) :shift(0,0), id(id_) {}
	unsigned long id;
	ns_vector_2i shift;
};
struct ns_alignment_state {
	ns_alignment_state() :registration_offset_count(0),debug_out(0),registration_offset_sum(0,0), cumulative_recentering_shift(0,0){}
	void clear();
	ns_image_whole<double> consensus;				//the sum of all pixels at this location in the time-average consensus kernal we're aligning to
	ns_image_whole<ns_16_bit> consensus_count;		//the number of images contributing to this location
	ns_image_whole<ns_16_bit> consensus_thresh;		//the sum of all image's thresholds at this point (used for finding the center of mass of only the worm
	ns_image_whole<double> current_round_consensus;		//used as temporary storage during subpixel image registration
	ns_vector_2d registration_offset_sum;
	unsigned long registration_offset_count;
	ns_vector_2d cumulative_recentering_shift;

	inline ns_vector_2d registration_offset_average() { return registration_offset_sum / (double)registration_offset_count; }
	
	std::deque<ns_added_element> element_occupancy_ids;	//stores info about the images that are already added to the consensus images,
														//making them easier to remove
	std::ofstream * debug_out;
	~ns_alignment_state() { ns_safe_delete(debug_out); }
};

#ifdef USE_INTEL_IPP

//builds a gausian pyramid and compares image gradients to solve for optimal alignment at each pyramid level
class ns_gaussian_pyramid;
class ns_gradient_shift;
class ns_calc_best_alignment_fast {
public:
	typedef float ns_difference_type;
	//images can be shifted a maximum of 2^(ns_max_pyramid_size+1) times
	enum { ns_max_pyramid_size = 30 };
	ns_calc_best_alignment_fast(const ns_vector_2i & max_offset_, const ns_vector_2i &bottom_offset_, const ns_vector_2i &size_offset_);
	~ns_calc_best_alignment_fast();
	ns_vector_2d operator()(const ns_vector_2d & initial_alignment,const ns_vector_2d & max_alignment, ns_alignment_state & state, const ns_image_standard & image, bool & saturated_offset, const ns_vector_2i & subregion_pos, const ns_vector_2i & subregion_size, const bool only_vertical=false,const std::string & dbg_info="");

	ns_vector_2d operator()(const ns_vector_2d & initial_alignment, const ns_vector_2d & max_alignment, const ns_gaussian_pyramid * state_pyramid, const ns_gaussian_pyramid *image_pyramid, bool & saturated_offset, const bool only_vertical=false);
	void minimize_memory_use(bool min) { minimize_memory_use_ = min; }

	void clear();

	//xxx
	/*ns_image_whole<float> grad_x[ns_max_pyramid_size];
	ns_image_whole<float> grad_y[ns_max_pyramid_size];
	ns_image_whole<float> diff[ns_max_pyramid_size];*/


	//xxx
	//ns_vector_2d debug_gold_standard_shift;

private:
	const ns_vector_2<float> max_offset, bottom_offset, size_offset;
	bool minimize_memory_use_;
	ns_gaussian_pyramid * state_pyramid, *image_pyramid;
	ns_gradient_shift * gradient_shift;

};

struct ns_stretch_registration_line_offsets {
	std::vector<float> p;
};
struct ns_stretch_source_mappings {
	std::vector<float> p;
};
class ns_stretch_registration {
public:
	double calculate(const ns_image_standard & im1, const ns_image_standard & im2, 
		const ns_vector_2i & tl, const ns_vector_2i & br,
		const ns_vector_2d &initial_offset, ns_stretch_registration_line_offsets & new_line_offsets, const float * histogram_matching_factors=0);

	static void convert_offsets_to_source_positions(const ns_stretch_registration_line_offsets & offsets, ns_stretch_source_mappings & mappings);
	static void register_image(const ns_stretch_source_mappings & mappings, const double x_shift, const ns_image_standard & im1,  ns_image_standard & im2);

	~ns_stretch_registration();
	ns_stretch_registration();
private:
	ns_gradient_shift * gradient_shift;
};
#endif

class ns_calc_best_alignment {
	typedef float ns_difference_type;
public:
	ns_calc_best_alignment(const ns_vector_2d & corse_step_, const ns_vector_2d & fine_step_, const ns_vector_2i & max_offset_, const ns_vector_2i &local_offset_, const ns_vector_2i &bottom_offset_, const ns_vector_2i &size_offset_) :
		max_offset(max_offset_), local_offset(local_offset_), bottom_offset(bottom_offset_), size_offset(size_offset_), corse_step(corse_step_), fine_step(fine_step_) {}

	ns_vector_2d operator()(ns_alignment_state & state, const ns_image_standard & image, bool & saturated_offset);
private:
	const ns_vector_2d corse_step, fine_step;
	const ns_vector_2d max_offset, local_offset, bottom_offset, size_offset;

};

#endif