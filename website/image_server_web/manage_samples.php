<?php
require_once('worm_environment.php');
require_once('ns_dir.php');
require_once('ns_experiment.php');
require_once('ns_processing_job.php');

$experiment_id=@$query_string['experiment_id'];
$sample_id=@$query_string['sample_id'];
$save_data=@$query_string['save_data'];
$region_id=@$query_string['region_id'];
$sample_action = @$query_string['sample_action'];
$region_action=@$query_string['region_action'];
$experiment_action=@$query_string['experiment_action'];
$hide_sample_jobs=@$query_string['hide_sample_jobs'];
$hide_region_jobs=@$query_string['hide_region_jobs'];
$hide_censored=@$query_string['hide_censored']=== '1';

$edit_region_id=@$query_string['edit_region_id'];
$delete_censored_jobs = @$query_string['delete_censored_jobs'];
$delete_problem_images = @$query_string['delete_problem_images'];
$delete_experiment_jobs = @$query_string['delete_experiment_jobs'];
$pause_jobs_set = ns_param_spec($query_string,'pause_jobs');
$pause_jobs = @$query_string['pause_jobs'];

$show_region_jobs=TRUE;
$show_sample_jobs=TRUE;
if ($hide_region_jobs!=0)
  $show_region_jobs = FALSE;
if ($hide_sample_jobs!=0)
  $show_sample_jobs = FALSE;
$region_job_query_string = "&hide_region_jobs=$hide_region_jobs&hide_sample_jobs=$hide_sample_jobs";

if ($pause_jobs_set){
  $v = "0";
  if ($pause_jobs==="1")
    $v = "1";
  $query = "UPDATE processing_jobs SET paused = $v WHERE experiment_id = $experiment_id";
  //  die($query);
  $sql->send_query($query);
  $query = "UPDATE processing_job_queue as q, processing_jobs as p SET q.paused = $v WHERE q.job_id = p.id AND p.experiment_id = $experiment_id";
  $sql->send_query($query);
 }
