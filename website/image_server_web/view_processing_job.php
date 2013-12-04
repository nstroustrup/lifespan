<?php
require_once('worm_environment.php');
require_once('ns_dir.php');	
//require_once('ns_external_interface.php');
require_once('ns_experiment.php');
require_once('ns_processing_job.php');

$lock_jobs_query = "LOCK TABLES processing_jobs write";
$lock_job_queue_query = "LOCK TABLES processing_job_queue WRITE";
$unlock_tables_query = "UNLOCK TABLES";

$set_autocommit_query = "SET AUTOCOMMIT = 0";
$clear_autocommit_query = "SET AUTOCOMMIT = 1";

$region_processing_jobs_to_display = array($ns_processing_tasks['ns_process_spatial'],
				$ns_processing_tasks['ns_process_lossy_stretch'],
$ns_processing_tasks['ns_process_threshold'],
$ns_processing_tasks['ns_process_worm_detection'],
$ns_processing_tasks['ns_process_worm_detection_labels'],
					   $ns_processing_tasks['ns_process_add_to_training_set'],
$ns_processing_tasks['ns_process_movement_paths_visualization'],
				$ns_processing_tasks['ns_process_movement_paths_visualization_with_mortality_overlay']);
try{
 

  $jobs = array();		
  $job_id = @$query_string['job_id'];
  $hide_entire_region_job = @$query_string['hide_entire_region_job'];
  $specified_experiment_id = @(int)$query_string['experiment_id'];
  $specified_sample_id = @(int)$query_string['sample_id'];
  $specified_region_id = @(int)$query_string['region_id'];
  $specified_image_id =  @(int)$query_string['image_id'];

  $strain_specified= isset($query_string['strain']);
  $strain_specification = @$query_string['strain'];

  $condition_1_specified = isset($query_string['condition_1']);
  $condition_2_specified = isset($query_string['condition_2']);
  $condition_3_specified = isset($query_string['condition_3']);
  $device_specified = isset($query_string['device']);
  
  $culturing_temperature_specified = isset($query_string['culturing_temperature']);
  $experiment_temperature_specified = isset($query_string['experiment_temperature']);
  $food_source_specified = isset($query_string['food_source']);
  $environment_conditions_specified = isset($query_string['environment_conditions']);

  $condition_1 = @$query_string['condition_1'];
  $condition_2 = @$query_string['condition_2'];
  $condition_3 = @$query_string['condition_3'];
  $device = @$query_string['device'];
  $culturing_temperature = @$query_string['culturing_temperature'];
  $experiment_temperature = @$query_string['experiment_temperature'];
  $food_source = @$query_string['food_source'];
  $environment_conditions = @$query_string['environment_conditions'];
  //die("$environment_conditions");

  $specified_all_experiments = @$query_string['experiment_id'] === "all";
  $specified_hidden_experiments = @$query_string['hidden']=="1";
  $specified_experiment_list = isset($query_string['experiment_list']);
  $specified_all_samples = @$query_string['sample_id']=== "all";
  $specified_all_regions = @$query_string['region_id']=== "all";

  $all_new = @$query_string['all_new'] == '1';
  $include_censored = @(int)$query_string['include_censored'];
  $live_dangerously = @$query_string['live_dangerously']==="1";

 $experiment_list = @$query_string['experiment_list'];

  $clear_region = @$_POST["clear_region"];
  $delete_sample_images = @$_POST["delete_sample_images"];
  $job_specified = false;
  if ($job_id != 0){
    $job_specified = true;
	$specified_job = new ns_processing_job();
	$query = $specified_job->provide_query_stub();
	$query .= " FROM processing_jobs WHERE processing_jobs.id = " . $job_id;
	$sql->get_row($query,$result);
	if (sizeof($result) == 0)
		throw new ns_exception("Could not load job id " . $job_id . " from database");
	$specified_job->load_from_result($result[0]);
	$experiment_id = (int)$specified_job->experiment_id;
	$sample_id = $specified_job->sample_id;
	$region_id = $specified_job->region_id;
	$image_id = $specified_job->image_id;
	$all_samples = FALSE;
	$all_regions = FALSE;
  }
else{
	$experiment_id = $specified_experiment_id;
	$sample_id = $specified_sample_id;
	$region_id = $specified_region_id;
	$image_id = $specified_image_id;
}
$IS_EXPERIMENT = 0; 
$IS_SAMPLE = 1;
$IS_REGION = 2;
 
  if (  ($experiment_id != 0  || $specified_all_experiments ||
	 $specified_experiment_list)
	&& ($sample_id === 0 && !$specified_all_samples    ) 
	&& ($region_id === 0  && !$specified_all_regions    ))
	$job_type = $IS_EXPERIMENT;  
else if (  ($sample_id !== 0  || $specified_all_samples    ) 
	&& ($region_id === 0  && !$specified_all_regions    ))
	$job_type = $IS_SAMPLE;
else if ($region_id != 0  || $specified_all_regions)
	$job_type = $IS_REGION;

$experiments_to_process = array();
$experiment_name_hash = array();

 $request_type = "";

if ($specified_experiment_list){
	$experiments_to_process = explode(";",$experiment_list);
	if (sizeof($experiments_to_process)==0)
		throw new ns_exception("Empty experiment link");

	$query = "SELECT id,name FROM experiments WHERE id = " . $experiments_to_process[0];
	for ($i = 1; $i < sizeof($experiments_to_process); $i++)
		$query .= " || id = " . $experiments_to_process[$i];
	//die($query);
	$sql->get_row($query,$enames);

	for ($i = 0; $i < sizeof($enames); $i++){
		$experiment_name_hash[$enames[$i][0]] = $enames[$i][1];
	}

}
 else if ($experiment_id != 0 || $specified_all_experiments){

  if ($specified_all_experiments){
     $query = "SELECT id,name FROM experiments";
     if (!$specified_hidden_experiments)
       $query .= " WHERE hidden = 0";
	$sql->get_row($query,$enames);
	for ($i = 0; $i < sizeof($enames); $i++){
		array_push($experiments_to_process,$enames[$i][0]);
		$experiment_name_hash[$enames[$i][0]] = $enames[$i][1];
	}
   }
   else{
	array_push($experiments_to_process,$experiment_id);
	$query = "SELECT id,name FROM experiments WHERE id = $experiment_id";
	$sql->get_row($query,$enames);
	for ($i = 0; $i < sizeof($enames); $i++){
		$experiment_name_hash[$enames[$i][0]] = $enames[$i][1];
	}
  }  
}
//var_dump($experiments_to_process);
 if (sizeof($experiments_to_process) > 0){
   $append_experiment_name_to_sample = 0;//sizeof($experiments_to_process) > 1;
  
   $request_type = "experiment";


    for ($i = 0; $i < sizeof($experiments_to_process); $i++){
	$query = "SELECT id, name FROM capture_samples WHERE experiment_id = " . $experiments_to_process[$i];
	
	$sql->get_row($query,$snames);
	if ($append_experiment_name_to_sample){
		for ($j = 0; $j < sizeof($snames); $j++){
			$experiment_id_hash[$snames[$j][0]] = $experiments_to_process[$i];
			$sample_name_hash[$snames[$j][0]] = $experiment_name_hash[$experiments_to_process[$i]] . "::" . $snames[$j][1];
		}
	}
	else{
		for ($j = 0; $j < sizeof($snames); $j++){
			$sample_name_hash[$snames[$j][0]] = $snames[$j][1];
			$experiment_id_hash[$snames[$j][0]] = $experiments_to_process[$i];
		}
	}
   }
 /*  $query = "SELECT r.id, s.name,r.name FROM sample_region_image_info as r, capture_samples as s WHERE r.sample_id = s.id AND s.experiment_id = $experiment_id";
   $sql->get_row($query,$rnames);
   for ($i = 0; $i < sizeof($rnames); $i++)
      $region_name_hash[$rnames[$i][0]] = $rnames[$i][1] . "::" . $rnames[$i][2];*/
    
  }


  //var_dump($_POST);
  //die($clear_region);
  $samples_to_process = array();
  //if we're creating a job to run on all regions of an experiment
  if ($specified_all_samples){
    if (sizeof($experiments_to_process) == 0)
      throw new ns_exception("No experiment specified!");
    for ($i = 0; $i < sizeof($experiments_to_process); $i++){
    	//echo $i;
	if ($include_censored==0)  $censored_string = "censored=0 AND ";
	if (!$specified_all_regions && $include_censored == 2) $censored_string="censored=1 AND ";
	$query = "SELECT id FROM capture_samples WHERE $censored_string experiment_id = " . $experiments_to_process[$i];
	//	  die($query);
	$sql->get_row($query,$res);
	for ($j = 0; $j < sizeof($res); $j++)
		array_push($samples_to_process,$res[$j][0]);
	//	var_dump($samples_to_process);die("");
	}
  }
  else if ($specified_sample_id != 0){
   	 array_push($samples_to_process,$specified_sample_id);
}
$region_end_times = array();
$region_start_times = array();
$region_strain = array();
$region_strain_condition_1 = array();
$region_strain_condition_2 = array();
 $region_strain_condition_3 = array();
 $region_culturing_temperature = array();
 $region_experiment_temperature = array();
 $region_food_source = array();
 $region_environment_conditions = array();

 if ($specified_all_regions){
   
   $request_type = "region";

   $k = 0;
   for ($j = 0; $j < sizeof($samples_to_process); $j++){
     $cur_sample = $samples_to_process[$j];
    
     if ($include_censored==0 || $include_censored==='') $censored_string = 'ri.censored=0 AND';
     
     if ($include_censored==2) $censored_string = 'ri.censored=1 AND';
     $conditions_string = '';
     //  die ($censored_string);
     if ($strain_specified) 
       $conditions_string .= "STRCMP(LOWER(ri.strain),LOWER('".mysql_real_escape_string($strain_specification)."'))=0 AND ";
     if ($condition_1_specified)
       $conditions_string .= "STRCMP(LOWER(ri.strain_condition_1),LOWER('".mysql_real_escape_string($condition_1)."'))=0 AND ";
     
      if ($condition_2_specified)
	$conditions_string.= "STRCMP(LOWER(ri.strain_condition_2),LOWER('".mysql_real_escape_string($condition_2)."'))=0 AND ";
     ;
      if ($condition_3_specified)
	$conditions_string .= "STRCMP(LOWER(ri.strain_condition_3),LOWER('".mysql_real_escape_string($condition_3)."'))=0 AND ";

      if ($device_specified)
	$conditions_string .= "STRCMP(LOWER(s.device_name),LOWER('".mysql_real_escape_string($device)."'))=0 AND ";
   
      if ($culturing_temperature_specified)
	$conditions_string .= "STRCMP(LOWER(ri.culturing_temperature),LOWER('".mysql_real_escape_string($culturing_temperature)."'))=0 AND ";
     
      if ($experiment_temperature_specified)
	$conditions_string .= "STRCMP(LOWER(ri.experiment_temperature),LOWER('".mysql_real_escape_string($experiment_temperature)."'))=0 AND ";
     
      if ($food_source_specified)
	$conditions_string .= "STRCMP(LOWER(ri.food_source),LOWER('".mysql_real_escape_string($food_source)."'))=0 AND ";

      if ($environment_conditions_specified)
	$conditions_string .= "STRCMP(LOWER(ri.environmental_conditions),LOWER('".mysql_real_escape_string($environment_conditions)."'))=0 AND ";
    

      $query = "SELECT ri.id, s.id,s.experiment_id, ri.name, ri.time_at_which_animals_had_zero_age,ri.time_of_last_valid_sample, ri.strain, ri.strain_condition_1,ri.strain_condition_2, ri.strain_condition_3, ri.culturing_temperature,ri.experiment_temperature,ri.food_source,ri.environmental_conditions FROM sample_region_image_info as ri, capture_samples as s WHERE $censored_string $strain_string $conditions_string ri.sample_id=" . $cur_sample . " AND ri.sample_id = s.id";
      // die($query);
      $sql->get_row($query,$region_ids);
      //   var_dump($region_ids);die();
//	echo $query . "<BR>"; var_dump($region_ids); echo $query;
      if (sizeof($region_ids) == 0)
	continue;
      //throw new ns_exception("This sample ($cur_sample) has no regions; region-jobs cannot be scheduled.");
      for ($i = 0; $i < sizeof($region_ids); $i++){

	$region_start_times[$region_ids[$i][0]] = $region_ids[$i][4];
	$region_end_times[$region_ids[$i][0]] = $region_ids[$i][5];
	$region_strain[$region_ids[$i][0]] = $region_ids[$i][6];
	$region_strain_condition_1[$region_ids[$i][0]] = $region_ids[$i][7];
	$region_strain_condition_2[$region_ids[$i][0]] = $region_ids[$i][8];
	$region_strain_condition_3[$region_ids[$i][0]] = $region_ids[$i][9];
	$region_culturing_temperature[$region_ids[$i][0]] = $region_ids[$i][10];
	$region_experiment_temperature[$region_ids[$i][0]] = $region_ids[$i][11];
	$region_food_source[$region_ids[$i][0]] = $region_ids[$i][12];
	$region_environment_conditions[$region_ids[$i][0]] = $region_ids[$i][13];

	$region_name_hash[$region_ids[$i][0]] = $region_ids[$i][3];
	if ($all_new){
	  $jobs[$k] = new ns_processing_job;
	  $jobs[$k]->region_id=$region_ids[$i][0];
	  $jobs[$k]->sample_id = $region_ids[$i][1];
	  // die($region_ids[$i][1]);
	  $jobs[$k]->experiment_id = $region_ids[$i][2];
	}
	else{
	  //check to see if any jobs are already assigned to the specified region
	  $query = "SELECT id FROM processing_jobs WHERE region_id = " . $region_ids[$i][0];
	  $sql->get_row($query,$job_ids);

	  // echo $query;
	  // var_dump($job_id);
	  // echo "<BR>";
	  //make new jobs if necissary
	  if (sizeof($job_ids) == 0){
	    $jobs[$k] = new ns_processing_job;
	    $jobs[$k]->region_id = $region_ids[$i][0];
	  $jobs[$k]->sample_id = $region_ids[$i][1];
	  $jobs[$k]->experiment_id = $region_ids[$i][2];
	    $jobs[$k]->id = 0; 
	  }
	  else{
	    //attach to existing jobs
	    for ($m = 0; $m < sizeof($job_ids); $m++){
	      $jobs[$k] = new ns_processing_job;
	      $jobs[$k]->region_id = $region_ids[$i][0];
		$jobs[$k]->sample_id = $region_ids[$i][1];
		$jobs[$k]->experiment_id = $region_ids[$i][2];
	      $jobs[$k]->id=$job_ids[$m][0];
	      $jobs[$k]->load_from_db($jobs[$k]->id,$sql);
	      $jobs[$k]->get_names($sql);
	      $k++;
	    }
	    $k--;
	  }
	}
	
	$jobs[$k]->maintenance_task = $_POST['maintenance_task'];
	$jobs[$k]->maintenance_flag = $_POST['maintenance_flag'];
	//echo $jobs[$i]->region_id . ": " . $jobs[$i]->sample_id . ':' . $jobs[$i]->experiment_id . "<BR>";
	//	$jobs[$k]->image_id = 0;
	$k++;
      }
    }
    // die("");
  }
  $k=0;
  //if we're creating a sample job to run on specified samples
  if (sizeof($samples_to_process) > 0 && !$specified_all_regions){
    
    $request_type = "sample";
    for ($i = 0; $i < sizeof($samples_to_process); $i++){

	if (sizeof($experiments_to_process) == 0){
		$query = "SELCT name FROM experiments WHERE sample_id = " . $samples_to_process[$i];
		$sql->get_row($query,$res);
		if (sizeof($res) == 0)
			throw new ns_exception("Could not load sample from db");
		$experiment_name_hash[$samples_to_process[$i]] = $res[0][0];
	}
      $query = "SELECT id FROM processing_jobs WHERE sample_id = " . $samples_to_process[$i] . " AND region_id = 0";
      $sql->get_row($query,$extant_jobs);
      if (sizeof($extant_jobs) == 0 || $all_new){
	$jobs[$k] = new ns_processing_job;
	$jobs[$k]->experiment_id = $experiment_id_hash[$samples_to_process[$i]];
	$jobs[$k]->sample_id = $samples_to_process[$i];
	$jobs[$k]->id = 0;
	$jobs[$k]->get_names($sql);
      }
      else{
	for ($m = 0; $m < sizeof($extant_jobs); $m++){
	  $jobs[$k] = new ns_processing_job;
	  $jobs[$k]->experiment_id = $experiment_id_hash[$samples_to_process[$i]];
	  $jobs[$k]->sample_id = $samples_to_process[$i];
	  $jobs[$k]->id = $extant_jobs[$m][0];
	  $jobs[$k]->load_from_db($jobs[$k]->id,$sql);
	  $jobs[$k]->get_names($sql);
	  $k++;
	}
	$k--;
      }
      $jobs[$k]->maintenance_task = $_POST['maintenance_task'];
      $jobs[$k]->maintenance_flag = $_POST['maintenance_flag'];
      $jobs[$k]->region_id = 0;
      $jobs[$k]->image_id = 0;
      $k++;
    }
  }
//echo "JOB SIZE = " . sizeof($jobs);
  //look for jobs that are attached only to a single region
  if (!$specified_all_regions && $region_id > 0 && !$job_specified){
    
    $request_type = "region";
    
    if (!$all_new){
      $query = "SELECT id FROM processing_jobs WHERE region_id = " . $region_id;
      $sql->get_row($query,$extant_jobs);
    }
    
    $query = "SELECT cs.id, cs.name, r.name, r.time_at_which_animals_had_zero_age,r.time_of_last_valid_sample FROM capture_samples as cs,sample_region_image_info as r WHERE r.id=$region_id AND cs.id = r.sample_id";
    $sql->get_row($query,$smp_name);
    if (sizeof($smp_name) == 0)
      die("No sample name found for region");
    
    $region_start_times[$region_id] = $smp_name[0][3];
    $region_end_times[$region_id] = $smp_name[0][4];
    $region_name_hash[$region_id] = $smp_name[0][2];
    // echo $query . "::" . sizeof($extant_jobs);
    //die($extant_jobs);
    
    if (sizeof($extant_jobs) != 0){
      
      for ($i = 0; $i < sizeof($extant_jobs); $i++){
	$jobs[$i] = new ns_processing_job;
	$jobs[$i]->id = $extant_jobs[$i][0];
	// die($jobs[$i]->id);
	$jobs[$i]->load_from_db($jobs[$i]->id,$sql);
	$jobs[$i]->get_names($sql);
	
	//$jobs[$i]->sample_id = $smp_name[0][0];
	//$jobs[$i]->region_id = $region_id;
	//	var_dump($smp_name);
	//die("");
	//$sample_name_hash[$smp_name[0][0]] = $smp_name[0][1];
	//$jobs[$i]->sample_name= $smp_name[0][1];
	//die(	$jobs[$i]->sample_name);
	// die($jobs[$i]->sample_name());
      }
    }
  }
  //var_dump($experiment_list_specified);
  //die("");
  if (($specified_all_experiments || $specified_experiment_list ) && sizeof($samples_to_process) == 0){
    // die("SDF");
    $request_type = "experiment";
    for ($i = 0; $i < sizeof($experiments_to_process); $i++){
      $jobs[$i] = new ns_processing_job;
      $jobs[$i]->id=0;
      $jobs[$i]->experiment_id=$experiments_to_process[$i];
      $jobs[$i]->sample_id=0;
      $jobs[$i]->region_id=0;
      $jobs[$i]->image_id=0;
      $jobs[$i]->maintenance_task = $_POST['maintenance_task'];
      $jobs[$i]->maintenance_flag = $_POST['maintenance_flag'];
    }
    
  }
  
  //if we're creating a job to run on a single region
  //on a single sample, or a single experiment
  if (sizeof($samples_to_process) == 0){
    //die("WOOR");
    //  if ($region_id == '' && $sample_id== '' && $experiment_id=='' &&  $job_id==0)
    //    throw new ns_exception("No experiment, sample, or region specified as target of requested job!");
    if (sizeof($jobs) == 0){
      //die($job_id);
      if ($job_id != 0){
	$jobs[0] = $specified_job;
      }
      else{
	$jobs[0] = new ns_processing_job;
	$jobs[0]->id=$job_id;
	$jobs[0]->experiment_id=$experiment_id;
	$jobs[0]->sample_id=$sample_id;
	$jobs[0]->region_id=$region_id;
	$jobs[0]->image_id=$image_id;
	$jobs[0]->maintenance_task = $_POST['maintenance_task'];
	$jobs[0]->maintenance_flag = $_POST['maintenance_flag'];
	if ($jobs[0]->id == '')	$jobs[0]->id = 0;
	if ($jobs[0]->experiment_id == '')	$jobs[0]->experiment_id = 0;
	if ($jobs[0]->sample_id == '')		$jobs[0]->sample_id = 0;
	if ($jobs[0]->region_id == '')		$jobs[0]->region_id = 0;
	if ($jobs[0]->image_id == '')		$jobs[0]->image_id = 0;
	
	if ($region_id != 0 && $sample_id == 0){
	  //  die("WHA")
	  $query = "SELECT sample_id FROM sample_region_image_info WHERE id=$region_id";
	  $sql->get_row($query,$res);
	  $jobs[0]->sample_id = $res[0][0];
	  
	}
      }
    }
    
    //var_dump($jobs[0]->operations); die("");
  }
  $query_parameters = $_SERVER['QUERY_STRING'];
  
  $pos = strpos($query_parameters,"include_censored=");
  // die(":".$pos);
  if ($pos === FALSE)
    $query_parameters_without_censored = $query_parameters;
  else{
    $query_parameters_without_censored = substr($query_parameters,0,$pos) .
      substr($query_parameters,$pos+19);
   # die($query_parameters_without_censored);
  }
  //load all jobs
  for ($i = 0; $i < sizeof($jobs); $i++){
    if ($jobs[$i]->id != 0){
      $jobs[$i]->load_from_db($jobs[$i]->id,$sql);
    }
  }
  if ($jobs[0]->id != 0)
    $page_title = "Inspect";

/*
  if ($_POST['set_control_strain']){
    $region_id = $_POST['control_region_id'];
    if ($region_id == 'none')
      $region_id = '';
    $query = "UPDATE experiments SET control_strain_for_device_regression = '" . $region_id . "' WHERE id = " . $experiment_id;
    //echo $query
    
    $sql->send_query($query);
    
    
  }
  */
  
  if ($job_type == $IS_EXPERMENT){
    foreach($experiments_to_process as $e){
    $query = "SELECT r.id,r.strain, r.strain_condition_1,r.strain_condition_2,r.strain_condition_3 "
      ."FROM sample_region_image_info as r, capture_samples as s "
      ."WHERE r.sample_id = s.id AND s.experiment_id = " . $e;
    $sql->get_row($query,$exps);
      for ($i = 0; $i < sizeof($exps); $i++){
	$strain = $exps[$i][1];
	if ($exps[$i][2] != '')
	  $strain .= "::" . $exps[$i][2];
	//	if ($exps[$i][3] != '')
	// $strain .= "::" . $exps[$i][3];
	$experiment_strains[$strain] = $exps[$i][0];
      }
   // $query = "SELECT control_strain_for_device_regression FROM experiments WHERE id = " . $experiment_id;
  //  $sql->get_row($query,$exps);
    
  //  $experiment_control_strain = $exps[0][0];
    }
  }
  if ($_POST['censor_images_after_last_valid_sample']){

    for ($i = 0; $i < sizeof($jobs); $i++){
      if ($jobs[$i]->region_id != 0){
	$query = "SELECT time_of_last_valid_sample FROM sample_region_image_info WHERE id = " . $jobs[$i]->region_id;
	$sql->get_value($query,$etime);

	$query = "UPDATE sample_region_images SET censored=0 WHERE region_info_id= " . $jobs[$i]->region_id . " AND capture_time <= " . $etime;
	$sql->send_query($query);

	$query = "UPDATE sample_region_images SET censored=1 WHERE region_info_id= " . $jobs[$i]->region_id . " AND capture_time > " . $etime;
	$sql->send_query($query);
      }
    }
  }
  if ($_POST['censor_none']){
    for ($i = 0; $i < sizeof($jobs); $i++){
      if ($jobs[$i]->region_id != 0){
    
	$query = "UPDATE sample_region_images SET censored=0 WHERE region_info_id= " . $jobs[$i]->region_id;
	$sql->send_query($query);
      }
    }
  }
  if($specified_all_experiments)
	$back_url = "view_experiments.php";
  else $back_url =" manage_samples.php?experiment_id={$jobs[0]->experiment_id}&hide_sample_jobs=1&hide_region_jobs=1";

  $refresh_url = 'view_processing_job.php?';
  foreach($query_string as $k => $v)
	$refresh_url.= $k . '=' .$v. '&';

  $value_to_set = '';
  if ($_POST['censor']){
    $value_to_set = 'censored = 1';
  }
  if ($_POST['uncensor']){
    $value_to_set = 'censored = 0';
  }
  if ($_POST['exclude']){
    $value_to_set = 'excluded_from_analysis=1';
  }
  if ($_POST['unexclude']){
    $value_to_set = 'excluded_from_analysis=0';
  }
  if ($value_to_set != ''){
      for ($i = 0; $i < sizeof($jobs); $i++){
      if($jobs[$i]->region_id != 0){
	$query = "UPDATE sample_region_image_info SET $value_to_set WHERE id = " . $jobs[$i]->region_id;
	//	echo $query . "<BR>";//	die($query);
	$sql->send_query($query);
      }
      }

    header("Location: $back_url\n\n");
	die("");
  }
  if ($_POST['update_region_information'] !=''){
	$start_minute = $_POST['start_minute'];
    	$start_hour = $_POST['start_hour'];
    	$start_day = $_POST['start_day'];
    	$start_month = $_POST['start_month'];
    	$start_year = $_POST['start_year'];

    	$end_minute = $_POST['end_minute'];
    	$end_hour = $_POST['end_hour'];
    	$end_day = $_POST['end_day'];
    	$end_month = $_POST['end_month'];
    	$end_year = $_POST['end_year'];


	//	die( "$end_hour:$end_minute $end_month/$end_day/$end_year");
    $start_time = mktime($start_hour, $start_minute, 0, $start_month,$start_day,$start_year);
    $end_time_date = mktime($end_hour, $end_minute, 0, $end_month, $end_day, $end_year);
    $end_time_age = $_POST['end_age'];


    $strain = $_POST['strain_spec'];
    $strain_condition_1 = $_POST['strain_condition_1_spec'];
    $strain_condition_2 = $_POST['strain_condition_2_spec'];
    $strain_condition_3 = $_POST['strain_condition_3_spec'];
    $culturing_temperature = $_POST['culturing_temperature'];
    $experiment_temperature = $_POST['experiment_temperature'];
    $food_source = $_POST['food_source'];
    $environment_conditions = $_POST['environment_conditions'];
    
    $set_strain = $_POST['set_strain']=='1';
    $set_strain_condition_1 = $_POST['set_strain_condition_1']=='1';
    $set_strain_condition_2 = $_POST['set_strain_condition_2']=='1';
    $set_strain_condition_3 = $_POST['set_strain_condition_3']=='1';
    $set_culturing_temperature = $_POST['set_culturing_temperature']=='1';
    $set_experiment_temperature = $_POST['set_experiment_temperature']=='1';
    $set_food_source = $_POST['set_food_source']=='1';
    $set_environment_conditions = $_POST['set_environment_conditions']=='1';
    $set_start_time = $_POST['set_start_time']=='1';
    $set_end_time_date = $_POST['set_end_time_date']=='1';
    $set_end_time_age = $_POST['set_end_time_age']=='1';


    $vals = array();
    if ($set_strain) array_push($vals,"strain='" .mysql_real_escape_string($strain)."'");
    if ($set_strain_condition_1) array_push($vals,"strain_condition_1='" .mysql_real_escape_string($strain_condition_1)."'");
    if ($set_strain_condition_2) array_push($vals,"strain_condition_2='" .mysql_real_escape_string($strain_condition_2)."'");
    if ($set_strain_condition_3) array_push($vals,"strain_condition_3='" .mysql_real_escape_string($strain_condition_3)."'");
    if ($set_culturing_temperature) array_push($vals,"culturing_temperature='" .mysql_real_escape_string($culturing_temperature)."'");
    if ($set_experiment_temperature) array_push($vals,"experiment_temperature='" .mysql_real_escape_string($experiment_temperature)."'");
    if ($set_food_source) array_push($vals,"food_source='" .mysql_real_escape_string($food_source)."'");
    if ($set_environment_conditions) array_push($vals,"environmental_conditions='" .mysql_real_escape_string($environment_conditions)."'");
    if ($set_start_time) array_push($vals,"time_at_which_animals_had_zero_age='$start_time'");
    if ($set_end_time_date)   array_push($vals,"time_of_last_valid_sample='$end_time_date'");
    
    if ($set_end_time_age){
      $time_in_seconds = floor($end_time_age*60*60*24);
      array_push($vals,"time_of_last_valid_sample=time_at_which_animals_had_zero_age + " . $time_in_seconds);
      // var_dump($vals);
      //  die("");
    }

    if (sizeof($vals) == 0)
	die("No fields selected for update!");
    //  echo "$end_hour:$end_minute $end_month/$end_day/$end_year";
    $set_str = $vals[0];
    for ($i = 1; $i < sizeof($vals); $i++)
	$set_str .= "," .  $vals[$i];

    for ($i = 0; $i < sizeof($jobs); $i++){
      if($jobs[$i]->region_id != 0){
	$query = "UPDATE sample_region_image_info SET $set_str WHERE id = " . $jobs[$i]->region_id;
	//		echo $query . "<BR>";	die($query);
	$sql->send_query($query);
      }
    }
//die("");
    header("Location: $back_url\n\n");
	die("");
  }


  $delete_files_from_disk = $_POST['delete_files'] == "1";
  // if ($delete_files)
    //   die("YES");
  //else die("NO");

  
  if ($_POST['pause_job'] || $_POST['unpause_job']){
    $paused = ($_POST['pause_job']!='')?"1":"0";

    for ($i = 0; $i < sizeof($jobs); $i++){
      //if ($jobs[$i]->id == 0)
      //	continue;
      $query = "UPDATE processing_job_queue SET paused=$paused WHERE job_id = " . $jobs[$i]->id;
      $sql->send_query($query);
      //echo $query . "<BR>";
      $query = "UPDATE processing_jobs SET paused=$paused WHERE id = " . $jobs[$i]->id;
      // echo $query . "<BR>";
      $sql->send_query($query);
    }
    header("Location: $back_url\n\n");
    die("");
  }
  if ($_POST['cancel_captures']){
    $sample_ids = array();
    $device_names = array();
    for ($i = 0; $i < sizeof($jobs); $i++){
      $e_id = $jobs[$i]->experiment_id;
      if ($e_id == 0)
	throw ns_exception("No experiment id specified for job!");
      
      //echo var_dump($jobs[$i]);
      if ($jobs[$i]->region_id != 0){
	$query = "SELECT r.sample_id, s.device_name FROM sample_region_image_info as r, capture_samples as s WHERE r.id = " . $jobs[$i]->region_id . " AND s.id = r.sample_id";
	$sql->get_row($query,$res);
	if (sizeof($res) == 0)
	  throw ns_exception("Could not find sample for region " . $jobs[$i]->region_id);
	
	$sample_ids[$res[0][0]] = 1;
	$device_names[$res[0][1]] = 1;
      }
      if ($jobs[$i]->sample_id != 0){
	$query = "SELECT device_name FROM capture_samples WHERE id = " . $jobs[$i]->sample_id;
	$sql->get_row($query,$res);
	if (sizeof($res)==0)
	  throw ns_exception("Could not find sample id " . $jobs[$i]->sample_id);
	$sample_ids[$jobs[$i]->sample_id] = 1;
	$device_names[$res[0][0]] = 1;
      }
      else if ($jobs[$i]->experiment_id != 0){
	$query = "SELECT id, device_name FROM capture_samples WHERE experiment_id = " . $jobs[$i]->experiment_id;
	$sql->get_row($query,$res);
	for ($i = 0; $i < sizeof($res); $i++){
	  $sample_ids[$res[$i][0]] = 1;
	  $device_names[$res[$i][1]] = 1;
	}
      }
    }
    $lock_cs = "LOCK TABLES capture_schedule WRITE";
    $unlock_cs = "UNLOCK TABLES";
    
    $sql->send_query($lock_cs);
    foreach ($sample_ids as $sample_id => $d){
      $query = "UPDATE capture_schedule SET censored=1 WHERE sample_id='$sample_id' AND time_at_start='0' AND missed='0'";
      //echo $query . "<br>";
      $sql->send_query($query);
    }
    $sql->send_query($unlock_cs);
    
    foreach($device_names as $device_name=>$d){  
      $query = "DELETE FROM autoscan_schedule WHERE device_name ='" . $device_name . "'";
      //echo $query . "<br>";
      $sql->send_query($query); 
    }
    header("Location: $back_url\n\n");
    die("");
  } 
  
  if ($_POST['retry_transfer_to_long_term_storage'] !=''){
    
    for ($i = 0; $i < sizeof($jobs); $i++){
      $e_id = $jobs[$i]->experiment_id;
      if ($e_id == 0)
	throw ns_exception("No experiment id specified for job!");
      
     //echo var_dump($jobs[$i]);
      if ($jobs[$i]->region_id != 0)
	throw ns_exception("Cannot schedule a cache transfer jobon a region!");
      if ($jobs[$i]->sample_id != 0)
	ns_attempt_to_retry_transfer_to_long_term_storage($jobs[$i]->sample_id,"",0,$sql);
      else if ($jobs[$i]->experiment_id != 0)
	ns_attempt_to_retry_transfer_to_long_term_storage(0,"",$jobs[$i]->experiment_id,$sql);
    }
    
    header("Location: $back_url\n\n");
    die("");
  }

    if ($_POST['submit'] != ''){
    //var_dump($jobs);
    //	die("");
    //$query = "LOCK TABLES processing_job_queue write";
    $sql->send_query($set_autocommit_query);
    $sql->send_query($lock_job_queue_query);
    $bg = "BEGIN";
    $ed = "COMMIT";
    $sql->send_query($bg);
    for ($i = 0; $i < sizeof($jobs); $i++){
      if ($jobs[$i]->id != 0){
	//this job will generate new processing_job_queue entries, but old ones may exist.
	//delete them to avoid duplicated
	$query = "DELETE FROM processing_job_queue WHERE job_id='{$jobs[$i]->id}'";
	$sql->send_query($query);
      }
    }
    $sql->send_query($ed);
    $sql->send_query($unlock_tables_query);
 
  
    //$sql->send_query($lock_jobs_query);
    //$sql->send_query($bg);
   for ($i = 0; $i < sizeof($jobs); $i++){
      $jobs[$i]->processor_id = $_POST['processor_id'];
      $jobs[$i]->video_timestamp_type = $_POST['video_add_timestamp'];
      if ($_POST['urgent'] == 69){
	$jobs[$i]->pending_another_jobs_completion = 1;
	$jobs[$i]->urgent = 0;
      }
      else $jobs[$i]->urgent = $_POST['urgent'];
      $jobs[$i]->paused = $_POST['paused'];
      $jobs[$i]->processed_by_push_scheduler = 0;
      for ($j = 0; $j <= $NS_LAST_PROCESSING_JOB ; $j++){
	$jobs[$i]->operations[$j] = 0;
	if ($_POST["op$j"] == '1'){
	  $jobs[$i]->operations[$j] = 1;
	}
      }
      $jobs[$i]->job_name = $jobs[$i]->experiment_id . "=" . //$sample_name_hash[$jobs[$i]->sample_id] . "=" . $region_name_hash[$jobs[$i]->region_id];
      //echo $jobs[$i]->job_name . "<BR>";
      $jobs[$i]->time_submitted = ns_current_time();
      $jobs[$i]->subregion_position_x = $_POST['subregion_position_x'];
      $jobs[$i]->subregion_position_y = $_POST['subregion_position_y'];
      $jobs[$i]->subregion_width = $_POST['subregion_width'];
      $jobs[$i]->subregion_height = $_POST['subregion_height'];
      $jobs[$i]->subregion_start_time = $_POST['subregion_start_time'];
      $jobs[$i]->subregion_stop_time = $_POST['subregion_stop_time'];
    //  echo "Submitting job " . $i . "<BR>";
      $jobs[$i]->save_to_db($sql);
      $jobs[$i]->load_from_db($jobs[$i]->id,$sql);
    }
    //$query = "UNLOCK TABLES";
   //$sql->send_query($ed);
   //$sql->send_query($unlock_tables_query);
    $sql->send_query($clear_autocommit_query);
    ns_update_job_queue($sql);
    header("Location: $back_url\n\n");
   die("");
  }	
  //see if any image processing steps have been requested cleare
  $operations_to_clear = array_fill(0,$NS_LAST_PROCESSING_JOB+1,0);

  for ($i = 0; $i <= $NS_LAST_PROCESSING_JOB ; $i++){
    if (@$_POST["clear_$i"] === '1'){
      $operations_to_clear[$i] = 1;
    }
  }

  //var_dump($samples_to_process);
  //die("sdffgdfgdfg" );// . sizeof($samples_to_process)+1);
  //if the user has requested a deletion of an entire sample region, do so.
  if (($clear_region != '' || $delete_sample_images != '')&&  sizeof($samples_to_process) != 0){
    $sample_regions = array();
    
    
	if (!$live_dangerously)
	throw new ns_exception("I can't let you do that Dave!  (Enable dangerous operations to continue)");

    for ($i = 0; $i < sizeof($samples_to_process); $i++){
      $query = "SELECT id FROM sample_region_image_info WHERE sample_id = " . $samples_to_process[$i];
      //echo $query . "<BR>";
      $sql->get_row($query,$sample_regions[$i]);
    }
    
    $sql->send_query($lock_jobs_query,TRUE);
    for ($i = 0; $i < sizeof($samples_to_process); $i++){
      
      if ($delete_files_from_disk){
	//make a processing job for the sample--this will allow us to delete all temprary data later on.
	$job = new ns_processing_job();
	$job->id = 0;
	$job->maintenance_task =  $ns_maintenance_tasks['ns_maintenance_delete_files_from_disk_request'];
	$job->time_submited = ns_current_time();
	for ($d = 0; $d < sizeof($file_deletion_job->operations); $d++)
	  $job->operations[$d] = 0;
	
	if ($delete_sample_images!=''){
	  $job->sample_id = $samples_to_process[$i];
	  $job->save_to_db($sql);
	  
	  
	  //  ns_delete_sample_from_database($samples_to_process[$i],$sql);
	}
	else if ($clear_region !=''){
	  for ($b = 0; $b < sizeof($sample_regions[$i]); $b++){  
	    $file_deletion_job = clone $job;
	    $file_deletion_job->region_id = $sample_regions[$i][$b][0];
	    $file_deletion_job->save_to_db($sql);
	    
	    ns_delete_region_from_database($sample_regions[$i][$b][0],$sql);
	  }
       
	}
	else{
	  throw new ns_ex("Unknown error!");
	}
	
      }
    }
    $sql->send_query($unlock_tables_query);
    if (!($clear_region=='')){
      for ($i = 0; $i < sizeof($samples_to_process); $i++){
	$query = "UPDATE captured_images SET problem = 0, mask_applied=0 WHERE sample_id = " . $samples_to_process[$i];
	//echo $query . "<BR>";
	$sql->send_query($query,TRUE);
      }
    }
 

    
    
    ns_update_job_queue($sql);
    if(!($clear_region == ''))
      $operations_to_clear = array_fill(0,$NS_LAST_PROCESSING_JOB,1); //this will trigger deletion of all temporary images in the region
  }

  $image_deletion_requested = FALSE;
  $last_deletion_operation = 0;
  //sort out which operations should be cleared;
  //specifically, deleting any of the early image processing task should
  //automatically delete all later tasts except for static mask, heat map, and temporal interpolation
  if ($job_type == $IS_REGION){
    for ($i = 0; $i <= $ns_processing_tasks['ns_process_worm_detection']; $i++){
      if ($operations_to_clear[$i] == 1 && $i != $ns_processing_tasks['ns_process_lossy_stretch']){
	
	for ($j = $i+1; $j <= $NS_LAST_PROCESSING_JOB; $j++){
	  if ($operations_to_clear[$j] == 0)
	    $operations_to_clear[$j] = 2;
	}
	break;
      }
    }
    
    //deleting worm detection jobs should delete all later jobs
    //var_dump($operations_to_clear);
    //die("");
    if ($operations_to_clear[$ns_processing_tasks['ns_process_worm_detection']] == 1 ||
	$operations_to_clear[$ns_processing_tasks['ns_process_region_vis']] == 1){
      if ($operations_to_clear[$ns_processing_tasks['ns_process_region_vis']] == 1)
	$start = $ns_processing_tasks['ns_process_region_vis']+1;
      else $start = $ns_processing_tasks['ns_process_worm_detection']+1;
      for ($j = $start; $j <= $NS_LAST_PROCESSING_JOB; $j++){
	if ($operations_to_clear[$j] == 0)
	  $operations_to_clear[$j] = 2;
      }
    }
    
    if ($operations_to_clear[$ns_processing_tasks['ns_process_movement_coloring']]==1){
      for ($j = $ns_processing_tasks['ns_process_movement_coloring']+1; $j <= $NS_LAST_PROCESSING_JOB; $j++){
	if ($operations_to_clear[$j] == 0)
	  $operations_to_clear[$j] = 2;
      }
    }
    
    
    
    //make sure that static masks, heat maps, and temporal interpolation are deleted only explicitly.
    if ($operations_to_clear[$ns_processing_tasks['ns_process_static_mask']] != 1 &&
	$operations_to_clear[$ns_processing_tasks['ns_process_heat_map']] != 1){
      $operations_to_clear[$ns_processing_tasks['ns_process_static_mask']] = 0;
      $operations_to_clear[$ns_processing_tasks['ns_process_heat_map']] = 0;
    }
    if ($operations_to_clear[$ns_processing_tasks['ns_process_temporal_interpolation']] != 1)
      $operations_to_clear[$ns_processing_tasks['ns_process_temporal_interpolation']] = 0;
    
    $operations_to_clear[$ns_processing_tasks['ns_process_compile_video']] = 0;  
    $operations_to_clear[$ns_processing_tasks['ns_process_analyze_mask']] = 0;
    $operations_to_clear[$ns_processing_tasks['ns_process_apply_mask']] = 0;
    $operations_to_clear[$ns_processing_tasks['ns_process_resized_sample_image']] = 0;
  }
  
  for ($i = 0; $i <= $NS_LAST_PROCESSING_JOB; $i++){
    if ($operations_to_clear[$i] > 0){
      $image_deletion_requested = TRUE;
      $last_deletion_operation = $i;
    }
  }
  
  if ($_POST['delete_images'] != ''){
    //if any sample_region processing step images are identified for deletion, delete them
   
    if ($image_deletion_requested){
      //die("clear value: " . $clear_value);
      for ($b = 0; $b < sizeof($jobs); $b++){
	
	$file_deletion_job = clone $jobs[$b];	
	//die("WHOOP");
	if ($delete_files_from_disk){
	  $file_deletion_job->maintenance_task =  		$ns_maintenance_tasks['ns_maintenance_delete_files_from_disk_request'];
	}
	else{ 
	  $file_deletion_job->maintenance_task = 
	    $ns_maintenance_tasks['ns_maintenance_delete_images_from_database'];
	}
	//die("IT IS: " . $file_deletion_job->maintenance_task);
	for ($i = 0; $i < sizeof($file_deletion_job->operations); $i++){
	  $file_deletion_job->operations[$i] = $operations_to_clear[$i]?1:0;
	}
	if (!$live_dangerously){
	  if($file_deletion_job->operations[0] != 0 && 
	     $file_deletion_job->maintenance_flag == 0){
	    throw new ns_exception("To delete capture images that have not yet been processed, you must enable dangerous operations.");
	  }
	  
	} 
	//var_dump($file_deletion_job->operations);
	//die();
	$file_deletion_job->maintenance_flag = $_POST['maintenance_flag'];
	//die($file_deletion_job->maintenance_flag);
	$file_deletion_job->id = 0;
	//echo "<BR>----<br>";var_dump($file_deletion_job);echo "<BR><br>";
	$file_deletion_job->save_to_db($sql);
      }
      ns_update_job_queue($sql);
      header("Location: $back_url\n\n");
      die("");
    }
  }
  
  if ($_POST['delete_job'] != ''){
	//die(var_dump($jobs));
    // $lock_job_queue_query);
    //$sql->send_query($set_autocommit_query);
    //$sql->send_query($lock_job_queue_query);
    // $bg = "BEGIN";
    //$ed = "COMMIT";
    //$sql->send_query($bg);
    for ($i = 0; $i < sizeof($jobs); $i++){
      $query = "DELETE FROM processing_job_queue WHERE job_id={$jobs[$i]->id}";
      $sql->send_query($query);
    }
    //$sql->send_query($ed);
    //$sql->send_query($unlock_tables_query);
  
    //$sql->send_query($unlock_tables_query);
    //$commit_query = "COMMIT";
    //$sql->send_query($commit_query);
    //$sql->send_query($lock_jobs_query);
    //$sql->send_query($bg);
    for ($i = 0; $i < sizeof($jobs); $i++){
      $query = "DELETE FROM processing_jobs WHERE id={$jobs[$i]->id}";
      //echo $query . "<BR>";
      
      $sql->send_query($query);
    }
    //$sql->send_query($ed);
    //$sql->send_query($unlock_tables_query);
    //$sql->send_query($clear_autocommit_query);
    header("Location: $back_url\n\n");
    die("");
  }
  else $page_title = "Create New";
  $page_title .= " Processing Job";
  if (sizeof($jobs) == 0)
    die("No Jobs or job subjects could be found matching the specifications.");
  //$jobs[0]->get_names($sql);

  
  if ($jobs[0]->image_id == 0 && $jobs[0]->region_id == 0 && $jobs[0]->sample_id == 0 && $jobs[0]->experiment_id == 0)
    throw new ns_exception("You must specifiy an image or an image group on which to apply the processing job.");
  
    $last_experiment_id = 0;
    $last_sample_id = 0;
    $out_first_experiment = TRUE;
    $out_first_sample = TRUE;
    $out_first_region = TRUE;
    for ($i = 0; $i < sizeof($jobs); $i++){
      if ($jobs[$i]->experiment_id != $last_experiment_id){
	//$target_text.='a';
	if (!$out_first_experiment){
	  if ($jobs[$i]->sample_id != 0) $target_text .= "<font size=\"+1\"> ]</font>";
	  $target_text.="<BR>";
	}
	
	else $out_first_experiment = $jobs[$i]->sample_id != 0;
	$target_text .= " <font size=\"+1\">". $experiment_name_hash[$jobs[$i]->experiment_id];
	if ($jobs[$i]->sample_id != 0) $target_text .= "[";
	$target_text .= "</font>";
	$last_experiment_id = $jobs[$i]->experiment_id;
	$last_sample_id = '';
	$out_first_sample = TRUE;
      
      }
      if ($jobs[$i]->sample_id != $last_sample_id ){
	if (!$out_first_sample && $jobs[$i]->region_id != 0) $target_text .= "] ";
	else $out_first_sample =  $jobs[$i]->region_id == 0;
	//XXX
	//var_dump($jobs[$i]->sample_id);
	//	die($sample_name_hash);
	$target_text .= $sample_name_hash[$jobs[$i]->sample_id] . " ";
	//$target_text .= $jobs[$i]->sample_name;
	if ($jobs[$i]->region_id != 0) $target_text .= "[";
	$last_sample_id = $jobs[$i]->sample_id;
	$out_first_region = TRUE;
      }
      if ($jobs[$i]->region_id != 0){
	$target_text.="<font color=\"gray\" size=\"-1\">";
		if (!$out_first_region) $target_text.= ",";
		else $out_first_region = FALSE;
		$target_text .= $region_name_hash[$jobs[$i]->region_id];
		$target_text .="</font>";
	}
      /*if ($jobs[0]->id == 0)
       $target_text .= "(New) ";
       else */
	//$target_text .= $region_name_hash[$jobs[$i]->region_id] . " ";
    }
    if (!$out_first_sample ) $target_text .= "]";
    if ($out_first_experiment) $target_text .= "<font size=\"+1\">]</font>";
  }
