<?php

$ns_processing_tasks =  array(	'ns_unprocessed'=>0,
				'ns_process_apply_mask' => 1,
				'ns_process_spatial'=>2,
				'ns_process_lossy_stretch'=>3,
				'ns_process_threshold'=>4 ,
				'ns_process_resized_sample_image'=>5,
				'ns_process_worm_detection'=>6,
				'ns_process_worm_detection_labels'=>7,
				'ns_process_worm_detection_with_graph'=>8,
				'ns_process_region_vis'=>9,
				'ns_process_region_interpolation_vis'=>10,
				'ns_process_accept_vis'	=>11,
				'ns_process_reject_vis'=>12,
				'ns_process_interpolation_vis'=>13,
				'ns_process_add_to_training_set'=>14,
				'ns_process_analyze_mask'=>15,
				'ns_process_compile_video'=>16,
				'ns_process_movement_coloring'=>17,
				'ns_process_movement_mapping'=>18,
				'ns_process_posture_vis'=>19,
				'ns_process_movement_coloring_with_graph'=>20,
				'ns_process_heat_map'=>21,
				'ns_process_static_mask'=>22,
				'ns_process_compress_unprocessed'=>23,
				'ns_process_movement_coloring_with_survival'=>24,
				'ns_process_movement_paths_visualization'=>25,
				'ns_process_movement_paths_visualization_with_mortality_overlay'=>26,
				'ns_process_movement_posture_visualization'=>27,
				'ns_process_movement_posture_aligned_visualization'=>28
);

$ns_processing_task_labels = array(0 =>'Unprocessed',
				   1 =>'Apply Mask',
				   2=>'Median Filter',
				   3=>'Intensity Stretch',
				   4=>'Threshold',
				   5=>'Resized Small Image',
				   6=>'Worm Detection',
				   7=>'Worm Detection (Vis)' ,
				   8=>'Worm Detection (Graph)',
				   9=>'Detected Object Data',
				   10=>'Interpolated Region Visualization',
				   11=>'Accepted Worm Visualization' ,
				   12=>'Rejected Worm Visualization',
				   13=>'Interpolated Worm Visualization',
				   14=>'Add Detected Objects to Training Set',
				   15=>'Analyzed Mask',
				   16=>'Compiled Video',
				   17=>'Movement Coloring',
				   18=>'Movement Mappings',
				   19=>'Posture Visualization',
				   20=>'Movement Coloring with Graph',
				   21=>'Heat Map',
				   22=>'Static Mask',
				   23=>'Compress Unprocessed Image for archiving',
				   24=>'Movement Coloring With Survival',
				   25=>'Movement Paths Visualization',
				   26=>'Movement Paths Vis with Mortality Overlay',
				   27=>'Movement Posture Visualization',
				   28=>'Movement Posture Aligned Visualization');

$NS_LAST_PROCESSING_JOB = 28;

$ns_path_image_task = $NS_LAST_PROCESSING_JOB + 1;

$ns_maintenance_tasks = array('ns_maintenance_no_task'=>0,
			      'ns_maintenance_rebuild_sample_time_table'=>1,
			      'ns_maintenance_rebuild_worm_movement_table'=>2,
			      'ns_maintenance_update_processing_job_queue'=>3,
			      'ns_maintenance_delete_files_from_disk_request'=>4,
			      'ns_maintenance_delete_files_from_disk_action'=>5,
			      'ns_maintenance_rebuild_movement_data'=>6,
			      'ns_maintenance_rebuild_movement_from_stored_images'=>7,
			      'ns_maintenance_rebuild_movement_from_stored_image_quantification'=>8,
			      'ns_maintenance_recalculate_censoring'=>9,
			      'ns_maintenance_generate_movement_posture_visualization'=>10,
		              'ns_maintenance_generate_movement_posture_aligned_visualization'=>11,
			      'ns_maintenance_generate_sample_regions_from_mask'=>12,
			      'ns_maintenance_delete_images_from_database'=>13,
			      'ns_maintenance_check_for_file_errors'=>14,
			      'ns_maintenance_determine_disk_usage'=>15,
			      'ns_maintenance_generate_animal_storyboard'=>16,
			      'ns_maintenance_generate_animal_storyboard_subimage'=>17,
			      'ns_maintenance_compress_stored_images'=>18,
			      'ns_maintenance_generate_subregion_mask'=>19,
			      'ns_maintenance_rerun_image_registration'=>20,
			      'ns_maintenance_recalc_image_stats'=>21,
			      'ns_maintenance_recalc_worm_morphology_statistics'=>22  
			      );

$ns_denoising_option_labels = array(
		       "No normalization of worm movement scores"=>0,
		       "Normalize worm movement scores among all worms on a plate (Recommended)"=>1,
		       "Normalize worm movement scores to individual's median"=>2,
		       "Normalize worm movement scores to individual's 15% oldest median"=>3,
		       "Normalize worm movement scores among all worms on a scanner"=>4);