try{

  /*****************
   Load Experiment, Sample, and Region Information
  ****************/
  if ($experiment_id == '' || $experiment_id == 0)
    throw new ns_exception("Please specify an experiment number");
  if ($delete_problem_images){
    $query = "UPDATE sample_region_images as r, sample_region_image_info as i, capture_samples as s SET ";
    for ($i = $ns_processing_tasks['ns_process_spatial']; $i <= $NS_LAST_PROCESSING_JOB; $i++){
      if ($i == $ns_processing_tasks['ns_process_resized_sample_image'])
	continue;
      $query .= "r.op${i}_image_id=0,";
    }
    $query .= " currently_under_processing=0 WHERE r.problem > 0 AND r.region_info_id = i.id AND i.sample_id = s.id AND s.experiment_id = $experiment_id";
    $sql->send_query($query);

  }
  if ($delete_censored_jobs){
    $query = "SELECT id FROM capture_samples WHERE censored=1 AND experiment_id=$experiment_id";
    $sql->get_row($query,$samples);
    for ($i = 0; $i < sizeof($samples); $i++){
      $query = "DELETE FROM processing_jobs as p, processing_job_queue as q WHERE p.sample_id=" . $samples[$i][0] . "AND q.job_id = p.id";
      //echo $query . "<BR>";
      $sql->send_query($query);

    }
    $query = "SELECT i.id FROM sample_region_image_info as i, capture_samples as s WHERE i.censored=1 AND i.sample_id = s.id AND s.experiment_id = $experiment_id";

    $sql->get_row($query,$regions);

    for ($i = 0; $i < sizeof($regions); $i++){
      $query = "DELETE FROM processing_jobs WHERE region_id=" . $regions[$i][0];
      //echo $query . "<BR>";
      $sql->send_query($query);
    }
    header("Location:manage_samples.php?experiment_id=$experiment_id\n\n");
    die("");
  }
  if ($delete_experiment_jobs){
    $query = "DELETE FROM processing_jobs WHERE experiment_id = $experiment_id AND sample_id=0 AND region_id = 0";
    $sql->send_query($query);
    header("Location: manage_samples.php?experiment_id=$experiment_id\n\n");
    die("");
  }

  $experiment = new ns_experiment($experiment_id,'',$sql,false);
#if ($hide_censored)die("HIDE");
# else die("NO HIDE");
  $experiment->get_sample_information($sql,!$hide_censored);


  /**************************
   Sort Samples, putting censored samples at back of list.
  *************************/

  $uncensored = array();
  $censored = array();
  for ($i=0; $i < sizeof($experiment->samples); $i++){
    if ($experiment->samples[$i]->censored)
      array_push($censored,$experiment->samples[$i]);
    else array_push($uncensored,$experiment->samples[$i]);
  }
  for ($i = 0; $i < sizeof($uncensored); $i++)
    $experiment->samples[$i] = $uncensored[$i];
  for ($i = 0; $i < sizeof($censored); $i++)
    $experiment->samples[sizeof($uncensored)+$i] = $censored[$i];


  if ($sample_id =='')
    $sample_id = ns_param_spec($_POST,'sample_id')?$_POST['sample_id']:0;

  /***************************
   Handle Requests for Regions to Exclude, Censor, or flag busy
  ***************************/
$refresh_page=FALSE;
  if ($region_id != '' && $region_action != ''){
    if ($region_action == "Censor"||$region_action == "Uncensor"){
      $c = "0";
      if ($region_action == "Censor")
	$c = "1";
      $query = "UPDATE sample_region_image_info SET censored = $c WHERE id=$region_id";
      $sql->send_query($query);
      $refresh_page=TRUE;
    }
    else if ($region_action == "Exclude"||$region_action == "UnExclude"){
      $e = "0";
      if ($region_action == "Exclude")
	$e = "1";
      $query = "UPDATE sample_region_image_info SET excluded_from_analysis=$e WHERE id=$region_id";
      $sql->send_query($query);
      $refresh_page = TRUE;
    }
    else{
      $query = "UPDATE sample_region_images SET ";
      $query2= "UPDATE worm_movement, sample_region_images SET ";
      if ($region_action=='remove_problems'){
	$query .= 'problem=0';
	$query2.= 'worm_movement.problem=0';
      }
      else if ($region_action=='remove_busy'){
	$query .='currently_under_processing=0';
	$query2 .='worm_movement.currently_under_processing=0';
      }
      else throw new ns_exception("Unknown region sample image action: $region_action");

      $query .= " WHERE region_info_id=$region_id";
      $query2 .= " WHERE sample_region_images.region_info_id=$region_id AND sample_region_images.worm_movement_id = worm_movement.id";
      $sql->send_query($query);
      echo $query2;
      $sql->send_query($query2);
    }
    $refresh_page = TRUE;
  }

  /*******************************
   Handle Requests for Samples to Exclude Censor, or flag busy
  ****************************/
  if ($sample_id != '' && $sample_action != ''){
    $query = "UPDATE captured_images SET ";
    if ($sample_action=='clear_problem')
      $query .= 'problem=0';
    else if ($sample_action=='clear_busy')
      $query .='currently_being_processed=0';
    else throw new ns_exception("Unknown sample action: $sample_action");
    $query .= " WHERE sample_id=$sample_id";
    //die($query);
    $sql->send_query($query);
    $sample_id = 0;
    $refresh_page = TRUE;
  }
if ($sample_id != 0 && ns_param_spec($_POST,'cancel_pending_scans')){
	$query = "DELETE FROM capture_schedule WHERE sample_id = $sample_id AND time_at_start='0' AND missed='0' AND time_at_start='0' AND scheduled_time > " . ns_current_time();
	$sql->send_query($query);
    header("Location: manage_samples.php?experiment_id=$experiment_id&$region_job_query_string#$sample_id\n\n");
    die("");
}

  if ($sample_id != 0 && ns_param_spec($_POST,'censor_sample')){
    $query = "UPDATE capture_samples SET censored=1 WHERE id=" . $sample_id;
    $sql->send_query($query);
    header("Location: manage_samples.php?experiment_id=$experiment_id&$region_job_query_string#$sample_id\n\n");
    die("");
  }
  if ($sample_id != 0 && ns_param_spec($_POST,'uncensor_sample')){
    $query = "UPDATE capture_samples SET censored=0 WHERE id=" . $sample_id;
    $sql->send_query($query);
    header("Location: manage_samples.php?experiment_id=$experiment_id&$region_job_query_string#$sample_id\n\n");
    die("");
  }

  /******************************
   Hande Request to Change Region Information
  ******************************/

  if ($edit_region_id == '')
    $edit_region_id = ns_param_spec($_POST,'edit_region_id')?$_POST['edit_region_id']:0;

  $new_url = "manage_samples.php?experiment_id=$experiment_id$region_job_query_string";

  if ($region_id != 0){
    unset($set_plate_lf);
    $excluded = true;
    //  var_dump($query_string);
    //  die("")'
    if ($query_string['mark_c3'] == 'short')
      $set_plate_lf = "?short";
    if ($query_string['mark_c3'] == 'mid')
      $set_plate_lf = "?mid";
    if ($query_string['mark_c3'] == 'long')
      $set_plate_lf = "?long";
    if ($query_string['mark_c3'] == 'OK'){
      $set_plate_lf = "";
      $excluded = false;
    }
    if (isset($set_plate_lf)){
      $query = "UPDATE sample_region_image_info SET strain_condition_3 = '$set_plate_lf', excluded_from_analysis=" . ($excluded?"1":"0") . " WHERE id =$region_id";
      $sql->send_query($query);
    }

  }


  if ($edit_region_id != 0 && ns_param_spec($_POST,'save')){
    $region_details = $_POST['region_details'];
    $region_condition_1 = $_POST['region_condition_1'];
    $region_condition_2 = $_POST['region_condition_2'];
    $region_condition_3 = $_POST['region_condition_3'];
    $region_strain = $_POST['region_strain'];
    $environment_condition = $_POST['region_environment_condition'];
    $food_source = $_POST['region_food_source'];
    $culturing_t = $_POST['region_culturing_temperature'];
    $experiment_t = $_POST['region_experiment_temperature'];

    //die("Want to subit region details: " . $region_details . " for region_id " . $edit_region_id);
    $query = "UPDATE sample_region_image_info SET strain='".ns_slash($region_strain)."',details='".ns_slash($region_details)."'"
	. ",strain_condition_1='".ns_slash($region_condition_1)."'"
	. ",strain_condition_2='".ns_slash($region_condition_2)."'"
	. ",strain_condition_3='".ns_slash($region_condition_3)."'"
      . ",environmental_conditions='".ns_slash($environment_condition)."'"
	. ",food_source='".ns_slash($food_source)."'"
	. ",culturing_temperature='".ns_slash($culturing_t)."'"
	. ",experiment_temperature='".ns_slash($experiment_t)."'"
        . " WHERE id=$edit_region_id";


    //die($query);
    $sql->send_query($query);
    $edit_region_id = 0;

  }

  if ($sample_id != 0 && ns_param_spec($_POST,'cancel'))
    $sample_id = 0;


  /*********************************
Handle Requests to delete a sample
  *************************************/

  if ($sample_id !=0 && ns_param_spec($_POST,'delete_sample')){
    ns_delete_sample_from_database($sample_id,$sql);
    /*
    $query = "DELETE FROM capture_schedule WHERE sample_id =" . $sample_id;
    $sql->send_query($query);
    $query = "SELECT id FROM sample_region_image_info WHERE sample_id=" . $sample_id;
    $sql->get_row($query,$region_ids);
    for ($i = 0; $i < sizeof($region_ids); $i++){
      $query = "DELETE FROM sample_region_images WHERE region_info_id = " . $region_ids[$i][0];
      $sql->send_query($query);
    }
    $query = "DELETE FROM sample_region_image_info WHERE sample_id=" .$sample_id;
    $sql->send_query($query);
    $query = "DELETE FROM captured_images WHERE sample_id=" .$sample_id;
    $sql->send_query($query);
    $query = "DELETE FROM sample_time_relationships WHERE sample_id=" . $sample_id;
    $sql->send_query($query);
    $query = "DELETE FROM worm_movement WHERE sample_id =" . $sample_id;
    $sql->send_query($query);
    $query = "DELETE FROM capture_samples WHERE id=" . $sample_id;
    $sql->send_query($query);*/
    header("Location: manage_samples.php?experiment_id=$experiment_id&$region_job_query_string#$sample_id\n\n");
    die("");
  }

  /*********************************
   Handle Requests to Change Sample Information
  ***********************************/

  if ($sample_id != 0 && ns_param_spec($_POST,'save')){
    $found = false;
    
    for ($i = 0; $i < sizeof($experiment->samples); $i++){
      if ($experiment->samples[$i]->id() == $sample_id){
	$found = true;
	$experiment->samples[$i]->name = ns_slash($_POST['sample_name']);
	$experiment->samples[$i]->description = ns_slash($_POST['sample_description']);
	$experiment->samples[$i]->capture_parameters = ns_slash($_POST['sample_capture_parameters']);

	$mask_new_id = $_POST['mask_id'];
	if ($experiment->samples[$i]->mask_id != $mask_new_id && $mask_new_id != 0){
	  $ex_name = $experiment->name;
	  $s_name = $experiment->samples[$i]->name;
	  $fn = "mask_" . $s_name . ".tif";
	  $fname = $ns_image_server_storage_directory_absolute . $DIR_CHAR . $ex_name . $DIR_CHAR . $fn;
	  $query = "SELECT images.path, images.filename FROM images, image_masks WHERE image_masks.id=$mask_new_id AND images.id=image_masks.image_id";
	  $sql->get_row($query,$res);
	  if (sizeof($res) == 0)
	    throw new ns_exception("Could not load mask image from db");
	  $oname = $ns_image_server_storage_directory_absolute . $DIR_CHAR . $res[0][0] . $DIR_CHAR . $res[0][1];
	  //die ($fname . "->" . $oname);
	  if (!rename($oname,$fname))
	    throw new ns_experiment("Could not move mask to experiment directory");
	  $query = "UPDATE images, image_masks SET images.path='" . $experiment->name . "', images.filename='$fn' WHERE image_masks.id=$mask_new_id AND images.id = image_masks.image_id";
	  //die($query);
	  $sql->send_query($query);
	}
	$experiment->samples[$i]->mask_id = ns_slash($_POST['mask_id']);

	$experiment->samples[$i]->model_filename = ns_slash($_POST['sample_model_filename']);
	//die ($experiment->samples[$i]->model_filename);

	if ($experiment->samples[$i]->name == '')
	  throw new ns_exception("You must specify a sample name.");
	if ($experiment->samples[$i]->capture_parameters == '')
	  throw new ns_exception("You must specify capture parameters for your sample.");
	$experiment->samples[$i]->save($sql);
      }

    }
    if (!$found)
      throw ns_exception("The sample you modified no longer exists in the database!");
    $sample_id = 0;

  }
  /*******************************
   Load Mask information for samples
  ********************************/
  $query = "SELECT image_masks.id, images.id, images.filename FROM image_masks, images WHERE image_masks.processed = 0 AND image_masks.image_id = images.id ORDER BY images.filename ";
  $sql->get_row($query, $masks);
  $masks[sizeof($masks)] = array(0,0,"(none)");

  /**********************************
   Load Experiment-wide Jobs
  **********************************/
  $job = new ns_processing_job;
  $query = $job->provide_query_stub();
  $query .= "FROM processing_jobs WHERE processing_jobs.experiment_id = $experiment_id AND sample_id = 0 AND region_id = 0";
  $sql->get_row($query,$ejobs);
  $experiment_jobs = array();
  for ($i = 0; $i < sizeof($ejobs); $i++){
    $experiment_jobs[$i] = new ns_processing_job;
    $experiment_jobs[$i]->load_from_result($ejobs[$i]);
  }


  /*****************************
   Load Sample Jobs
  ****************************/
  $sample_jobs = array();
  if ($show_sample_jobs){
    for ($i = 0; $i < sizeof($experiment->samples); $i++){
      $query = $job->provide_query_stub();
      $query .= "FROM processing_jobs WHERE experiment_id = $experiment_id AND sample_id = '" . $experiment->samples[$i]->id() ."' AND region_id = 0 AND image_id = 0";
      $sjobs = array();
      $sql->get_row($query,$sjobs);
      $sample_jobs[$i] = array();
      for($j = 0; $j < sizeof($sjobs); $j++){
	$sample_jobs[$i][$j] = new ns_processing_job;
	$sample_jobs[$i][$j]->load_from_result($sjobs[$j]);
      }
    }
  }
  //var_dump($sample_jobs);
  /***************************
   Load Region Jobs
  ****************************/
  $strains = array();
  $regions = array();
  $region_jobs = array();
  $devices_used = array();
  $all_animal_type_values = array();
  $all_animal_type_values=array();
 $environment_condition_values=array();
$strains = array();
$condition_1_values=array();
$condition_2_values=array();
$condition_3_values=array();
$culturing_temperature_values=array();
$experiment_temperature_values=array();

$food_source_values=array();

for ($i = 0; $i < sizeof($experiment->samples); $i++){
    $devices_used[$experiment->samples[$i]->device_name()] = $experiment->samples[$i]->device_name();
}

$query = "SELECT r.id, r.name, r.details, r.censored,r.strain,r.excluded_from_analysis, r.reason_censored,r.strain_condition_1,r.strain_condition_2,r.strain_condition_3,r.culturing_temperature,r.experiment_temperature,r.food_source,r.environmental_conditions,r.censored, r.op22_image_id,s.id FROM sample_region_image_info as r, capture_samples as s WHERE r.sample_id= s.id AND s.experiment_id = " . $experiment_id;
if ($hide_censored)
  $query.=" AND r.censored = 0 ";
$query .= " ORDER BY s.id,r.name";
$sql->get_row($query,$all_results);
$all_region_job_results = array();
if ($show_region_jobs){
  $query = $job->provide_query_stub();
  $query .= "FROM processing_jobs, sample_region_image_info as r, capture_samples as s WHERE processing_jobs.image_id = 0 AND processing_jobs.region_id = r.id AND r.sample_id = s.id AND s.experiment_id = " . $experiment_id . " ORDER BY s.id,r.name";
  //	die($query);
  $sjobs = array();
  $sql->get_row($query,$all_region_job_results);
}
$current_region_job_index = 0;
    $start_range = 0;
    if (sizeof($all_results) > 0)
        $cur_sample_id = $all_results[0][16];
    for ($i = 0; $i < sizeof($all_results); $i++){
      #echo $cur_sample_id . ": ";
      if ($all_results[$i][16] != $cur_sample_id || $i+1 == sizeof($all_results)){
	$rrr =&$regions[ $cur_sample_id ];
	$stop = $i-1;
	if ($i+1 == sizeof($all_results))
	  $stop = $i;
	$rrr = array_slice($all_results,$start_range,$stop-$start_range+1);
	$cur_sample_id = $all_results[$i][16];
	$start_range = $i;
	//get jobs that apply to individual samples
	for ($k = 0; $k < sizeof( $rrr ); $k++){
	  $cur_region =& $rrr[$k];
	  $censored = $cur_region[14]!="0";
	  if (!$censored){
	    $strains[strtoupper($cur_region[4])] = $cur_region[4];
	    $condition_1_values[strtolower($cur_region[7])] = $cur_region[7];
	    $condition_2_values[strtolower($cur_region[8])] = $cur_region[8];
	    $condition_3_values[strtolower($cur_region[9])] = $cur_region[9];
	    $culturing_temperature_values[strtolower($cur_region[10])] = $cur_region[10];
	    $experiment_temperature_values[strtolower($cur_region[11])] = $cur_region[11];
	    $food_source_values[strtolower($cur_region[12])] = $cur_region[12];
	    $environment_condition_values[strtolower($cur_region[13])] = $cur_region[13];

	    $cstr = strtolower($cur_region[4]);
	    if($cur_region[7] != '')
	      $cstr .= "::" . strtolower($cur_region[7]);
	    if($cur_region[8] != '')
	      $cstr .= "::" . strtolower($cur_region[8]);

	    if($cur_region[9] != '')
	      $cstr .= "::" . strtolower($cur_region[9]);
	    if($cur_region[10] != '')
	      $cstr .= "::" . strtolower($cur_region[10]);
	    //if($cur_region[11] != '')
	    //  $cstr .= "::" . strtolower($cur_region[11]);
	    if($cur_region[12] != '')
	      $cstr .= "::" . strtolower($cur_region[12]);
	    if($cur_region[13] != '')
	      $cstr .= "::" . strtolower($cur_region[13]);

	    $all_animal_type_values[$cstr] =
	      array($cur_region[4],
		    $cur_region[7],
		    $cur_region[8],$cur_region[9],$cur_region[10],$cur_region[11],$cur_region[12],$cur_region[13],);
	  }

	  $region_jobs[$cur_region[0]] = array();
	  if ($show_region_jobs){
	    $jid=0;
	    #print($all_region_job_results[$current_region_job_index][3] ." ". $cur_region[0] . ";");
	    if (sizeof($all_region_job_results)>0)
	    while(array_key_exists($current_region_job_index,$all_region_job_results) &&
	          $all_region_job_results[$current_region_job_index][3] == $cur_region[0]){
	      //print($current_region_job_index  "<BR>");
	      
	      
	      $region_jobs[$cur_region[0]][$jid] = new ns_processing_job;
	      $region_jobs[$cur_region[0]][$jid]->load_from_result($all_region_job_results[$current_region_job_index]);
	      
	      $current_region_job_index++;
	      $jid++;
	    }
	  }
	}
      }
    }
    //var_dump($region_jobs);
  ksort($all_animal_type_values);
  ksort($environment_condition_values);
  ksort($strains);
  ksort($condition_1_values);
  ksort($condition_2_values);
  ksort($condition_3_values);
  ksort($culturing_temperature_values);
  ksort($experiment_temperature_values);
  ksort($food_source_values);
  //  var_dump($regions);

  /*****************************
   Handle Requests for Experiment to Exclude Censor or Flag Busy
  ****************************/

  if ($experiment_action != ''){
    if ($experiment_action == "remove_problems"){
	$change = "problem=0";
	$schange = "problem=0";
    }
    if ($experiment_action == "remove_busy"){
	$change = "currently_under_processing=0";
	$schange = "currently_being_processed=0";
    }
    //echo "TRYING";
    for ($i = 0; $i < sizeof($experiment->samples); $i++){
      $query = "UPDATE captured_images SET $schange WHERE sample_id=" . $experiment->samples[$i]->id();
	$sql->send_query($query);
	//die($query);
      for ($j = 0; $j < sizeof($regions[$experiment->samples[$i]->id()]); $j++){
	$query = "UPDATE sample_region_images SET $change WHERE region_info_id=" . $regions[$experiment->samples[$i]->id()][$j][0];
	//echo $query . "<br>";
	$sql->send_query($query);
	$query = "UPDATE worm_movement SET $change WHERE region_info_id = " . $regions[$experiment->samples[$i]->id()][$j][0];
	$sql->send_query($query);

	if ($experiment_action == "remove_problems"){
	  $query = "UPDATE worm_movement as m, sample_region_images as s SET m.calculated=0 WHERE m.region_info_id=".$regions[$experiment->samples[$i]->id()][$j][0]." AND s.id = m.region_id_short_1 AND s.op" . $ns_processing_tasks["ns_process_movement_coloring"] . "_image_id = 0";
	  $sql->send_query($query);

	}

      }
    }
    ;$refresh_page = TRUE;
  }

  if ($save_data != 0){
    $sample_id = 0;
  }

  $strain_info = array();
  foreach ($strains as $s){
    $query = "SELECT genotype,conditions FROM strain_aliases WHERE strain='$s'";
    $sql->get_row($query,$r);
    if( sizeof($r)>0)
      $strain_info[$s]=$r[0];
  }
  $query = "SELECT latest_storyboard_build_timestamp, last_timepoint_in_latest_storyboard_build FROM experiments WHERE id = " . $experiment_id;
  $sql->get_row($query,$res);
  if (sizeof($res) == 0)
    throw new ns_exception("Could not find experiment");
  //	var_dump($res);
  //die($res);
  $storyboard_build_timestamp = $res[0][0];
  $storyboard_last_timepoint = $res[0][1];
  $query = "SELECT max(i.latest_movement_rebuild_timestamp) FROM sample_region_image_info as i, capture_samples as s WHERE i.sample_id = s.id AND s.experiment_id = " . $experiment_id;
  $sql->get_row($query, $res);
  //	var_dump($res);
  //	die("");
  $latest_rebuild = $res[0][0]
    ;
  if ($storyboard_build_timestamp == 0){
    $storyboard_info = "No storyboard built";
  }
  else {
    $storyboard_info = "<table><tr><td>Storyboard built on </td><td>" . format_time($storyboard_build_timestamp) . "</td></tr>";
    if ($latest_rebuild > $storyboard_build_timestamp)
      $storyboard_info.="<tr><td></td><td><b>(Out of date!)</b></td></tr>";
    $storyboard_info .="<tr><td>Up to time </td><td>" . format_time($storyboard_last_timepoint) . "</td></tr>";

    $storyboard_info .= "</table>";
    //	  $storyboard_info .= $latest_rebuild;
  }










  /*******************/
  //load plate storyboard information
  $query = "SELECT region_id FROM animal_storyboard WHERE experiment_id = 0";
  $sql->get_row($query,$story_res);

  $storyboard_exists_for_region = array();
  foreach($regions as $id=>&$val){
    for($i=0; $i < sizeof($val); $i++)
    $storyboard_exists_for_region[$val[$i][0]] = false;
  }
  for ($i = 0; $i < sizeof($story_res);$i++){
    //print((int)$story_res[$i][0] . " ");
    if (array_key_exists((int)$story_res[$i][0],$storyboard_exists_for_region)){
      $storyboard_exists_for_region[(int)$story_res[$i][0]] = true;
      //print($story_res[$i][0] . " ");
    }
  }
  //  var_dump($storyboard_exists_for_region);
}
catch (ns_exception $e){
  die($e->text);
}
if ($refresh_page === TRUE){
  header("Location: manage_samples.php?experiment_id=$experiment_id&$region_job_query_string\n\n");
  die("");
 }