catch(ns_exception $e){
  die($e->text);
}
$hide_image_delete_button=FALSE;
//edit an image deletion job
if ($query_string['delete_file_taskbar']!=''){
  $view_processing_taskbar = FALSE;
  // die("BLO");
  $view_video_taskbar = FALSE;
  $view_maintenance_taskbar = FALSE;
  $view_delete_images_taskbar = TRUE;
  $view_boundaries_taskbar = FALSE;
  $view_control_strain_taskbar = FALSE;
 }
//create a new job
 else if (sizeof($jobs) == 0 || $jobs[0]->id == 0){
	if ($job_type == $IS_EXPERIMENT){
	$view_processing_taskbar = FALSE;
	$view_video_taskbar = FALSE;
	$view_maintenance_taskbar = TRUE;
	$view_delete_images_taskbar = FALSE;
	$view_boundaries_taskbar = FALSE;
	//$view_control_strain_taskbar = TRUE;
	
	}
	else{
	$view_processing_taskbar = TRUE;
	$view_video_taskbar = TRUE;
	$view_maintenance_taskbar = TRUE;
	$view_delete_images_taskbar = TRUE;
	$view_boundaries_taskbar = TRUE;
	}
 }
//edit a video job
 else if ($jobs[0]->operations[$ns_processing_tasks['ns_process_compile_video']]!= 0){
    $view_processing_taskbar = FALSE;
    $view_video_taskbar = TRUE;
    $view_maintenance_taskbar = FALSE;
    $view_delete_images_taskbar = FALSE;
    $view_boundaries_taskbar = FALSE;
  }
