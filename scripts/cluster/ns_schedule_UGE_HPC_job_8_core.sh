#!/bin/bash
#$ -e /users/nstroustrup/nstroustrup/qsub/lifespan/lifespan_machine_$JOB_ID_$HOSTNAME_$TASK_ID_errors.txt
#$ -o /users/nstroustrup/nstroustrup/qsub/lifespan/lifespan_machine_$JOB_ID_$HOSTNAME_$TASK_ID_output.txt
#$ -l virtual_free=16G,h_rt=72:00:00
#$ -pe smp 8
#$ -N ns_im_8    #job name
#$ -q long-sl7  #queue
#$ -t 1-10
#try to stop after 70 hours walltime, giving two hours for any pending jobs to run before hard stop at 72h 
#stop after 50 checks of an empty job queue
#run using eight cores and 16 gigabytes of memory
/users/nstroustrup/nstroustrup/projects/lifespan/bin/ns_image_server max_run_time_in_seconds 252000 idle_queue_check_limit 50 number_of_processor_cores_to_use 8 max_memory_to_use 16384 additional_host_description 8_core:job:$JOB_ID:$SGE_TASK_ID