display_worm_page_header($experiment->name . " samples");
?>
<span class="style1">Experiment-wide operations</span>
<table border="0" cellpadding="0" cellspacing="1" bgcolor="#000000"><tr><td>
<table border="0" cellpadding="0" cellspacing="0">
  <tr>
<td bgcolor="<?php echo  $table_colors[0][0] ?>" valign="top">

<b>Configuration</b><br>
<?php
//echo "<table border=0 cellpadding=4 cellspacing=0><tr><td valign=\"top\" bgcolor=\"{$table_colors[0][0]}\">";
echo "<a href=\"manage_experiment_analysis_configuration.php?experiment_id=$experiment_id\">[Configure Machine Analysis]</a><br>";
?>

<b>Misc. Experiment Info</b><br>
<?php
echo "<a href=\"ns_view_sample_images.php?experiment_id=$experiment_id\">[View All Captured Images]</a><br>";
echo "<a href=\"view_hosts_log.php?experiment_id=$experiment_id\">[View Sample Problems]</a><br>";
echo "<a href=\"view_hosts_log.php?experiment_id=$experiment_id&region_id=all\">[View Region Problems]</a><br>";
echo "<a href=\"strain_aliases.php?experiment_id=$experiment_id\">[Edit Strain Information]</a>";
?><font size="-1"><?php echo "<br><br>" . $storyboard_info?></font><?php
echo "</td><td valign=\"top\" bgcolor=\"{$table_colors[1][0]}\">";
echo "<b>Experiment Jobs</b><br>";
echo "<a href=\"view_processing_job.php?job_id=0&experiment_id= $experiment_id\">[New Experiment Job]</a><br>";
echo "<a href=\"manage_samples.php?&experiment_id=$experiment_id&experiment_action=remove_problems&$region_job_query_string\"><font size=\"-1\">[Clear Problem Flags]</font></a>";
echo "<a href=\"manage_samples.php?&experiment_id=$experiment_id&experiment_action=remove_busy&$region_job_query_string\"><font size=\"-1\">[Clear Busy Flags]</font></a><br>";
echo "<a href=\"manage_samples.php?experiment_id=$experiment_id&delete_experiment_jobs=1\">[Delete All Experiment Jobs]</a><br>";
//echo "<a href=\"manage_samples.php?experiment_id=$experiment_id&delete_censored_jobs=1\">[Delete All Censored Jobs]</a><br>";
echo "<a href=\"manage_samples.php?experiment_id=$experiment_id&delete_problem_images=1\">[Delete problematic processed images]</a><br>";
echo "<br><a href=\"manage_samples.php?experiment_id=$experiment_id&pause_jobs=1\">[Pause]</a> / <a href=\"manage_samples.php?experiment_id=$experiment_id&pause_jobs=0\">[Resume]</a> Image Analysis";
?>
<br><br>
<b>Sample Jobs</b>
<br>
 <!--<a href="view_processing_job.php?job_id=0&experiment_id=<?php echo $experiment_id?>&sample_id=all&all_new=1&include_censored=1">[New Job For All Samples]</a>-->