//edit a maintenenace job
 else if ($jobs[0]->maintenance_task != 0){
    $view_processing_taskbar = FALSE;
    $view_video_taskbar = FALSE;
    $view_maintenance_taskbar = TRUE;
	
    if ($jobs[0]->maintenance_task == $ns_maintenance_tasks['ns_maintenance_delete_files_from_disk_request'] ||
	$jobs[0]->maintenance_task ==  $ns_maintenance_tasks['ns_maintenance_delete_files_from_disk_action'] ||
	$jobs[0]->maintenance_task ==  $ns_maintenance_tasks['ns_maintenance_delete_images_from_database']){

	$hide_image_delete_button=TRUE;
	$view_delete_images_taskbar= TRUE;
	}
	else $view_delete_images_taskbar = FALSE;

    $view_boundaries_taskbar = FALSE;
  }
  else{
	//var_dump($jobs[0]);
    $view_processing_taskbar = TRUE;
    //  die("BLOOP");
    $view_video_taskbar = FALSE;
    $view_maintenance_taskbar = FALSE;
    $view_delete_images_taskbar = FALSE;
    $view_boundaries_taskbar = FALSE;
  }
/*if ($job_type == $IS_EXPERIMENT){
 
  $view_processing_taskbar = FALSE;
  $view_video_taskbar = FALSE;
  $view_boundaries_taskbar = FALSE;

  }*/
