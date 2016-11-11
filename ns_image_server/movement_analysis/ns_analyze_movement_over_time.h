#ifndef NS_ANALYZE_MOVEMENT_OVER_TIME
#define NS_ANALYZE_MOVEMENT_OVER_TIME
#include "ns_image_server.h"
#include "ns_processing_job.h"

void analyze_worm_movement_across_frames(const ns_processing_job & job, ns_image_server * image_server, ns_sql & sql, bool log_output);

#endif