<a href="view_processing_job.php?job_id=0&experiment_id=<?php echo $experiment_id?>&sample_id=all&all_new=1&include_censored=0">[New Job For all Samples ]</a>
<br>
<a href="view_processing_job.php?job_id=0&experiment_id=<?php echo $experiment_id?>&sample_id=all&include_censored=0">[Edit Jobs for All Samples]</a><br>
<!--<a href="view_processing_job.php?job_id=0&experiment_id=<?php echo $experiment_id?>&sample_id=all&all_new=1&include_censored=2">[New Job For Censored Samples]</a>-->

</td><td bgcolor="<?php echo  $table_colors[0][0] ?>" valign="top">
<b>Region Jobs</b><br>
<a href="view_processing_job.php?job_id=0&experiment_id=<?php echo $experiment_id?>&sample_id=all&region_id=all&all_new=1">[New Job for All Regions]</a><br>
<a href="view_processing_job.php?job_id=0&experiment_id=<?php echo $experiment_id?>&sample_id=all&region_id=all">[Edit Jobs for All Regions]</a><br>
<!--
<a href="view_processing_job.php?job_id=0&experiment_id=<?php echo $experiment_id?>&sample_id=all&region_id=all&include_censored=1">[Edit Jobs for All Regions included censored]</a><br>
<a href="view_processing_job.php?job_id=0&experiment_id=<?php echo $experiment_id?>&sample_id=all&region_id=all&all_new=1&include_censored=1">[New Job for All Regions including Censored]</a><br>
<a href="view_processing_job.php?job_id=0&experiment_id=<?php echo $experiment_id?>&sample_id=all&region_id=all&all_new=1&include_censored=2">[New Job for Censored Regions]</a><br>-->
</td>
<td valign="top" bgcolor="<?php echo  $table_colors[1][0] ?>">
<b>Jobs for Specific Animal Types</b><br>
<?php
	$per_line = 4;
	if (sizeof($strains) > 1){
		echo "New Job for Strain:<br><font size=\"-1\">";
		$k = 0;
		foreach($strains as $s){
			echo "<a href=\"view_processing_job.php?job_id=0&experiment_id= $experiment_id&sample_id=all&region_id=all&all_new=1&strain=".urlencode($s)."\">[$s]</a> ";
			$k++;
			if ($k == $per_line){echo "<br>";$k = 0;}
		}
		if ($k!=$per_line) echo "<br>";
		echo "</font>";
	}
	if (sizeof($culturing_temperature_values) > 1){
		echo "New Job for Culturing Temperature:<br><font size=\"-1\">";
		$k = 0;
		foreach($culturing_temperature_values as $s){
			echo "<a href=\"view_processing_job.php?job_id=0&experiment_id= $experiment_id&sample_id=all&region_id=all&all_new=1&culturing_temperature=".urlencode($s)."\">[$s]</a> ";
			$k++;
			if ($k == $per_line){echo "<br>";$k = 0;}
		}
		echo "</font><br>";
	}

	if (sizeof($experiment_temperature_values) > 1){
		echo "New Job for Experiment Temperature:<br><font size=\"-1\">";
		$k = 0;
		foreach($experiment_temperature_values as $s){
			echo "<a href=\"view_processing_job.php?job_id=0&experiment_id= $experiment_id&sample_id=all&region_id=all&all_new=1&experiment_temperature=".urlencode($s)."\">[$s]</a> ";
			$k++;
			if ($k == $per_line){echo "<br>";$k = 0;}
		}
		echo "</font><br>";
	}

	if (sizeof($food_source_values) > 1){
		echo "New Job for Food Source Values:<br><font size=\"-1\">";
		$k = 0;
		foreach($food_source_values as $s){
			echo "<a href=\"view_processing_job.php?job_id=0&experiment_id= $experiment_id&sample_id=all&region_id=all&all_new=1&food_source=".urlencode($s)."\">[$s]</a> ";
			$k++;
			if ($k == $per_line){echo "<br>";$k = 0;}
		}
		echo "</font><br>";
	}
	if (sizeof($environment_condition_values) > 1){
		echo "New Job for Experiment Condition:<br><font size=\"-1\">";
		$k = 0;
		foreach($environment_condition_values as $s){
			echo "<a href=\"view_processing_job.php?job_id=0&experiment_id= $experiment_id&sample_id=all&region_id=all&all_new=1&environment_conditions=".urlencode($s)."\">[$s]</a> ";
			$k++;
			if ($k == $per_line){echo "<br>";$k = 0;}
		}
		echo "</font><br>";
	}
     if (sizeof($condition_1_values) > 1){
		echo "New Job for Condition 1:<br><font size=\"-1\">";
		$k = 0;
		foreach($condition_1_values as $s){
			echo "<a href=\"view_processing_job.php?job_id=0&experiment_id= $experiment_id&sample_id=all&region_id=all&all_new=1&condition_1=".urlencode($s)."\">[$s]</a> ";
			$k++;
			if ($k == $per_line){echo "<br>";$k = 0;};
		}
		if ($k!=$per_line) echo "<br>";
		echo "</font><br>";
	}
	if (sizeof($condition_2_values) > 1){
		echo "New Job for Condition 2:<br><font size=\"-1\">";
		$k = 0;
		foreach($condition_2_values as $s){
			echo "<a href=\"view_processing_job.php?job_id=0&experiment_id= $experiment_id&sample_id=all&region_id=all&all_new=1&condition_2=".urlencode($s)."\">[$s]</a> ";
			$k++;
			if ($k == $per_line){echo "<br>";$k = 0;}
		}
		echo "</font><br>";
	}
	if (sizeof($condition_3_values) > 1){
		echo "New Job for Condition 3:<br><font size=\"-1\">";
		$k = 0;
		foreach($condition_3_values as $s){
			echo "<a href=\"view_processing_job.php?job_id=0&experiment_id= $experiment_id&sample_id=all&region_id=all&all_new=1&condition_3=".urlencode($s)."\">[$s]</a> ";
			$k++;
			if ($k == $per_line){echo "<br>";$k = 0;}
		}
		echo "</font><br>";
	}