if ($job_type != $IS_REGION){
  $view_boundaries_taskbar = FALSE;
 }
 //die("$view_delete_image_taskbar);

$query = "SELECT id, name, ip, last_ping, comments, long_term_storage_enabled, port FROM hosts ORDER BY name DESC";
$sql->get_row($query,$hosts);
$hosts[sizeof($hosts)] = array(0,"(Any)");

if ($jobs[0]->experiment_id != 0)
	display_worm_page_header($page_title, "<a href=\"$back_url\">[Back to {$jobs[0]->experiment_name} Samples]</a>");
else display_worm_page_header($page_title);
?>


<span class="style1">Job Target(s):</span><br><?php echo $target_text?> 
<?php 
if ($job_type != $IS_EXPERIMENT){
  if ($job_type == $IS_SAMPLE)
    $sub = "Samples";
  else $sub = "Regions";
  if ($query_string["include_censored"] == 1){
    echo "<a href=\"view_processing_job.php?" . $query_parameters_without_censored."&include_censored=2\">[Include Only Censored $sub]</a>";
  }
  else if ($query_string["include_censored"] == 2){
    echo "<a href=\"view_processing_job.php?" . $query_parameters_without_censored."&include_censored=0\">[Exclude Censored $sub]</a>";
  }
  else{
    echo "<a href=\"view_processing_job.php?" . $query_parameters_without_censored."&include_censored=1\">[Include Censored $sub]</a>";
    
  }
}
			       //<?php if ($condition_3 == 'Nash') echo "<br>follow the white rabbit neo..."?>