function ns_maintenance_task_order($is_region,$is_sample,$is_experiment){
  $r = array();
  global $ns_maintenance_tasks;
  if ($is_region){
    array_push($r,$ns_maintenance_tasks['ns_maintenance_rebuild_movement_data']);
    array_push($r,$ns_maintenance_tasks['ns_maintenance_generate_animal_storyboard']);
    array_push($r,$ns_maintenance_tasks['ns_maintenance_rebuild_movement_from_stored_image_quantification']);
    //array_push($r,$ns_maintenance_tasks['ns_maintenance_generate_movement_posture_visualization']);
    //array_push($r,$ns_maintenance_tasks['ns_maintenance_generate_movement_posture_aligned_visualization']);
    array_push($r,$ns_maintenance_tasks['ns_maintenance_determine_disk_usage']);
    array_push($r,$ns_maintenance_tasks['ns_maintenance_compress_stored_images']);
    array_push($r,$ns_maintenance_tasks['ns_maintenance_rerun_image_registration']);
    array_push($r,$ns_maintenance_tasks['ns_maintenance_check_for_file_errors']);
    array_push($r,$ns_maintenance_tasks['ns_maintenance_rebuild_movement_from_stored_images']);
    array_push($r,$ns_maintenance_tasks['ns_maintenance_generate_subregion_mask']);
    array_push($r,$ns_maintenance_tasks['ns_maintenance_recalc_image_stats']);
    array_push($r,$ns_maintenance_tasks['ns_maintenance_recalc_worm_morphology_statistics']);
    return $r;
  }
  if ($is_sample || $is_experiment){
       array_push($r,$ns_maintenance_tasks['ns_maintenance_generate_animal_storyboard']);
       array_push($r,$ns_maintenance_tasks['ns_maintenance_determine_disk_usage']);
       array_push($r,$ns_maintenance_tasks['ns_maintenance_compress_stored_images']);
       if (!$is_experiment)
       array_push($r,$ns_maintenance_tasks['ns_maintenance_check_for_file_errors']);
       return $r;
  }
  throw new ns_exception("No job type specified");
}

$ns_maintenance_task_labels = array(0=>'No Task',
				    1=>'Rebuild Sample Time Table',
				    2=>'Rebuild Worm Movement Table',
				    3=>'Update Processing Job Queue',
				    4=>'Delete Images',
				    5=>'Execute File Deletion',
				    6=>'Analyze Worm Movement',
				    7=>'Analyze Worm Movement Using Cached Images',
				    8=>'Re-calculate Censoring and Death Times',
				    9=>'Recalculate Censoring',
				    10=>'Generate Time-Aligned Posture Visualization',
				    11=>'Generate Death-Aligned Visualization',
				    12=>'Generate Sample Regions From the Sample Mask',
				    13=>'Delete Images from the Database',
				    14=>'Check for missing or broken files',
				    15=>'Determine disk usage for files',
				    16=>'Generate Animal Storyboard',
				    17=>'Generate Animal Storyboard Subimage',
				    18=>'Compress Stored Images',
				    19=>'Generate Subregion Mask',
				    20=>'Re-Run Image Registration',
				    21=>'Re-Calculate Image Statistics',
				    22=>'Compile Worm Morphology Statistics'
			      );
$NS_LAST_MAINTENANCE_TASK = 22;

$ns_maintenance_flags = array('ns_none'=>0,
			     'ns_only_delete_processed_capture_images'=>1,
			     'ns_only_delete_censored_images'=>2,
			      'ns_delete_entire_sample_region'=>3,
			      'ns_delete_everything_but_raw_data'=>4
			    );
$ns_maintenance_flag_labels = array(0=>'',
				    1=>'Delete only Processed Capture Images',
				    2=>'Delete Censored Images',
			     	    3=>'Delete Entire Sample Region Including Metadata',
				    4=>'Delete Everything Except for Raw Data'
				    );
$NS_LAST_MAINTENANCE_FLAG = 4;


$ns_operation_state_labels = array(	0=>'Idle',
			    		1=>'Capturing Image',
					2=>'SQL Read',
					3=>'SQL Write',
					4=>'Processing a Job',
					5=>'Reading from local disk',
					6=>'Writing to local disk',
					7=>'Reading from file server',
					8=>'Writing to file server',
					9=>'Allocating Image Memory',
					10=>'Deallocating Image Memory'
			);
$NS_LAST_OPERATION_STATE = 10;

$ns_video_timestamp_types = array( 0=>"No Timestamp",
				  1=>"Date Timestamp",
				  2=>"Age Timestamp"
				  );

class ns_processing_job{