if (sizeof($all_animal_type_values) > 1){
	echo "New Job by Animal Type:<br><font size=\"-1\">";
		$k = 0;
		foreach($all_animal_type_values as $v=>$s){
		  echo "<a href=\"view_processing_job.php?job_id=0&experiment_id= $experiment_id&sample_id=all&region_id=all&all_new=1&strain=".$s[0]."&condition_1=".$s[1]. "&condition_2=".urlencode($s[2])."&condition_3=".urlencode($s[3])."&culturing_temperature=".urlencode($s[4])./*"&experiment_temperature=".urlencode($s[5]).*/"&food_source=".urlencode($s[6])."&environment_conditions=".urlencode($s[7])."\">[$v]</a> ";
			$k++;
			if ($k == $per_line){echo "<br>";$k = 0;}
		}
		echo "</font>";
}
	if (sizeof($devices_used) > 1){
		echo "<br>New Job for Device:<br><font size=\"-1\">";
		$k = 0;
		foreach($devices_used as $s){
			echo "<a href=\"view_processing_job.php?job_id=0&experiment_id= $experiment_id&sample_id=all&region_id=all&all_new=1&device=".urlencode($s)."\">[$s]</a> ";
			$k++;
			if ($k == $per_line*2){echo "<br>";$k = 0;}
		}
		echo "</font><br>";
	}