<br><br>
 
<table width="100%"><tr><td valign="top">
<table width="100%"><tr><td valign="top">

<!--*********************************
Schedule an Image Processing Job Begin
***********************************-->
<?php if ($view_processing_taskbar){ ?>

<form action="view_processing_job.php?<?php echo $query_parameters?>" method="post">
<table align="center" border="0" cellpadding="0" cellspacing="1" bgcolor="#000000"><tr><td>

<table border="0" cellpadding="4" cellspacing="0" width="100%">
<tr <?php echo $table_header_color?> ><td colspan=2><b>Schedule a Job for Individual Images</a></td></tr>
  <tr><td bgcolor="<?php echo  $table_colors[0][0] ?>" >Submission time</td>
	  <td bgcolor="<?php echo  $table_colors[0][1] ?>"><?php echo format_time($jobs[0]->time_submitted)?></td>
  </tr>  
  <tr>
    <td bgcolor="<?php echo  $table_colors[1][0] ?>" >Priority</td>
	  <td bgcolor="<?php echo  $table_colors[1][1] ?>">
		<select name="urgent" size="1">
	   	 	<option value="1" <?php if ($jobs[0]->urgent==1) echo "selected"?> >Urgent</option>
	   	 	<option value="0" <?php if (sizeof($jobs)==0 || $jobs[0]->urgent==0) echo "selected "?> >Standard</option>
<option value="-1" <?php if ($jobs[0]->urgent==-1) echo "selected "?> >Low</option>
	  	</select><br>
	</td>
  </tr>  
<!--
	<tr>
		<td bgcolor="<?php echo  $table_colors[0][0] ?>" >Paused</td>
		  <td bgcolor="<?php echo  $table_colors[0][1] ?>"><select name="processor_id">
			<?php for ($i = sizeof($hosts)-1; $i >= 0; $i--){ 
				echo "<option value=\"{$hosts[$i][0]}\"";
				if ($jobs[0]->processor_id == $hosts[$i][0]) echo " selected";
				echo ">" . $hosts[$i][1] . "</option>\n";
				}
			?>
		  </select></td>
	  </tr>--></table>
	 <table border="0" cellpadding="4" cellspacing="0" width="100%">
	<tr <?php echo $table_header_color?>><td colspan=3>Processing Tasks</td></tr>
				     <?php if ($job_type == $IS_REGION){?><tr ><td bgcolor="<?php echo  $table_colors[1][0] ?>" colspan=3><font size="-1">Only "Median Filter", "Threshold" and "Worm Detection" are necessary.</font></td></tr><?php }?>
	<?php 
				     $cc = 0;
				     $start = 1;
				     $finish = $NS_LAST_PROCESSING_JOB;
				   
				     if ($job_type == $IS_REGION || $region_id==="all")
				       $start = 2;
				     else $finish = $ns_processing_tasks['ns_process_apply_mask'];

				     for ($i = (int)$start; $i <= (int)$finish ; $i++){
	if ($job_type == $IS_SAMPLE){
		if ($i != $ns_processing_tasks['ns_process_apply_mask'] &&
		    $i !=$ns_processing_tasks['ns_process_resized_sample_image'])
		continue;
	}
else{
  /*	if ($i == $ns_processing_tasks['ns_process_analyze_mask'] ||  $i==$ns_processing_tasks['ns_process_resized_sample_image'] || $i==$ns_processing_tasks['ns_process_region_vis'] || $i==$ns_processing_tasks['ns_process_region_interpolation_vis'] || $i==$ns_processing_tasks['ns_process_interpolation_vis'] || $i==$ns_processing_tasks['ns_process_movement_coloring'] ||  $i==$ns_processing_tasks['ns_process_movement_posture_aligned_visualization'] || $i==$ns_processing_tasks['ns_process_movement_posture_visualization'] || $i==$ns_processing_tasks['ns_process_movement_mapping'] || $i==$ns_processing_tasks['ns_process_temporal_interpolation'] || $i==$ns_processing_tasks['ns_process_movement_coloring_with_graph']  || $i==$ns_processing_tasks['ns_process_movement_coloring_with_survival'] || $i==$ns_processing_tasks['ns_process_posture_vis']) continue;*/
  if ($jobs[0]->operations[$i]==0 && array_search($i,$region_processing_jobs_to_display) === FALSE)
    continue;
}
  $cc++;
		$clrs = $table_colors[($cc+1)%2];
	?>
  		<tr>
		<td bgcolor="<?php echo  $clrs[1] ?>" ><input name="op<?php echo $i?>" type="checkbox" value="1" <?php if ($jobs[0]->operations[$i]) echo "checked"?>></td>
		<td bgcolor="<?php echo  $clrs[0] ?>" ><?php echo $ns_processing_task_labels[(int)$i]?></td>
	    <td bgcolor="<?php echo  $clrs[0] ?>" >

</td>
		</tr>
	<?php
	}
	?>
				     <td bgcolor="<?php echo  $table_colors[0][1] ?>" colspan=3 ><div align="right"><input name="submit" type="submit" value="Save Job"><input name="delete_job" type="submit" value="Delete Job"><?php if ($jobs[0]->paused==1){?><input name="unpause_job" type="submit" value="Unpause Job"><?php } else{?><input name="pause_job" type="submit" value="Pause Job"><?php }?>
</div>

</td></tr>
</table></td></tr>
</table>
</form>

				     <?php }?>
