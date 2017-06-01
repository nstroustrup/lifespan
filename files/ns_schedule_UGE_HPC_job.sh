#!/bin/bash
#$ -e /users/nstroustrup/nstroustrup/qsub/lifespan/lifespan_machine_$JOB_ID_$HOSTNAME_$TASK_ID_errors.txt
#$ -o /users/nstroustrup/nstroustrup/qsub/lifespan/lifespan_machine_$JOB_ID_$HOSTNAME_$TASK_ID_output.txt
#$ -l virtual_free=8G,h_rt=6:00:00
#$ -pe smp 4
#$ -N ns_im_4    #job name
#$ -q short-sl7  #queue
#$ -t 1-50
#try to stop after 5 hours walltime, giving one hour for any pending jobs to run before hard stop at 6h 
#stop after 50 checks of an empty job queue
#run using four cores and 8*1024 megabytes of memory
/users/nstroustrup/nstroustrup/projects/lifespan/bin/ns_image_server max_run_time_in_seconds 18000 idle_queue_check_limit 50 number_of_processor_cores_to_use 4 max_memory_to_use 8192 additional_host_description job:$JOB_ID:$SGE_TASK_ID