?>
</td>
</tr>
</table>
</td>
</tr>
</table>

<?php
	if (ob_get_length()){
		@ob_flush();
		@flush();
		@ob_end_flush();
	}
	@ob_start();
?>

<?php if (sizeof($experiment_jobs)>0){?>
<br>
<span class="style1">Experiment Jobs</span>
<table border="0" cellpadding="0" cellspacing="1" bgcolor="#000000" >
  <tr>
    <td>
	    <table border="0" cellpadding="4" cellspacing="0" width="100%">
	    <?php
	    $number_of_columns = 5;
	  $c = 0;
	  $r = 0;
	  for ($i = 0; $i < sizeof($experiment_jobs); $i++){
	    if ($c == 0)
	      echo "<tr>";
	    $clrs = $table_colors[$c%2];
	    echo "<td valign=\"top\" bgcolor=\"$clrs[$r]\">";
	    echo "<a href=\"view_processing_job.php?job_id={$experiment_jobs[$i]->id}$region_job_query_string\"";
	    echo format_time($experiment_jobs[$i]->time_submitted);
	    echo "</a>";
	    echo $experiment_jobs[$i]->get_job_description($sql);
	    echo "\n</td>\n";
	    if ($c == $number_of_columns-1){
	      echo "</tr>";
	      $c = 0;
	      // $r = !$r;
	    }
	    else $c++;
	  }
	  if ($c != 0){
	    for (;$c < $number_of_columns; $c++){

	    $clrs = $table_colors[$c%2];
	    echo "<td bgcolor=\"$clrs[$r]\"></td>";
	    }
	  echo "</tr>";
	  }

?></td>
  </tr>
</table></td></tr></table>

<?php }?>

<span class="style1">Experiment Samples</span>
<?php if ($show_sample_jobs){
?>
<a href="manage_samples.php?experiment_id=<?php echo $experiment_id?>&hide_region_jobs=1&hide_sample_jobs=1">[Hide Jobs]</a>
<?php }else { ?>

<a href="manage_samples.php?experiment_id=<?php echo $experiment_id?>&show_region_jobs=0&show_sample_jobs=0">[Show Jobs]</a>
  <?php }?>
<form action="<?php echo $new_url?>" method="post">
<table border="0" cellpadding="0" cellspacing="1" bgcolor="#000000" width="100%">
  <tr>
    <td>
