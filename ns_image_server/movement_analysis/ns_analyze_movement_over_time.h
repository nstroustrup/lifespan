#ifndef NS_ANALYZE_MOVEMENT_OVER_TIME
#define NS_ANALYZE_MOVEMENT_OVER_TIME
#include "ns_image_server.h"
#include "ns_processing_job.h"

void analyze_worm_movement_across_frames(const ns_processing_job & job, ns_image_server * image_server, ns_sql & sql, bool log_output,long specific_worm=-1);

void ns_refine_image_statistics(const ns_64_bit region_id, const bool recalculate_worm_morphology_from_images, std::ostream & out, ns_sql & sql);
#endif