	public $id,
	  $experiment_id,
	  $sample_id,
	  $region_id,
	  $image_id,
	  $urgent,
	  $pending_another_jobs_completion,
	  $paused,
	  $processor_id,
	  $mask_id,
	  $time_submitted,
	  $maintenance_task,
	  $job_name,
	  $operations,
	  $processed_by_push_scheduler,
	  $delete_file_job_id,
	  $video_timestamp_type;

	public $experiment_name,
	  $sample_name,
	  $region_name,
	  $image_filename,
	  $image_path,
	  $processor_name,
	  $currently_under_processing,
	  $problem;
	public $subregion_position_x,
	  $subregion_position_y,
	  $subregion_width,
	  $subregion_height,
	  $maintenance_flag;

	function ns_processing_job(){
		$this->operations = array();
		$this->currently_under_processing = 0;
		$this->problem = 0;
		$this->id = 0;
		$this->experiment_id = 0;
		$this->sample_id = 0;
		$this->region_id = 0;
		$this->image_id = 0;
		$this->urgent = 0;
		$this->pending_another_jobs_completion = 0;
		$this->paused = 0;
		$this->processor_id = 0;
		$this->time_submitted = 0;
		$this->mask_id = 0;
		$this->maintenance_task = 0;
		$this->job_name = '';
		//$this->processing_host_id = 0;
		$this->subregion_position_x = 0;
		$this->subregion_position_y = 0;
		$this->subregion_width = 0;
		$this->subregion_height = 0;
		$this->subregion_start_time = 0;
		$this->subregion_stop_time = 0;
		global $NS_LAST_PROCESSING_JOB;
		$this->operations = array_fill(0,$NS_LAST_PROCESSING_JOB+1,0);
		$this->processed_by_push_scheduler = FALSE;
		$this->delete_file_job_id = 0;
		$this->video_timestamp_type = 0;
		$this->maintenance_flag = 0;

	}
	function provide_query_stub(){
	  return 'SELECT processing_jobs.id, processing_jobs.experiment_id, processing_jobs.sample_id, processing_jobs.region_id, processing_jobs.image_id, ' //0-4
	    . 'processing_jobs.urgent, processing_jobs.paused,processing_jobs.processor_id, processing_jobs.time_submitted, processing_jobs.mask_id, processing_jobs.maintenance_task, processing_jobs.job_name, ' //5-11
		                . 'processing_jobs.problem, processing_jobs.currently_under_processing, processed_by_push_scheduler, '//12-14

	    .'subregion_position_x,subregion_position_y,subregion_width,subregion_height,subregion_start_time,subregion_stop_time,delete_file_job_id,video_add_timestamp,maintenance_flag,pending_another_jobs_completion,' //12-24
				. 'processing_jobs.op0, processing_jobs.op1, processing_jobs.op2, processing_jobs.op3, processing_jobs.op4, processing_jobs.op5, processing_jobs.op6, '
 				. 'processing_jobs.op7, processing_jobs.op8, processing_jobs.op9, processing_jobs.op10, processing_jobs.op11, processing_jobs.op12, processing_jobs.op13,'
		  . 'processing_jobs.op14, processing_jobs.op15, processing_jobs.op16, processing_jobs.op17, processing_jobs.op18, processing_jobs.op19, processing_jobs.op20, processing_jobs.op21, processing_jobs.op22, processing_jobs.op23, processing_jobs.op24, processing_jobs.op25,processing_jobs.op26, processing_jobs.op27, processing_jobs.op28 ';
	}
	function load_from_result(&$res){
	  global $NS_LAST_PROCESSING_JOB;
	//die("");
	  $this->id = (int)$res[0];
	  $this->experiment_id = (int)$res[1];
	  $this->sample_id = (int)$res[2];
	  $this->region_id = (int)$res[3];
	  $this->image_id = (int)$res[4];
	  $this->urgent = (int)$res[5];
	  $this->paused = $res[6]=='1';
	  $this->processor_id = (int)$res[13];
	  //	  die("D".$this->processor_id);
	  $this->time_submitted = (int)$res[8];
	  $this->mask_id = (int)$res[9];
	  $this->maintenance_task = (int)$res[10];
	  $this->job_name = $res[11];
	  $this->problem = (int)$res[12];
	  $this->currently_under_processing = (int)$res[13];
	  //	  $this->processing_host_id = (int)$res[14];
	  $this->processed_by_push_scheduler = $res[13] != '0';
	  $this->subregion_position_x = $res[15];
	  $this->subregion_position_y = $res[16];
	  $this->subregion_width = $res[17];
	  $this->subregion_height = $res[18];
	  $this->subregion_start_time = (int)$res[19];
	  $this->subregion_stop_time = (int)$res[20];
	  $this->delete_file_job_id = (int)$res[21];
	  $this->video_timestamp_type = (int)$res[22];
	  $this->maintenance_flag = (int)$res[23];
	  $this->pending_another_jobs_completion = (int)$res[24];
	  for ($i = 1; $i <= $NS_LAST_PROCESSING_JOB; $i++)
	    $this->operations[$i] = (int)$res[25+$i];
	}