<table border="0" cellpadding="4" cellspacing="0" width="100%">
  <tr <?php echo $table_header_color?> ><td >Sample Name</td><td>Info</td><td>Parameters</td><td>&nbsp;</td></tr>


 <?php
	//echo "<tr><td bgcolor=\"#FFFFFF\" colspan = 7>fsfdsdf";
	//var_dump($region_jobs);
	//echo "</td></tr>";
	$row_color = 0;
	for ($i = 0; $i < sizeof($experiment->samples); $i++){
		$clrs = $table_colors[$row_color];
		$cur_id = $experiment->samples[$i]->id();
		$edit = ($sample_id == $cur_id);

		echo "<tr><td bgcolor=\"$clrs[1]\" valign=\"top\">\n";
		echo "<a name=\"" . $experiment->samples[$i]->id() . "\">";
		echo "<b>";
		output_editable_field('sample_name', ns_slash($experiment->samples[$i]->name()),$edit,12);
		if ($experiment->samples[$i]->censored){
		  echo "<br>Censored";
		  if ($experiment->samples[$i]->reason_censored !='')
			echo '<br>(' . $experiment->samples[$i]->reason_censored . ')';
		}
		echo "</b>\n</td>";
		echo "<td bgcolor=\"$clrs[1]\" valign=\"top\" nowrap>\n" ;
		echo "Model: ";
		output_editable_field('sample_model_filename',ns_slash($experiment->samples[$i]->model_filename),$edit);
		echo "<BR>";
		if (!$edit){
			echo "<font size=\"-1\">";
			echo "<a href=\"manage_masks.php?mask_id={$experiment->samples[$i]->mask_id}\">[View Sample Mask]</a><br>";
			echo "<a href=\"ns_view_sample_images.php?sample_id=" . $experiment->samples[$i]->id() . "\">[View Sample Images]</a>";
			echo "</font>";
			}
		else{
			echo "<select name=\"mask_id\" size=\"1\">";
			for ($j = 0; $j < sizeof($masks); $j++){
				echo "<option value=\"{$masks[$j][0]}\" ";
				if ($masks[$j][0] == $experiment->samples[$i]->mask_id)
					echo "selected";
				echo ">{$masks[$j][2]}</option>";
			}
			echo "</select>";
		}
		if ($edit || strlen($experiment->samples[$i]->description) > 0)
		  echo "<br>Description:";
		if ($edit) echo "<BR>";
		output_editable_field('sample_description', ns_slash($experiment->samples[$i]->description),$edit,30,TRUE);
		echo "\n</td><td bgcolor=\"$clrs[1]\" valign=\"top\"><font size=\"-1\">";
		echo "On device " . $experiment->samples[$i]->device_name();
		if ($edit) echo "<BR>";
		output_editable_field('sample_capture_parameters',ns_slash($experiment->samples[$i]->capture_parameters),$edit,50,TRUE);
		echo "\n</font></td><td bgcolor=\"$clrs[1]\" valign=\"top\" align=\"right\" nowrap>\n";
		echo "<a href=\"manage_samples.php?experiment_id=$experiment_id&sample_id=".$experiment->samples[$i]->id()."&sample_action=clear_problem&$region_job_query_string#{$experiment->samples[$i]->id()}\"><font size=\"-1\">[Problem]</font></a>\\";
		echo "<a href=\"manage_samples.php?experiment_id=$experiment_id&sample_id=".$experiment->samples[$i]->id()."&sample_action=clear_busy&$region_job_query_string#{$experiment->samples[$i]->id()}\"><font size=\"-1\">[Busy]</font></a><br>";
		global $show_sample_jobs;
		if (!$edit){
		  //echo "<a href=\"$new_url&sample_id=$cur_id\">[Edit]</a>";
			echo "<a href=\"$new_url&sample_id=$cur_id&edit=1&$region_job_query_string#{$experiment->samples[$i]->id()}\">[Edit]</a><br>";
		        echo "<a href=\"view_processing_job.php?job_id=0&experiment_id=$experiment_id&all_new=1&sample_id=".$experiment->samples[$i]->id()."\"><font size=\"-1\">[Create]</font></a>";
			echo "<a href=\"view_processing_job.php?job_id=0&experiment_id=$experiment_id&sample_id=".$experiment->samples[$i]->id()."\"><font size=\"-1\">[Edit]</font></a> <font size=\"-1\">a job for this sample</font></a><br>";

  echo "<a href=\"view_processing_job.php?job_id=0&experiment_id=$experiment_id&sample_id=".$experiment->samples[$i]->id()."&region_id=all&all_new=1\"><font size=\"-1\">[Create]</font>";
		  echo "<a href=\"view_processing_job.php?job_id=0&experiment_id=$experiment_id&sample_id=".$experiment->samples[$i]->id()."&region_id=all\"><font size=\"-1\">[Edit]</font></a><font size=\"-1\"> a Job for all regions in this sample</font><br>"	;
		  //		  echo "<a href=\"view_processing_job.php?job_id=0&experiment_id=$experiment_id&sample_id=".$experiment->samples[$i]->id()."&region_id=all&all_new=1&include_censored=1\"><font size=\"-1\">[Create]</font></a><font size=\"-1\"> a job for all Sample Regions including censored";
		}
		else{
		  echo "<table width=\"100%\" cellspacing=\"10\" cellpadding=\"0\"><tr><td valign=\"top\">";
		echo "<input name=\"sample_id\" type=\"hidden\" value=\"$cur_id\">\n";
		echo "<input name=\"save\" type=\"submit\" value=\"Save\"><br>\n";
		echo "<input name=\"cancel\" type=\"submit\" value=\"Cancel\">\n";
		echo "</td><td>";
		echo "<input name=\"delete_sample\" type=\"submit\" value=\"Delete\" onClick=\"javascript: return confirm('Are you sure you want to delete all data and pending captures from this sample?')\"><br>\n";
		if ($experiment->samples[$i]->censored)
		  echo "<input name=\"uncensor_sample\" type=\"submit\" value=\"Un-Censor\" onClick=\"javascript: return confirm('Are you sure you want to censor this sample?')\"><br>\n";
		else
		  echo "<input name=\"censor_sample\" type=\"submit\" value=\"Censor\" onClick=\"javascript: return confirm('Are you sure you want to censor this sample?')\"><br>\n";
		echo "<input name=\"cancel_pending_scans\" type=\"submit\" value=\"Cancel Pending Scans\" onClick=\"javascript: return confirm('Are you sure you want to cancel all pending scans on this sample?')\"><br>\n";
		echo "</td></tr></table>\n";
		}
		echo "</td></tr>\n";
		echo "<tr><td bgcolor=\"$clrs[1]\" colspan=\"2\">&nbsp;</td><td bgcolor=\"$clrs[1]\" colspan=\"2\">";
		    echo "<table cellspacing=0 cellpadding=0 align=\"left\"><tr valign=\"top\"><td>";
		    if ($show_sample_jobs){
		    $job = new ns_processing_job;
				$job->sample_id = $experiment->samples[$i]->id();
				echo $job->get_processing_state_description(TRUE,$sql);
		    }
		    echo "</td></tr></table>";


		//out job info
		//$row_color = !$row_color;
				global $show_sample_jobs;
		if ($show_sample_jobs){

		echo "<table cellspacing=0 cellpadding=0 align = \"right\"><tr><td valign=\"top\">";
		  for ($j = 0; $j < sizeof($sample_jobs[$i]); $j++){
		    $clrs = $table_colors[$row_color];

		    echo $sample_jobs[$i][$j]->get_job_description($sql);
		    //$row_color = !$row_color;
		  }
		echo "</tr></td></table>";

		}
		echo "</td></tr>\n";
		$row_color = !$row_color;
		$cur_sample_id = $experiment->samples[$i]->id();
		if (!array_key_exists($cur_sample_id,$regions)){
		         echo "<tr>";
                        echo "<td bgcolor=\"$clrs[0]\" nowrap valign=\"top\" colspan = 4>";
			echo "No Regions identified in this sample";
			echo "</td></tr>";
		}else 
		for ($k = 0; $k < sizeof($regions[$cur_sample_id]); $k++){

			$clrs = $table_colors[$row_color];

			$cur_region =& $regions[$cur_sample_id][$k];
			$cur_region_id = $cur_region[0];
			if ($cur_region[3] == "0")
			  $censor = "Censor";
			else $censor = "Uncensor";
			if ($cur_region[5] == "0")
			  $exclude = "Exclude";
			else $exclude = "UnExclude";
			echo "<tr>";
			echo "<td bgcolor=\"$clrs[0]\" nowrap valign=\"top\"><b>";
			//	echo "$cur_region_id :: $k . <BR>";
			echo $experiment->samples[$i]->name() . "::{$cur_region[1]}";
			echo "</b>";

			$b = '<BR>';
			global $show_region_jobs;
			if (!$show_region_jobs)
			  $b = '';
			echo  "<br><font size=\"-2\"><a href=\"ns_view_region_images.php?region_id={$cur_region[0]}&experiment_id={$experiment->id()}\">[View Region Images]</a></font><br>";
			//	echo "<a href=\"view_movement_data.php?region_id={$cur_region[0]}\"><font size=\"-2\">[View Movement]</font></a>$b";
			echo "<a href=\"view_hosts_log.php?region_id={$cur_region[0]}\"><font size=\"-2\">[View Problems]</font></a>$b";
			echo "</td>\n";
			echo "<td bgcolor=\"$clrs[0]\" colspan=2 valign=\"top\">";
			echo "<table width=\"100%\" colspan =0 colpadding=0><tr><td width=\"200\"  valign=\"top\">";
			$edit_region = $edit_region_id == $cur_region[0];

			echo "<table cellspacing = 1 cellpadding=1 border=0>";
			echo "<tr><td><font size=\"-1\">Strain:</font></td><td><font size=\"-1\">";
			output_editable_field('region_strain',$cur_region[4],$edit_region,10);
			echo "</font></td></tr>";
			if (isset($strain_info[$cur_region[4]])){
				echo "<tr><td><font size=\"-1\">Genotype:</font></td><td><font size=\"-1\"><i>" . $strain_info[$cur_region[4]][0] . '</i>  ' .$strain_info[$cur_region[4]][1] . "</font></td></tr>";
			}

			//	if ($edit_region || ($cur_region[2]!=''&& $cur_region[4]!=''))

			if ($cur_region[3] != "0"){
			  echo "<tr><td><font size=\"-1\"><b>Censored</b>:</font></td><td><font size=\"-1\">";
			if ($cur_region[6] != '')
				echo "" . $cur_region[6] . "";
			echo "</font></td></tr>";
			}

			if ($cur_region[5] != "0"){
			  echo "<tr><td colspan=2><font size=\"-1\"><b>(excluded)</b></font></td></tr>";

}
			//echo ($cur_region[15]);
if ($cur_region[15] > 0) echo "<tr><td colspan=2><font size=\"-1\"><a href=\"ns_view_image.php?image_id=" . $cur_region[15] . "\">Subregion Mask Specified</a></font></td></tr>";
if (strlen($cur_region[10]) > 0 || $edit_region){
echo "<tr><td><font size=\"-1\">Culturing T:</font></td><td><font size=\"-1\">";
			output_editable_field('region_culturing_temperature',$cur_region[10],$edit_region,10);
echo "</font></td></tr>";
 }
if (strlen($cur_region[11]) > 0 || $edit_region){
echo "<tr><td><font size=\"-1\">Experiment T:</font></td><td><font size=\"-1\">";
			output_editable_field('region_experiment_temperature',$cur_region[11],$edit_region,10);
echo "</font></td></tr>";
 }
if (strlen($cur_region[12]) > 0 || $edit_region){
echo "<tr><td><font size=\"-1\">Food Source:</font></td><td><font size=\"-1\">";
			output_editable_field('region_food_source',$cur_region[12],$edit_region,10);
echo "</font></td></tr>";
 }
if (strlen($cur_region[13]) > 0 || $edit_region){
echo "<tr><td><font size=\"-1\">Environment Condition:</font></td><td><font size=\"-1\">";
			output_editable_field('region_environment_condition',$cur_region[13],$edit_region,10);
echo "</font></td></tr>";
 }
if (strlen($cur_region[7]) > 0 || $edit_region){
echo "<tr><td><font size=\"-1\">Condition 1:</font></td><td><font size=\"-1\">";
			output_editable_field('region_condition_1',$cur_region[7],$edit_region,10);
echo "</font></td></tr>";
}
if (strlen($cur_region[8]) > 0 || $edit_region){
echo "<tr><td><font size=\"-1\">Condition 2:</font></td><td><font size=\"-1\">";
			output_editable_field('region_condition_2',$cur_region[8],$edit_region,10);
echo "</font></td></tr>";
 }
if (strlen($cur_region[9]) > 0 || $edit_region){
echo "<tr><td><font size=\"-1\">Condition 3:</font></td><td><font size=\"-1\">";
			output_editable_field('region_condition_3',$cur_region[9],$edit_region,10);

echo "</font></td></tr>";
}
if (strlen($cur_region[2]) > 0 || $edit_region){
				echo "<tr><td><font size=\"-1\">Details:</font></td><td><font size=\"-1\">";
				output_editable_field('region_details',$cur_region[2],$edit_region,10,TRUE);
echo "</font></td></tr>";
}
echo "</table>";
			echo "</font>";
			if ($edit_region)
			  echo "<input type=\"hidden\" name=\"edit_region_id\" value=\"" . $cur_region[0] . "\"><br>";
			echo "</td><td>";
			if ($show_region_jobs){

			  $job = new ns_processing_job;
			  $job->region_id = $cur_region[0];
			  $v = $job->get_processing_state_description(FALSE,$sql);
			  echo $v;
			  //	die($cur_region_id);
				$storyboard_exists = $storyboard_exists_for_region[$cur_region[0]];

				  echo "<font face=\"Arial\" size=\"-2\">Storyboard Generated:</font><font size=\"-2\">";
				if ($storyboard_exists)
				  echo "Yes";
				else echo "No";
				echo "</font><br>";
			  if (strlen($v) > 0)
			    echo "<a href=\"view_processing_job.php?job_id=0&experiment_id=$experiment_id&region_id=".$cur_region[0]."&all_new=1&delete_file_taskbar=1\"><font size=\"-1\">[Delete Images]</font></a> ";
			}

			echo "</td></tr></table>";
			echo "</td>";
			echo "<td bgcolor=\"{$clrs[0]}\" align=\"right\" nowrap valign=\"top\">";
			echo "<a href=\"manage_samples.php?experiment_id=$experiment_id&edit_region_id=".$cur_region[0]."$region_job_query_string#{$experiment->samples[$i]->id()}\"><font size=\"-1\">[Edit]</font><a> ";
			echo "<a href=\"manage_samples.php?experiment_id=$experiment_id&region_id=".$cur_region[0]."&region_action=remove_problems&$region_job_query_string#{$experiment->samples[$i]->id()}\"><font size=\"-1\">[Problem]</font></a> ";
                        echo "<a href=\"manage_samples.php?experiment_id=$experiment_id&region_id=".$cur_region[0]."&region_action=remove_busy&$region_job_query_string#{$experiment->samples[$i]->id()}\"><font size=\"-1\">[Busy]</font></a> ";
			echo "<a href=\"manage_samples.php?experiment_id=$experiment_id&region_id=".$cur_region[0]."&region_action=$censor&$region_job_query_string#{$experiment->samples[$i]->id()}\"><font size=\"-1\">[$censor]</font></a> ";
			echo "<a href=\"manage_samples.php?experiment_id=$experiment_id&region_id=".$cur_region[0]."&region_action=$exclude&$region_job_query_string#{$experiment->samples[$i]->id()}\"><font size=\"-1\">[$exclude]</font></a>";
			if ($edit_region){
			  echo "<br><br><input name=\"save\" type=\"submit\" value=\"Save\">  \n";
			  echo "<input name=\"cancel\" type=\"submit\" value=\"Cancel\"><br><br>\n";
			}
			//	if ($show_region_jobs) {
			  echo "<br>";
			  echo "<a href=\"view_processing_job.php?job_id=0&experiment_id=$experiment_id&region_id=".$cur_region[0]."&all_new=1\"><font size=\"-1\">[New Job]</font></a> ";
			  echo "<a href=\"view_processing_job.php?job_id=0&experiment_id=$experiment_id&region_id=".$cur_region[0]."\"><font size=\"-1\">[Edit Jobs]</font></a>";

			  //  echo "<BR>Plate lifespan: <a href=\"manage_samples.php?experiment_id=$experiment_id&region_id=".$cur_region[0]."&mark_c3=short\"><font size=\"-1\">[short]</font></a><a href=\"manage_samples.php?experiment_id=$experiment_id&region_id=".$cur_region[0]."&mark_c3=long\"><font size=\"-1\">[long]</font></a><a href=\"manage_samples.php?experiment_id=$experiment_id&region_id=".$cur_region[0]."&mark_c3=OK\"><font size=\"-1\">[OK]</font></a>";

			  //		}



			if ($show_region_jobs){
			  //out job info
			  echo '<br><br>'; echo "<table cellspacing=0 cellpadding=0 align = \"right\"><tr><td valign=\"top\">";
			  for ($m = 0; $m < sizeof($region_jobs[ $cur_region[0] ]); $m++){
			    $clrs = $table_colors[$row_color];
			    $cur_job =&  $region_jobs[ $cur_region[0] ][$m];

			    echo $cur_job->get_job_description($sql);
			    echo  "</font>\n";
			    //$row_color = !$row_color;
			  }
			    echo "</td></tr></table>";
			}

			echo "</td></tr>\n";
			/*($clrs = $table_colors[$row_color];
			echo "<tr><td bgcolor=\"$clrs[0]\" colspan=\"5\"></td>\n";
			echo "<td bgcolor=\"$clrs[1]\">&nbsp;</td>";
			echo "<td bgcolor=\"{$clrs[1]}\" nowrap></td></tr>";*/
			$row_color = !$row_color;
		}

	}
?></td>
  </tr>
</table>
</table>
</form>
<?php
display_worm_page_footer();
?>
