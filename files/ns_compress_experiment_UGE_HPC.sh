#!/bin/bash
#$ -N to_jp2k
#$ -cwd 
#$ -q short-sl7
#$ -l virtual_free=8G,h_rt=6:00:00
#$ -t 1-20
#$ -j yes
#Compress all raw image files in a lifespan machine experiment to jpeg2000
#using UGE batch system array jobs
#Place this this script in the base directory of an experiment
#eg /long_term_storage/partition_000/my_experiment/ns_compress_experiment_UGE_HPC.sh
#and run it.  Subdirectories will be traversed and images compressed from TIF to jpeg2000
#using the opj_compress command
#The LD_LIBRARY_PATH definition specifies the location for the openjpeg2000 shared libraries
LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/users/nstroustrup/nstroustrup/libs/lib
export LD_LIBRARY_PATH

num_tasks=$(expr $SGE_TASK_LAST - $SGE_TASK_FIRST)
num_tasks=$(expr $num_tasks + 1)
jid=$SGE_TASK_ID
num_dirs=$(ls -l | grep ^d | wc -l)
folders_per_job=$(expr $num_dirs / $num_tasks);
ddr=$(expr $num_dirs % $num_tasks);
if [ $ddr -ne 0 ]
then
	folders_per_job=$(expr $folders_per_job + 1)
fi
job_start=$(expr $jid - 1)
job_start=$(expr $job_start \* $folders_per_job)
job_end=$(expr $job_start + $folders_per_job)

f_num=-1
for smp in $(ls);
do 
	if [ -f $smp ]
	then 
		continue;
	fi
	f_num=$(expr $f_num + 1)
	if [ $f_num -lt $job_start ]
	then
		continue      
	fi
	if [ $f_num -ge $job_end ]
	then
		continue
	fi
	#echo $f_num $smp
	#continue;
	for reg in $(ls $smp)
	do
		if [ -f $reg ]
			then 
				continue;
		fi

		if [ $reg == 'captured_images' ]
		then
			for d in $(ls $smp/$reg/*.tif)
			do
			    b='./'
			    b+=$d
			    echo $b 
			    b2=$b
			    b2+='.jp2'
			   /users/nstroustrup/nstroustrup/libs/bin/opj_compress -i $b -o $b2 -r 20
			   if [ -s $b2 ]
			   then
			   	rm $b
			   fi
			   sleep 1
			done
		else
			for d in $(ls $smp/$reg/unprocessed/*.tif)
						do
						    b='./'
						    b+=$d
						    echo $b 
						    b2=$b
						    b2+='.jp2'
						   /users/nstroustrup/nstroustrup/libs/bin/opj_compress -i $b -o $b2 -r 20
						   if [ -s $b2 ]
						   then
						   	rm $b
						   fi
						  sleep 1
			done
		fi
	   	
  	done
 done