	function save_to_db($sql){
		global $ddbug;
		if ($ddbug=='') $ddebug = 0;
		$ddbug++;
		//echo "SAVING JOB" . $debug . ": ";
	  global $NS_LAST_PROCESSING_JOB;
		if ($this->id == 0 || $this->id == '')
			$query = "INSERT INTO ";
		else $query = "UPDATE ";
		$query .= "processing_jobs SET experiment_id = ".(string)((int)$this->experiment_id).", sample_id = ".(string)((int)$this->sample_id).", region_id = ".(string)((int)$this->region_id).", "
		  . "image_id = ".(string)((int)$this->image_id).", urgent=".(string)((int)$this->urgent)."., paused=".(string)((int)$this->paused).", processor_id=".(string)((int)$this->processor_id).", time_submitted=".(string)((int)$this->time_submitted).", mask_id =".(string)((int)$this->mask_id).", maintenance_task=".(string)((int)$this->maintenance_task).","
		  . "subregion_position_x=".(string)((int)$this->subregion_position_x).",subregion_position_y=".(string)((int)$this->subregion_position_y).",subregion_width=".(string)((int)$this->subregion_width).",subregion_height=".(string)((int)$this->subregion_height) . ",subregion_start_time=".(string)((int)$this->subregion_start_time) .",subregion_stop_time=".(string)((int)$this->subregion_stop_time)
		  .",maintenance_flag=".(string)((int)$this->maintenance_flag)."";
		for ($i = 0; $i <= $NS_LAST_PROCESSING_JOB; $i++)
		  $query .= ", op$i=".(string)((int)$this->operations[$i]);
		//echo $query;
		$query .= ", job_name='{$this->job_name}'";
		$query .= ", processed_by_push_scheduler=";
		if ($this->processed_by_push_scheduler)
		  $query .= "'1'";
		else $query .= "'0'";
		$query .= ", delete_file_job_id = " . $this->delete_file_job_id;
	        if ($this->video_timestamp_type === "")
		  $this->video_timestamp_type = 0;
		$query .= ", video_add_timestamp = " . (int)$this->video_timestamp_type;
		//	die($query);
		//	echo $query . "<BR>";
		$query .= ", pending_another_jobs_completion= " . (int)$this->pending_another_jobs_completion;
		if ($this->id == 0 || $this->id == '')
			$this->id = $sql->send_query_get_id($query);
		else{
			$query .= " WHERE id= {$this->id}";
			//die($query);
			$sql->send_query($query);
		}
		//echo $this->id . "<BR>";
		//echo $query . "<BR>";
	}

	function load_from_db($id,$sql){
		$query = $this->provide_query_stub();
		$query .= "FROM processing_jobs WHERE id=$id";
		$sql->get_row($query,$res);
		if (sizeof($res) == 0)
			throw new ns_exception("ns_processing_job::Could not load job (id = " . $id . ") from db");
		$this->load_from_result($res[0]);
	}