<!--********************************
Schedule an Image Procesing Job End
*********************************-->

<br></td></tr><td><tr>

 <?php 
 //die("CCC" + $view_video_taskbar);
?>

<?php if ($view_video_taskbar){ ?>

<td valign="top">

<!--********************************
Make A Video Job Start
*********************************-->
<?php if ($view_video_taskbar===TRUE){?>

<form action="view_processing_job.php?<?php echo $query_parameters?>" method="post">

<input type="hidden" name="op<?php echo $ns_processing_tasks[ns_process_compile_video];?>" value="1" >
<table align="center" border="0" cellpadding="0" cellspacing="1" bgcolor="#000000"><tr><td>
<table border="0" cellpadding="4" cellspacing="0" width="100%">
<tr <?php echo $table_header_color?> ><td colspan=2><b>Create a Video</b></td></tr>
  <tr><td bgcolor="<?php echo  $table_colors[0][0] ?>" >Submission time</td>
	  <td bgcolor="<?php echo  $table_colors[0][1] ?>"><?php echo format_time($jobs[0]->time_submitted)?></td>
  </tr>  
  <tr>
    <td bgcolor="<?php echo  $table_colors[1][0] ?>" >Priority</td>
	  <td bgcolor="<?php echo  $table_colors[1][1] ?>">
		<select name="urgent" size="1">
	   	  name="urgent" size="1">
	   	 	<option value="1" <?php if ($jobs[0]->urgent==1) echo "selected"?> >Urgent</option>
    <option value="0" <?php if (sizeof($jobs)==0 || $jobs[0]->urgent==0) echo "selected "?> >Standard</option>
<option value="-1" <?php if ($jobs[0]->urgent==-1) echo "selected "?> >Low</option>
	  	</select><br>
	</td>
  </tr>  <!--
	<tr>
		<td bgcolor="<?php echo  $table_colors[0][0] ?>" >Host</td>
		  <td bgcolor="<?php echo  $table_colors[0][1] ?>"><select name="processor_id">
			<?php for ($i = sizeof($hosts)-1; $i >= 0; $i--){ 
				echo "<option value=\"{$hosts[$i][0]}\"";
				if ($jobs[0]->processor_id == $hosts[$i][0]) echo " selected";
				echo ">" . $hosts[$i][1] . "</option>\n";
				}
			?>
		  </select></td>
	-->  </tr>
	<tr>
		<td bgcolor="<?php echo  $table_colors[1][0] ?>" >Add Timestamp</td>
		      <td bgcolor="<?php echo  $table_colors[1][1] ?>">
				      <select name="video_add_timestamp">
				      <?php
				      global $ns_video_timestamp_types;
				     
				     
				      foreach ($ns_video_timestamp_types as $num=>$name){
   echo "<option value=\"$num\"";
   if ($num == 1)
     echo " selected";
   echo">$name</option>";
   
 }
				     
?>
				      </select>
		      </td>
	  </tr>
</table>
	 <table border="0" cellpadding="4" cellspacing="0" width="100%">
	<tr <?php echo $table_header_color?>><td colspan=3>Image Types</td></tr>
		<?php
	$cc = 0; 
	$start = 0;
$finish = $NS_LAST_PROCESSING_JOB;
				  
				     if ($job_type == $IS_REGION|| $region_id==="all")
				       $start = 0;
				     else $finish = $ns_processing_tasks['ns_unprocessed'];
				     for ($i = $start; $i <= $finish ; $i++){
	if ($job_type == $IS_SAMPLE){
		if ($i != $ns_processing_tasks['ns_unprocessed'] &&
		    $i !=$ns_processing_tasks['ns_process_resized_sample_image'])
		continue;
	}
else{
  /*	if ($i == $ns_processing_tasks['ns_process_analyze_mask'] || $i==$ns_processing_tasks['ns_process_region_vis'] || $i==$ns_processing_tasks['ns_process_region_interpolation_vis'] || $i==$ns_processing_tasks['ns_process_interpolation_vis']   || $i==$ns_processing_tasks['ns_process_movement_posture_visualization'] || $i==$ns_processing_tasks['ns_process_movement_mapping'] || $i==$ns_processing_tasks['ns_process_temporal_interpolation'] || $i==$ns_processing_tasks['ns_process_movement_coloring_with_graph']  || $i==$ns_processing_tasks['ns_process_movement_coloring_with_survival'] || $i==$ns_processing_tasks['ns_process_posture_vis']) continue;
*/
  if ($i != $ns_processing_jobs['ns_unprocessed'] && array_search($i,$region_processing_jobs_to_display) === FALSE)
    continue;

}
  $cc++;
  
		$clrs = $table_colors[($cc+1)%2];
	?>
  		<tr>
		<td bgcolor="<?php echo  $clrs[1] ?>" ><input name="op<?php echo $i?>" type="checkbox" value="1" <?php if ($jobs[0]->operations[$i]) echo "checked"?>></td>
		<td bgcolor="<?php echo  $clrs[0] ?>" ><?php echo $ns_processing_task_labels[$i]?></td>
	    <td bgcolor="<?php echo  $clrs[0] ?>" >
</td>
		</tr>
	<?php
	}
	?>
				  <td bgcolor="<?php echo  $table_colors[0][1] ?>" colspan=3 >
				  Subregion
				  <table cellspacing='1',cellpadding='0'>
				  <tr><td>x</td><td><input size=4 name="subregion_position_x" value="<?php echo $jobs[0]->subregion_position_x;?>"></td>
				      <td>y</td><td><input size=4 name="subregion_position_y" value="<?php echo $jobs[0]->subregion_position_y;?>"></td>
				      <td>t_start</td>
				      <td><input size=10 name="subregion_start_time" value="<?php echo $jobs[0]->subregion_start_time;?>"></td></tr>
				  <tr><td>w</td><td><input size=4 name="subregion_width" value="<?php echo $jobs[0]->subregion_width;?>">
				      <td>h</td><td><input size=4 name="subregion_height" value="<?php echo $jobs[0]->subregion_height;?>"></td>
				      <td>t_stop</td>
				      <td><input size=10 name="subregion_stop_time" value="<?php echo $jobs[0]->subregion_stop_time;?>"></td></tr>

				  </table>

	<div align="right"><input name="submit" type="submit" value="Save Job"><input name="delete_job" type="submit" value="Delete Job"><?php if ($jobs[0]->paused==1){?><input name="unpause_job" type="submit" value="Unpause Job"><?php } else{?><input name="pause_job" type="submit" value="Pause Job"><?php }?>
</div>
</td></tr>
</table></td></tr>
</table>
</form>
				  <?php }?>

<!--********************************
Make A Video End
*********************************-->
</td>

				     <?php }?>
