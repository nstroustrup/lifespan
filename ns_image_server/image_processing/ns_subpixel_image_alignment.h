#ifndef ns_subpixel_image_alignment
#define ns_subpixel_image_alignment
#include "ns_image.h"
#include "ns_vector.h"
#include <queue>

struct ns_alignment_state {
	ns_alignment_state() :consensus_internal_offset(0, 0),registration_offset_count(0),registration_offset_sum(0,0) {}
	void clear();
	ns_image_whole<double> consensus;
	ns_image_whole<ns_16_bit> consensus_count;
	ns_image_whole<float> current_round_consensus;
	ns_vector_2d registration_offset_sum;
	unsigned long registration_offset_count;
	const ns_vector_2i consensus_internal_offset;
	inline ns_vector_2d registration_offset_average() { return registration_offset_sum / (double)registration_offset_count; }
};

#define USE_INTEL_IPP
#ifdef USE_INTEL_IPP
//builds a gausian pyramid and compares image gradients to solve for optimal alignment at each pyramid level
class ns_gaussian_pyramid;
class ns_gradient_shift;
class ns_calc_best_alignment_fast {
public:
	typedef float ns_difference_type;
	//images can be shifted a maximum of 2^(ns_max_pyramid_size+1) times
	enum { ns_max_pyramid_size = 30 };
	ns_calc_best_alignment_fast(const ns_vector_2i & max_offset_, const ns_vector_2i &local_offset_, const ns_vector_2i &bottom_offset_, const ns_vector_2i &size_offset_);
	~ns_calc_best_alignment_fast();
	ns_vector_2d operator()(const ns_vector_2d & initial_alignment,const ns_vector_2d & max_alignment, ns_alignment_state & state, const ns_image_standard & image, bool & saturated_offset);

	void clear();

	//xxx
	/*ns_image_whole<float> grad_x[ns_max_pyramid_size];
	ns_image_whole<float> grad_y[ns_max_pyramid_size];
	ns_image_whole<float> diff[ns_max_pyramid_size];*/


	//xxx
	ns_vector_2d debug_gold_standard_shift;

private:
	const ns_vector_2<float> max_offset, local_offset, bottom_offset, size_offset;

	ns_gaussian_pyramid * state_pyramid, *image_pyramid;
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