	function get_names($sql){
		if ($this->experiment_id != 0){
			$query = "SELECT name FROM experiments WHERE id='{$this->experiment_id}'";
			$sql->get_value($query,$this->experiment_name);
		}
		if ($this->sample_id != 0){
			$query = "SELECT name FROM capture_samples WHERE id='{$this->sample_id}'";
			$sql->get_value($query,$this->sample_name);
		}
		if ($this->region_id != 0){
			$query = "SELECT name FROM sample_region_image_info WHERE id='{$this->region_id}'";
			$sql->get_value($query,$this->region_name);
		}
		if ($this->maintenance_task == 0 && $this->image_id != 0){
			$query = "SELECT filename, path FROM images WHERE id='{$this->image_id}'";
			$sql->get_single_row($query,$p);
			$this->image_filename = $p[0];
			$this->image_path = $p[1];
		}
		if ($this->processor_id != 0){
			$query = "SELECT name FROM hosts WHERE id='{$this->processor_id}'";
			$sql->get_row($query,$res);
			if (sizeof($res) <= 0){
			  $this->processor_name = "(Unknown)";
			}
			else $this->processor_name = $res[0][0];
		}
	}
	//returns an array containing the number of images calculated.
	//[0] is the total number of jobs pending and [i] is the number of jobs completed of the current task.
	function get_number_complete($sql, $only_samples=FALSE){
	  global $NS_LAST_PROCESSING_JOB;
	  global $ns_processing_tasks;
	  global $ns_path_image_task;
	  //to see how many opererations we have completed on a region, we find the number of processed region images
	  //and divide it by the number of unprocessed region images
	  if (!$only_samples){
	    $operations = array();
	    $query = "SELECT SUM(image_id>0)";
	    $query_suffix = " FROM sample_region_images WHERE region_info_id ='{$this->region_id}' AND censored = 0 ";
	  //  $sql->get_value($base_query,$operations[0]);


	    for ($i = 1; $i <= $NS_LAST_PROCESSING_JOB; $i++){
	      if ($i == $ns_processing_tasks['ns_process_heat_map'] ||
		  $i == $ns_processing_tasks['ns_process_static_mask'] ||
		  $i == $ns_processing_tasks['ns_process_compress_unprocessed'] ||
		  $i == $ns_processing_tasks['ns_process_analyze_mask']  ||
		  $i == $ns_processing_tasks['ns_process_compile_video'])
		continue;

	      $query .= ",SUM(op{$i}_image_id > 0)";
	    }
	    $query .=$query_suffix;
	    //die($query);
	    $sql->get_row($query,$op_counts);
	    $operations[0] = $op_counts[0][0];
	    $r = 1;
	    for ($i = 1; $i <= $NS_LAST_PROCESSING_JOB; $i++){
	      if ($i == $ns_processing_tasks['ns_process_heat_map'] ||
		  $i == $ns_processing_tasks['ns_process_static_mask'] ||
		  $i == $ns_processing_tasks['ns_process_compress_unprocessed'] ||
		  $i == $ns_processing_tasks['ns_process_analyze_mask']  ||
		  $i == $ns_processing_tasks['ns_process_compile_video'])
		continue;
		$operations[$i] = $op_counts[0][$r];
		if ($operations[$i] == -1) $operations[$i] = 0;
		$r++;
	    }
	  // var_dump($operations);
	  // die('');

	    $query = 'SELECT op' .  $ns_processing_tasks['ns_process_static_mask']
	      .'_image_id, op' . $ns_processing_tasks['ns_process_heat_map']
	      .'_image_id,path_movement_images_are_cached, latest_movement_rebuild_timestamp, last_timepoint_in_latest_movement_rebuild,time_path_solution_id  FROM sample_region_image_info WHERE id=' . $this->region_id ;
	    $sql->get_row($query,$res);
	    $operations[-5] = $res[0][3];
	    $operations[-6] = $res[0][4];
	    $operations[-7] = $res[0][5];

	    $query = 'SELECT count(*) FROM sample_region_images WHERE censored!=0 AND region_info_id = ' . $this->region_id;
	    $sql->get_value($query,$operations[-2]);
		$query = 'SELECT count(*) FROM sample_region_images WHERE problem!=0 AND region_info_id = ' . $this->region_id;
	    $sql->get_value($query,$operations[-3]);
		$query = 'SELECT count(*) FROM sample_region_images WHERE currently_under_processing!=0 AND region_info_id = ' . $this->region_id;
	    $sql->get_value($query,$operations[-4]);
	    //var_dump($res);

	    $operations[$ns_processing_tasks['ns_process_static_mask']] = 1*($res[0][0] != '0');
	    $operations[$ns_processing_tasks['ns_process_heat_map']] = 1*($res[0][1] != '0');
	    $operations[$ns_path_image_task] = 1*($res[0][2] != '0');

	    return $operations;
	  }
	  //to find the number of captured images masked, we take the number of masked captured images for the specified sample
	  //and divide it by the number of captured images.
	  else{


		$query = "SELECT COUNT(*),COUNT(DISTINCT image_id), COUNT(DISTINCT small_image_id), SUM(censored>0), SUM(problem>0), SUM(currently_being_processed>0),SUM(mask_applied) FROM captured_images WHERE sample_id = {$this->sample_id}";

		$sql->get_row($query,$r);
		for ($i = -7; $i < $NS_LAST_PROCESSING_JOB; $i++)
		    $operations[$i] = 0;
		$operations[-1] = $r[0][0];
		$operations[0] = $r[0][1];
		$operations[$ns_processing_tasks['ns_process_resized_sample_image']] = $r[0][2];

		$operations[-2] = $r[0][3];
		$operations[-3] = $r[0][4];
		$operations[-4] = $r[0][5];

		$operations[1] = $r[0][6];


	    return $operations;
      	  }
	}