</td></tr></table>
</td>
<td valign="top">

<!--********************************
Schedule Database/File Storage Job Begin
*********************************-->

  <?php if ($view_maintenance_taskbar){ ?>
<form action="view_processing_job.php?<?php echo $query_parameters?>" method="post">
<?php //var_dump($jobs[0]);
?>
					<?php if (!$hide_entire_region_job){?>
<table align="center" border="0" cellpadding="0" cellspacing="1" bgcolor="#000000"><tr><td>
<table border="0" cellpadding="4" cellspacing="0" width="100%">
<tr <?php echo $table_header_color?> ><td colspan=2><b>Schedule a Job For an Entire Region</b></td></tr>
  <tr><td bgcolor="<?php echo  $table_colors[0][0] ?>" >Submission time</td>
	  <td bgcolor="<?php echo  $table_colors[0][1] ?>"><?php echo format_time($jobs[0]->time_submitted)?></td>
  </tr>  
  <tr>
    <td bgcolor="<?php echo  $table_colors[1][0] ?>" >Priority</td>
	  <td bgcolor="<?php echo  $table_colors[1][1] ?>">
		<select name="urgent" size="1">
	   	  name="urgent" size="1">
	   	 	<option value="1" <?php if ($jobs[0]->urgent==1) echo "selected"?> >Urgent</option>
	   	 	<option value="0" <?php if (sizeof($jobs)==0 || $jobs[0]->urgent==0) echo "selected "?> >Standard</option>
<option value="-1" <?php if ($jobs[0]->urgent==-1) echo "selected "?> >Low</option>
<option value="69" <?php if ($jobs[0]->pending_another_jobs_completion) echo "selected "?> >When Ready</option>
	  	</select><br>
	</td>
  </tr>  <!--
	<tr>
		<td bgcolor="<?php echo  $table_colors[0][0] ?>" >Host</td>
		  <td bgcolor="<?php echo  $table_colors[0][1] ?>"><select name="processor_id">
			<?php 

				    for ($i = sizeof($hosts)-1; $i >= 0; $i--){ 
				echo "<option value=\"{$hosts[$i][0]}\"";
				if ($jobs[0]->processor_id == $hosts[$i][0]) echo " selected";
				echo ">" . $hosts[$i][1] . "</option>\n";
				}
			?>
		  </select></td>
	  --></tr><tr><td colspan = 2 bgcolor="<?php echo $table_colors[1][1] ?>">
	 <select name = "maintenance_task" size="1">
					
					<?php $mt = ns_maintenance_task_order($job_type==$IS_REGION,$job_type==$IS_SAMPLE,$job_type==$IS_EXPERIMENT);
					var_dump($mt);
					for ($i = 0; $i < sizeof($mt) ; $i++){?>
					  <option value = "<?php echo $mt[$i];?>" <?php if ($jobs[0]->maintenance_task == $mt[$i]) echo "selected"?> > <?php echo $ns_maintenance_task_labels[$mt[$i]];?> </option>
	<?php } ?>
       </select><br><br>
	<div align="right"><input name="submit" type="submit" value="Save Job"><input name="delete_job" type="submit" value="Delete Job"><?php if ($jobs[0]->paused==1){?><input name="unpause_job" type="submit" value="Unpause Job"><?php } else{?><input name="pause_job" type="submit" value="Pause Job"><?php }?></div>
					</td></tr>
</table>
</td></tr>
</table>

<br>
<?php
  }
 	if ($live_dangerously){	  ?>
<table align="center" border="0" cellpadding="0" cellspacing="1" bgcolor="#000000" width="50%"><tr><td>
<table border="0" cellpadding="4" cellspacing="0" width="100%">
<tr <?php echo $table_header_color?> ><td colspan=2><b>Update capture schedule</b></td></tr>
				  <tr><td width = "400" bgcolor="<?php echo $table_colors[1][0] ?>">Cancel any scans pending for this experiment.</td><td bgcolor="<?php echo $table_colors[1][1] ?>" width="1%">
			       
				  <input name="cancel_captures" type="submit" value="Cancel Scheduled Captures" onClick="javascript:return confirm('Are you sure you want to cancel all pending scans?')"></td></tr><tr><td colspan = 1 bgcolor="<?php echo $table_colors[0][0] ?>">In certain circumstances, image servers may fail to correctly transfer images cached locally to long term storage.  This option instructs the server to retry such transfers.</td><td colspan = 1 bgcolor="<?php echo $table_colors[0][1] ?>"><input name="retry_transfer_to_long_term_storage" type="submit" value="Retry transfer of cached images" width = "1%"></td></tr>
				      
</table>
</td></tr>
</table>
<?php }?>
</form>
					<?php } ?>


<!--********************************
Set Control Strain Info
*********************************-->

  <?php if ($view_control_strain_taskbar){ ?>
<form action="view_processing_job.php?<?php echo $query_parameters?>" method="post">
<?php //var_dump($jobs[0]);
?>
<table align="center" border="0" cellpadding="0" cellspacing="1" bgcolor="#000000"><tr><td>
<table border="0" cellpadding="4" cellspacing="0" width="100%">
<tr <?php echo $table_header_color?> ><td colspan=2><b>Identify Control Strain for Device Regression</b></td></tr>
<tr><td colspan = 2 bgcolor="<?php echo $table_colors[1][1] ?>">
	 <select name = "control_region_id" size="1">
<?php
    echo "<option value=\"none\"";
  if ($experiment_control_strain=="")
    echo " selected";
  echo ">No Control</option>\n";
    foreach($experiment_strains as $strain => $region_id){
      echo "<option value =\"" . $region_id."\" ";
      if ($experiment_control_strain == $region_id)
	echo "selected";
      echo ">" ;
      echo $strain;
      echo "</option>\n";
    }
  ?>						
    </select><br><br>
	<div align="right"><input name="set_control_strain" type="submit" value="Set Control Strain">
	</div>
	</td></tr>
	</table>
	</td></tr>
	</table>
	<br>
</form>
	<?php } ?>
<!--********************************
Update Region info Begin
*********************************-->