	function get_processing_state_description($only_sample_info,$sql){
	  global $ns_processing_tasks,
	  	  $ns_path_image_task,
	    $ns_maintenance_task_labels;
	  $complete = $this->get_number_complete($sql,$only_sample_info);
	  if ($complete[0] == 0)
	    $complete[0] = 1;

	  $movement_analysis_rebuild_timestamp = $complete[-5];
	  $movement_analyisis_last_timepoint = $complete[-6];
	  $time_path_solution_exists = (int)$complete[-7] > 0;
	  // var_dump($complete);
	  global $ns_processing_task_labels;

	  $display = array();
	  $number_of_operations = 0;
	  if ($only_sample_info){
	    $display[0] = $ns_processing_tasks['ns_unprocessed'];
	    $display[2] = $ns_processing_tasks['ns_process_apply_mask'];
	    $display[1] = $ns_processing_tasks['ns_process_resized_sample_image'];
	  }
	  else{
	    $display[0] = $ns_processing_tasks['ns_process_resized_sample_image'];
	    $display[1]=$ns_processing_tasks['ns_process_spatial'];
	    $display[2]=$ns_processing_tasks['ns_process_lossy_stretch'];
	    $display[3]=$ns_processing_tasks['ns_process_threshold'];
	    $display[4]=$ns_processing_tasks['ns_process_worm_detection'];
	    $display[5]=$ns_processing_tasks['ns_process_worm_detection_labels'];

	    $display[6]=$ns_processing_tasks['ns_process_add_to_training_set'];
	    $display[7]=$ns_processing_tasks['ns_process_accept_vis'];
	    $display[8]=$ns_processing_tasks['ns_process_reject_vis'];
	    $display[9]=$ns_processing_tasks['ns_process_movement_coloring'];
	    $display[10]=$ns_processing_tasks['ns_process_movement_mapping'];
	    $display[11]=$ns_processing_tasks['ns_process_posture_vis'];
	    $display[12]=$ns_processing_tasks['ns_process_heat_map'];
	    $display[13]=$ns_processing_tasks['ns_process_static_mask'];
	    $display[14]=$ns_processing_tasks['ns_process_worm_detection_with_graph'];
	    $display[15]=$ns_processing_tasks['ns_process_movement_coloring_with_graph'];
	    $display[16]=$ns_processing_tasks['ns_process_movement_paths_visualization'];
	    $display[17]=$ns_processing_tasks['ns_process_movement_paths_visualization_with_mortality_overlay'];
	    $display[18]=$ns_processing_tasks['ns_process_movement_posture_visualization'];
	    $display[18]=$ns_processing_tasks['ns_process_movement_posture_aligned_visualization'];

	  }

	  $complete[$ns_processing_tasks["ns_process_movement_coloring"]]*=2;
	  $complete[$ns_processing_tasks["ns_process_movement_mapping"]]*=2;
	  $complete[$ns_processing_tasks["ns_process_posture_vis"]]*=2;
	  $complete[$ns_processing_tasks["ns_process_movement_coloring_with_graph"]]*=2;
	  $complete[$ns_processing_tasks["ns_process_movement_coloring_with_survival"]]*=2;

	  //static mask and heat map are all or nothing.
	  $complete[$ns_processing_tasks["ns_process_static_mask"]]*=$complete[0];

	  $complete[$ns_processing_tasks["ns_process_heat_map"]]*=$complete[0];

	  $number_of_operations = 0;
	  $vals = array();
	  $denom = $complete[0];
	  if ($only_sample_info)
		$denom = $complete[-1];
	  for ($i = 0; $i < sizeof($display); $i++){
	    if ($denom == 0)
		$vals[$i] = 0;
	    else
	    $vals[$i] = floor(10000*$complete[$display[$i]]/$denom)/100 . "%";
	    if ($vals[$i] != 0)
	      $number_of_operations++;
	  }
	  $number_of_operations+=3;//censored, problem and busy information is last items in list
	  $number_of_operations++;//movement path calculation
	  $number_of_columns = 1;
	  if ($number_of_operations > 4)
	    $number_of_columns++;
	  $res = "\n<table cellspacing = 0 cellpadding = 0><tr><td valign=\"top\">\n";
	  $res.= "<table valign=\"top\" cellspacing =0 cellpadding=0>\n";

	  $j = 0;
	  $a = floor($number_of_operations/2)+ $number_of_operations%2;
	  $images_present = FALSE;
	  // $res .= "XXX" . sizeof($display);
	  for ($i = 0; $i < sizeof($display); $i++){

	     if ($vals[$i] == 0)
	       continue;
	     $images_present = TRUE;
	    if ($number_of_columns > 1 && $j == $a)
	      $res .="</table>\n</td><td>&nbsp;</td><td valign=\"top\">\n<table cellspacing=0 cellpadding=0 valign=\"top\">";

	    $res .= "<tr><td><font face=\"Arial\" size=\"-2\">".  $ns_processing_task_labels[$display[$i]] . "&nbsp;</font></td><td>";


	    $res .=  "<font size=\"-2\">" . $vals[$i]. /*"%(" . $complete[$display[$i]] . "/" . $complete[0] . ")*/"</font></td></tr>\n";
	    $j++;
	  }
	  $count_column=0;
	  if ($complete[$count_column] > 0){
	    if (!$only_sample_info){
	      $res .= "<tr><td valign=\"top\"><font face=\"Arial\" size=\"-2\">Movement Last Calculated &nbsp;</font></td><td>";
	      $res .=  "<font size=\"-2\">";
	      if ($complete[$ns_path_image_task] != 0){

		$res .= format_time($movement_analysis_rebuild_timestamp);
		$res .= "<br>Up to time " . format_time($movement_analyisis_last_timepoint);
	      }
	      else $res.= "Never";
	      $res .= "</font>";
	      if (!$time_path_solution_exists){
		$res.="<BR><font size=\"-2\" color=\"#FF0000\">(time path solution appears lost) </font>";
	      }
	      $res.="</td></tr>\n";

	      $count_column = 0;
	    }
	    else $count_column = -1;

		$res .= "<tr><td><font face=\"Arial\" size=\"-2\"> <i>Censored </i>&nbsp; </font></td><td>";
		$res .=  "<font size=\"-2\">" . floor(10000*$complete[-2]/($complete[-2]+$complete[$count_column])/100). "%</font></td></tr>\n";
		$res .= "<tr><td><font face=\"Arial\" size=\"-2\"> <i>Problem </i>&nbsp; </font></td><td>";
		$res .=  "<font size=\"-2\">" . floor(10000*$complete[-3]/($complete[-3]+$complete[$count_column])/100). "%</font></td></tr>\n";  $res .= "<tr><td><font face=\"Arial\" size=\"-2\"> <i>Busy </i>&nbsp; </font></td><td>";
		$res .=  "<font size=\"-2\">" . floor(10000*$complete[-4]/($complete[-4]+$complete[$count_column])/100). "%</font></td></tr>\n";
	  }

	  $res .= "</table><td></tr></table>\n";
	  if (!$images_present)
	    return '';
	  return $res;
	}


	function get_job_description($sql,$show_subject_data=FALSE){
	  global $ns_processing_tasks,
	    $ns_maintenance_task_labels;

	  global $ns_processing_task_labels,
		$ns_maintenance_flag_labels;

	  $number_of_operations = 0;
	  for ($i = 0; $i < sizeof($this->operations);$i++)
	    if ($this->operations[$i] != 0 && $i != $ns_processing_tasks['ns_process_compile_video'])
	      $number_of_operations++;

	  $number_of_columns = 1;
	  if ($number_of_operations > 4)
	    $number_of_columns++;
	global $table_colors,$table_header_color;
//$table_colors = array(array("#EEEEEE","#B9CBDF"), array("#FFFFFF","#DBEFFF"));
//$table_header_color = " bgcolor=\"#BBBBBB\"";
	 $res = "\n<table><tr><td valign=\"top\" bgcolor=\"#CCCCCC\" cellspacing=0 cellpadding=1>\n";
	  $res.= "<table valign=\"top\" bgcolor=\"{$table_colors[1][0]}\" cellspacing=0 cellpadding=3><tr><td colspan=$number_of_columns bgcolor=\"{$table_colors[0][0]}\">\n";
	//Processing job header table
	$res.= "<table cellspacing=0 cellpadding=0 width=\"100%\"><tr><td>";
	$res.= "<a href=\"view_processing_job.php?job_id={$this->id}\"><font size=\"-1\">[Image Processing Job";
	//$res.=  format_time($this->time_submitted);
	$res.=  "]</font></a>";
	$res .= "</td><td><div align=\"right\">";
	  if ($this->problem ){
	    $res .= '<font size="-1"><a href="view_hosts_log.php?event_id='.$this->problem.'">(Problem)</a></font>';
	}
	  // $res .= $this->urgent;
	 if ($this->urgent==1){
	 $res .= '<font size="-1">(Urgent)</font>';
	}
	 else if ($this->urgent<0){
	   $res .= '<font size="-1">(Low Priority)</font>';
	 }
	 if ($this->paused){
	   $res .='<font size="-1">(Paused)</font>';
	 }
	 if ($this->pending_another_jobs_completion==1)
	   $res .='<font size="-1">(When Ready)</font>';

	  if ($this->currently_under_processing)
	    $res .= '<a href="view_hosts_log.php?host_id='.$this->processor_id.'&limit=50"><font size="-1">(Processing...)</font></a>';
	$res .="</div></td></tr></table>\n";
	//End processer job header table
	$res.="</td></tr>\n";
	  if ($this->operations[$ns_processing_tasks["ns_process_compile_video"]] != 0){
	    $vid = 1;
	    $res .= "<tr><td colspan=\"$number_of_columns\">\n<font size=\"-1\">Compile Video";
		if ($this->subregion_position_x != 0 || $this->subregion_position_y != 0 || $this->subregion_width != 0 || $this->subregion_height != 0){
				echo "<br>Position: ({$this->subregion_position_x},{$this->subregion_position_y})<br>Size: (" . $this->subregion_width . "," .$this->subregion_height . ")";
			}
	   $res.="</font>\n</td></tr>\n";
	  }
	if ($show_subject_data){
	    $res .= "<tr><td colspan=\"$number_of_columns\">\n<font size=\"-1\">Subject: ";
		$res .= $this->experiment_name . "::" . $this->sample_name;
		if ($this->region_id != 0)
			$res .= "::" . $this->region_name;
	   $res.="</font>\n</td></tr>\n";
	  }
	  if ($this->maintenance_task != 0){
		$res.='<tr><td colspan=\"$number_of_columns\"><font size="-1">Maintenance task: ';
		if (!$show_subject_data) $res.="<BR>";
		$res .= $ns_maintenance_task_labels[$this->maintenance_task];
		if($this->maintenance_flag != 0)
			$res.=" (".$ns_maintenance_flag_labels[$this->maintenance_flag] . ")";
		$res.= "</font></td></tr>\n";
	  }
	  $order = 1;
	  $a = floor($number_of_operations/2)+1;
	  if(sizeof($this->operations > 0)){
		$res.="<tr><td>\n<font size=\"-1\">";
		//$res.=var_dump($this->operations);
		for ($i = 0; $i < sizeof($this->operations); $i++){
			if ($this->operations[(int)$i] == 0 ||
				$i == $ns_processing_tasks["ns_process_compile_video"]){
				continue;
				}

			if ($number_of_columns > 1 && $order == $a)
				$res .="</font>\n</td><td>\n<font size=\"-1\"> ";

			$res .= $order . ". " .  $ns_processing_task_labels[$i] . "<br>";
			$order++;

		}
		$res .= "</font>\n</td></tr>\n";
	}
	  $res.= "</table></td></tr>\n</table>\n\n";
	  return $res;
	}