<?php if ($view_boundaries_taskbar){?>
<form action="view_processing_job.php?<?php echo $query_parameters?>" method="post">
<table align="center" border="0" cellpadding="0" cellspacing="1" bgcolor="#000000"><tr><td>
<table border="0" cellpadding="4" cellspacing="0" width="100%">

<tr <?php echo $table_header_color ?> ><td colspan=3><b>Update Region Information</td></tr>
<tr><td bgcolor="<?php echo  $table_colors[0][0] ?>" colspan=3><font size="-1">Only "Time at which animals had 0 age" is needed for analysis</font></td></tr>
<tr>
		<td bgcolor="<?php echo  $table_colors[1][0] ?>" >Strain</td>
		  <td bgcolor="<?php echo  $table_colors[1][1] ?>"><input type="checkbox" name="set_strain" value="1"></td>
		  <td bgcolor="<?php echo  $table_colors[1][1] ?>"><?php output_editable_field("strain_spec",$region_strain[$jobs[0]->region_id],TRUE)?></td>
	  </tr>
<tr>
		<td bgcolor="<?php echo  $table_colors[0][0] ?>" >Time at which animals had 0 Age</td> <td bgcolor="<?php echo  $table_colors[0][1] ?>" ><input type="checkbox" name="set_start_time" value="1"></td>
		  <td bgcolor="<?php echo  $table_colors[0][1] ?>"><?php
					ns_expand_unix_timestamp($region_start_times[$jobs[0]->region_id],$i,$h,$d,$m,$y);
					output_editable_field("start_hour",$h,TRUE,2);
					echo ":";
					output_editable_field("start_minute",$i,TRUE,2);
					echo "<br>";
					output_editable_field("start_month",$m,TRUE,2);
					echo "/";
					output_editable_field("start_day",$d,TRUE,2);
					echo "/";
					output_editable_field("start_year",$y,TRUE,4);				       
?>
</td>
	  </tr>
	<tr>
		<td bgcolor="<?php echo  $table_colors[1][0] ?>" >Culturing Temperature</td>
		  <td bgcolor="<?php echo  $table_colors[1][1] ?>" ><input type="checkbox" name="set_culturing_temperature" value="1"></td>
		  <td bgcolor="<?php echo  $table_colors[1][1] ?>"><?php output_editable_field("culturing_temperature",$region_culturing_temperature[$jobs[0]->region_id],TRUE)?></td>
	  </tr>	<tr>
		<td bgcolor="<?php echo  $table_colors[0][0] ?>" >Experiment Temperature</td>
		  <td bgcolor="<?php echo  $table_colors[0][1] ?>" ><input type="checkbox" name="set_experiment_temperature" value="1"></td>
		  <td bgcolor="<?php echo  $table_colors[0][1] ?>"><?php output_editable_field("experiment_temperature",$region_experiment_temperature[$jobs[0]->region_id],TRUE)?></td>
	  </tr>	<tr>
		<td bgcolor="<?php echo  $table_colors[1][0] ?>" >Food Source</td>
		  <td bgcolor="<?php echo  $table_colors[1][1] ?>" ><input type="checkbox" name="set_food_source" value="1"></td>
		  <td bgcolor="<?php echo  $table_colors[1][1] ?>"><?php output_editable_field("food_source",$region_food_source[$jobs[0]->region_id],TRUE)?></td>
	  </tr><tr>
		<td bgcolor="<?php echo  $table_colors[0][0] ?>" >Experiment Conditions</td>
		  <td bgcolor="<?php echo  $table_colors[0][1] ?>" ><input type="checkbox" name="set_environment_conditions" value="1"></td>
		  <td bgcolor="<?php echo  $table_colors[0][1] ?>"><?php output_editable_field("environment_conditions",$region_environment_conditions[$jobs[0]->region_id],TRUE)?></td>
	  </tr>
	<tr>
		<td bgcolor="<?php echo  $table_colors[1][0] ?>" >Strain Condition 1</td>
		  <td bgcolor="<?php echo  $table_colors[1][1] ?>" ><input type="checkbox" name="set_strain_condition_1" value="1"></td>
		  <td bgcolor="<?php echo  $table_colors[1][1] ?>"><?php output_editable_field("strain_condition_1_spec",$region_strain_condition_1[$jobs[0]->region_id],TRUE)?></td>
	  </tr>
	<tr>
		<td bgcolor="<?php echo  $table_colors[0][0] ?>" >Strain Condition 2</td>
		  <td bgcolor="<?php echo  $table_colors[0][1] ?>" ><input type="checkbox" name="set_strain_condition_2" value="1"></td>
		  <td bgcolor="<?php echo  $table_colors[0][1] ?>"><?php output_editable_field("strain_condition_2_spec",$region_strain_condition_2[$jobs[0]->region_id],TRUE)?></td>
	  </tr>
	<tr>
		<td bgcolor="<?php echo  $table_colors[1][0] ?>" >Strain Condition 3</td>
		  <td bgcolor="<?php echo  $table_colors[1][1] ?>" ><input type="checkbox" name="set_strain_condition_3" value="1"></td>
		  <td bgcolor="<?php echo  $table_colors[1][1] ?>"><?php output_editable_field("strain_condition_3_spec",$region_strain_condition_3[$jobs[0]->region_id],TRUE)?></td>
	  </tr>

	<tr>
    <td bgcolor="<?php echo  $table_colors[0][0] ?>" >Final Time Point (Date)</td> <td bgcolor="<?php echo  $table_colors[0][1] ?>" ><input type="checkbox" name="set_end_time_date" value="1"></td>
		  <td bgcolor="<?php echo  $table_colors[0][1] ?>"><?php
					ns_expand_unix_timestamp($region_end_times[$jobs[0]->region_id],$i,$h,$d,$m,$y);
					output_editable_field("end_hour",$h,TRUE,2);
					echo ":";
					output_editable_field("end_minute",$i,TRUE,2);
					echo "<br>";
					output_editable_field("end_month",$m,TRUE,2);
					echo "/";
					output_editable_field("end_day",$d,TRUE,2);
					echo "/";
					output_editable_field("end_year",$y,TRUE,4);				       
?><br><font size="-2">(Leave blank to include all images in analysis)</font>
</td></tr>
	<tr>
    <td bgcolor="<?php echo  $table_colors[1][0] ?>" >Final Time Point (Age)</td> <td bgcolor="<?php echo  $table_colors[1][1] ?>" ><input type="checkbox" name="set_end_time_age" value="1"></td>
		  <td bgcolor="<?php echo  $table_colors[1][1] ?>">
<?php
    $end_age = ($region_end_times[$jobs[0]->region_id] - $region_start_times[$jobs[0]->region_id])/(60*60*24);
						if($end_age < 0)
						  $end_age = '';
    
					output_editable_field("end_age",$end_age,TRUE,2)				       
?> days<br><font size="-2">(Leave blank to include all images in analysis)</font>
</td></tr>
<!--
	<tr>
    <td bgcolor="<?php echo  $table_colors[0][1] ?>" >&nbsp;</td> <td bgcolor="<?php echo  $table_colors[0][1] ?>" ></td>
  <td bgcolor="<?php echo  $table_colors[0][1] ?>"></td>
</td></tr>
-->


	<tr><td colspan = 3 bgcolor="<?php echo $table_colors[1][1] ?>">
      
	<div align="right"><input name="update_region_information" type="submit" value="Update Selected Fields"></div><br>
</td></tr>

 <tr><td colspan = 3 bgcolor="<?php echo $table_colors[1][0] ?>">
<div align="right">
<input name="exclude" type="submit" value="Exclude Regions"><input name="unexclude" type="submit" value="Un-Exclude Regions"></div>
<div align="right">
<input name="censor" type="submit" value="Censor Regions"><input name="uncensor" type="submit" value="Un-Censor Regions"></div>
<br>
<div align="right"><input name="censor_images_after_last_valid_sample" type="submit" value="Censor Images Captured after final time point"></div>
<div align="right"><input name="censor_none" type="submit" value="Un-censor all images"></div>

					</td></tr>
</table>
</td></td>
</table>
</form>
  <br>
   				
<!--********************************
Update Region info
*********************************-->

					<?php }?>
<!--*********************************
Delete Images Begin
***********************************-->

<?php if ($view_delete_images_taskbar){
?>
<form action="view_processing_job.php?<?php echo $query_parameters?>" method="post">
<table align="center" border="0" cellpadding="0" cellspacing="1" bgcolor="#000000"><tr><td>
<table border="0" cellpadding="4" cellspacing="0" width="100%">
<tr <?php echo $table_header_color?> ><td colspan=2><b>Delete Images</a></td></tr>
  <tr><td bgcolor="<?php echo  $table_colors[0][0] ?>" >Delete Files from Disk?</td>
	  <td bgcolor="<?php echo  $table_colors[0][1] ?>"><input name="delete_files" type="checkbox" value="1" checked></td>
  </tr>
    <?php if ($job_type==$IS_SAMPLE && $live_dangerously){?>
<tr><td bgcolor="<?php echo  $table_colors[1][0] ?>">Delete all Captured Images?</td>
<td bgcolor="<?php echo  $table_colors[1][1] ?>">
<input name="delete_sample_images" type="submit" value="Delete Sample Data" onClick="javascript:return confirm('Are you sure you wish to clear all raw and processed data in this sample?')"></td></tr>
   
<?php }if ($job_type == $IS_REGION && $live_dangerously){?>
<tr><td bgcolor="<?php echo  $table_colors[0][0] ?>">Delete all Masked Images?</td>
<td bgcolor="<?php echo  $table_colors[0][1] ?>">
<input name="clear_region" type="submit" value="Delete Region Data" onClick="javascript:return confirm('Are you sure you wish to clear all processed images in this sample?')"></td></tr>
   
<?php }?>
</table>
	 <table border="0" cellpadding="4" cellspacing="0" width="100%">
	<tr <?php echo $table_header_color?>><td colspan=3>Processing Tasks</td></tr>
	      
	<?php 
    $cc = 0; 
$finish = $NS_LAST_PROCESSING_JOB;
				  
				     if ($job_type == $IS_REGION || $region_id==="all"){
					if ( $live_dangerously)$start = 0;
					 else $start = 2;
					}
				     else $finish = $ns_processing_tasks['ns_process_resized_sample_image'];

				     for ($i = (int)$start; $i <= (int)$finish ; $i++){
	if ($job_type == $IS_SAMPLE){
		if ($i != $ns_processing_tasks['ns_process_unprocessed'] &&
		    $i !=$ns_processing_tasks['ns_process_resized_sample_image'])
		continue;
	}
else{
  /*	if ($i == $ns_processing_tasks['ns_process_analyze_mask'] ||  $i==$ns_processing_tasks['ns_process_apply_mask'] || $i==$ns_processing_tasks['ns_process_resized_sample_image'] || $i==$ns_processing_tasks['ns_process_region_vis'] || $i==$ns_processing_tasks['ns_process_region_interpolation_vis'] || $i==$ns_processing_tasks['ns_process_interpolation_vis'] || $i==$ns_processing_tasks['ns_process_movement_coloring']) continue;*/
  if ($jobs[0]->operations[$i]== 0 && array_search($i,$region_processing_jobs_to_display) === FALSE)
    continue;

}
  $cc++;
      $clrs = $table_colors[($cc+1)%2];
    ?>
  		<tr>
		<td bgcolor="<?php echo  $clrs[1] ?>" ><input name="clear_<?php echo $i?>" type="checkbox" value="1" <?php if ($jobs[0]->operations[$i] != 0) echo "checked"?>></td>
		<td bgcolor="<?php echo  $clrs[0] ?>" ><?php echo $ns_processing_task_labels[(int)$i]?></td>
	    <td bgcolor="<?php echo  $clrs[0] ?>" >

</td>
		</tr>
	<?php
	}
	?>
 <tr><TD  bgcolor="<?php echo  $table_colors[0][0] ?>" colspan=3><div align="right">
Options: <select name = "maintenance_flag" size="1" STYLE="font-size:8pt">
					
					<?php for ($i = (int)0; $i <= $NS_LAST_MAINTENANCE_FLAG ; $i++){
						if ($i == $ns_maintenance_flags['ns_delete_entire_sample_region'])
continue;			  ?>
 <option value = "<?php echo $i?>" <?php if ($jobs[0]->maintenance_flag == $i) echo "selected"?> ><?php
$v = $ns_maintenance_flag_labels[$i];  if ($v =='')$v = "(All Images)"; echo $v?> </option>
	<?php } ?>
       </select></div></TD></tr>
	<tr>
	<td bgcolor="<?php echo  $table_colors[0][1] ?>" colspan=3 ><?php if (
!$hide_image_delete_button){?><div align="right"><input name="delete_images" type="submit" value="Delete Images" onClick="javascript:return confirm('Are you sure you wish to clear all cached output?')">
</div>
<?php }?>
</td></tr>
</table></td></tr>
</table>
</form>

<!--********************************
Delete images end
*********************************-->

<?php } ?>
</td>
</tr></table><br>
<div align="right">
<a href = "view_processing_job.php?<?php echo $query_parameters?>&live_dangerously=1" onClick="javascript:return confirm('Dangerous commands can permanantly delete experimental data.  Continue?')">[Enable Dangerous Commands]</a></div>

<?php
display_worm_page_footer();
?>