	function get_concise_description(){
	  global $ns_processing_tasks, $ns_maintenance_task_labels, $number_of_operations,$ns_maintenance_flag_labels,$ns_processing_task_labels;
	  $res = "";
	  if ($this->operations[$ns_processing_tasks["ns_process_compile_video"]] != 0){
		    $vid = 1;
		    $res .= "Compile Video";
	  }

	  if ($this->maintenance_task != 0){

		$res .= $ns_maintenance_task_labels[$this->maintenance_task];
	
		if ($ns_maintenance_flag_labels[$this->maintenance_flag]!='')
			$res.=" (".$ns_maintenance_flag_labels[$this->maintenance_flag] . ")";
	  }

	  if(sizeof($this->operations > 0)){
	  $first = true;
		for ($i = 0; $i < sizeof($this->operations); $i++){
			if ($this->operations[(int)$i] == 0 ||
				$i == $ns_processing_tasks["ns_process_compile_video"]){
					continue;
			}
			$l = $ns_processing_task_labels[$i];
			if ($l != ''){
			   if (!$first){
			      $res.=", ";
			      
			      }
			      else $first = false;
			      $res .= $ns_processing_task_labels[$i];
			      }
		}
	
		}
	return $res;
	}
}

function ns_update_job_queue($sql){
  global $ns_maintenance_tasks;
  $job = new ns_processing_job;
  $job->processed_by_push_scheduler = TRUE;
  $job->maintenance_task = $ns_maintenance_tasks["ns_maintenance_update_processing_job_queue"];
  $job->time_submitted = ns_current_time();
  $job->save_to_db($sql);
  $query = "SET AUTOCOMMIT=0";
  $sql->send_query($query);
  $query = "BEGIN";
  $sql->send_query($query);
  $query = "LOCK TABLES processing_job_queue WRITE";
  $sql->send_query($query);
  $query = "INSERT INTO processing_job_queue SET job_id=" . $job->id . ", priority=50,job_name=''";
  $sql->send_query($query);
  $query = "COMMIT";
  $sql->send_query($query);
  $query = "UNLOCK TABLES";
  $sql->send_query($query);
}

function ns_delete_images_from_database($experiment_id,$sample_id,$region_id,$image_id,$sql){
  global $ns_maintenance_tasks;
  global $ns_maintenance_flags;
  $job = new ns_processing_job;
  $job->maintenance_task = $ns_maintenance_tasks['ns_maintenance_delete_images_from_database'];
  $job->experiment_id = $experiment_id;
  $job->sample_id = $sample_id;
  $job->region_id = $region_id;
  $job->image_id = $image_id;
  $job->urgent = 2;
  $job->time_submitted = ns_current_time();
 if ($experiment_id == 0 && $sample_id == 0 && $region_id!=0 && $image_id == 0){
//	die("WHOOP!");
	$job->maintenance_flag = $ns_maintenance_flags['ns_delete_entire_sample_region'];
  }
  $job->save_to_db($sql);

  ns_update_job_queue($sql);
}
function ns_delete_image_from_database($image_id,$sql){
  ns_delete_images_from_database(0,0,0,$image_id,$sql);
}
function ns_delete_region_from_database($region_id,$sql){
  ns_delete_images_from_database(0,0,$region_id,0,$sql);
}
function ns_delete_sample_from_database($sample_id,$sql){
  ns_delete_images_from_database(0,$sample_id,0,0,$sql);
}
function ns_delete_experiment_from_database($experiment_id,$sql){
  ns_delete_images_from_database($experiment_id,0,0,0,$sql);
}
?>