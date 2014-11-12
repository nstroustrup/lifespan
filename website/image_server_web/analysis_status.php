<?php
require_once('worm_environment.php');
require_once('ns_dir.php');	
//require_once('ns_external_interface.php');
require_once('ns_experiment.php');
require_once('ns_processing_job.php');

$experiment_id=@$query_string['experiment_id'];
$query = "SELECT name FROM experiments WHERE id = $experiment_id";
$sql->get_value($query,$experiment_name);
display_worm_page_header($experiment_name . " Image Analysis Status");
?>
<span class="style1">Mask Application:</span><br>
<?php

  $query = "SELECT a.name, s.sample_id, COUNT(DISTINCT s.id),COUNT(DISTINCT s.image_id), SUM(s.censored>0), SUM(s.problem>0), SUM(s.currently_being_processed>0),SUM(s.mask_applied) FROM captured_images as s, capture_samples as a WHERE s.sample_id = a.id AND a.experiment_id = $experiment_id GROUP BY s.sample_id ORDER BY a.name";
$sql->get_row($query,$sample_info);
$res = array();
$empty = array();
$done = array();
foreach ($sample_info as $d){
  if ($d[3]!=0){
    $p = $d[7]/$d[2];
    if ( $p < .95)
      array_push($res,array($d[0],floor($p*100)));
    else array_push($done,$d[0]);
  }
  else
    arrayy_push($empty,$d[0]);
}
if (sizeof($res) == 0)
  echo "All samples have a mask applied to more than 95% of captured images.<br>";
else{
  echo '<table border="0" cellpadding="0" width="500" cellspacing="1" bgcolor="#000000"><tr><td>';
  echo "<table cellspacing=0 cellpadding=3><tr><td $table_header_color>Samples that need to have a mask applied</td><td $table_header_color >Samples with a mask applied</td></tr>";
  echo "<tr><td bgcolor=\"".$table_colors[0][1]."\" valign=\"top\">";
  foreach ($res as $d){
    echo "<span title=\"$d[1]%\">" .$d[0]. "</span> ";
  }

  echo "</td><td bgcolor=\"".$table_colors[0][0] ."\" valign=\"top\">";
   foreach ($done as $d){
    echo $d[0]. " ";
  }
  echo "</td></tr></table></td></tr></table>";
}
?>
<span class="style1">Plate Image Analysis:</span><br>
<?php 
try{
  
    try{
      $query = "SELECT r.id, r.name, s.name FROM sample_region_image_info as r, capture_samples as s WHERE r.sample_id = s.id AND s.experiment_id = $experiment_id AND r.censored =0 AND s.censored = 0";
      $sql->get_row($query,$region_ids);
      if (sizeof($region_ids) == 0){
	echo "There are no plates specified yet for this experiment.  Have you generated and submitted a sample mask?";
      }
      else{  
    $region_names = array();
      for ($i = 0; $i < sizeof($region_ids);$i++){
	$region_names[$region_ids[$i][0]] = $region_ids[$i][2] . "::" . $region_ids[$i][1];
      }
      $movement_query = 'SELECT id, op' .  $ns_processing_tasks['ns_process_static_mask']
	      .'_image_id, op' . $ns_processing_tasks['ns_process_heat_map']
      .'_image_id,path_movement_images_are_cached, latest_movement_rebuild_timestamp, last_timepoint_in_latest_movement_rebuild  FROM sample_region_image_info WHERE ';
      
$operations = array();
      $processing_jobs = array();
      $query = "SELECT region_info_id, SUM(image_id>0)";
      $query_suffix = " FROM sample_region_images WHERE (";
      for ($i = 0; $i < sizeof($region_ids); $i++){
	$movement_query .= "id = " . $region_ids[$i][0];

	$query_suffix .= "region_info_id = " . $region_ids[$i][0];
	if ($i +1 != sizeof($region_ids)){
	  $query_suffix .= " OR ";
	  $movement_query .= " OR ";
	}
      }
      $query_suffix .= ") AND censored = 0 GROUP BY region_info_id";
      $col = 2;
      for ($i = 1; $i <= $NS_LAST_PROCESSING_JOB; $i++){
	if ($i == $ns_processing_tasks['ns_process_heat_map'] ||
	    $i == $ns_processing_tasks['ns_process_static_mask'] ||
	    $i == $ns_processing_tasks['ns_process_temporal_interpolation'] ||
	    $i == $ns_processing_tasks['ns_process_analyze_mask']  ||
	    $i == $ns_processing_tasks['ns_process_compile_video'])
	  continue;
	$processing_jobs[$i] = $col;
	$col++;
	$query .= ",SUM(op{$i}_image_id > 0)";
      }
      $query .=$query_suffix;
      //die($query);


      $sql->get_row($movement_query,$m);


      $sql->get_row($query,$op_counts);
      //var_dump($op_counts);
      $operations[0] = $op_counts[0][0];
      $r = 1;
      for ($i = 1; $i <= $NS_LAST_PROCESSING_JOB; $i++){
	if ($i == $ns_processing_tasks['ns_process_heat_map'] ||
	    $i == $ns_processing_tasks['ns_process_static_mask'] ||
	    $i == $ns_processing_tasks['ns_process_temporal_interpolation'] ||
	    $i == $ns_processing_tasks['ns_process_analyze_mask']  ||
	    $i == $ns_processing_tasks['ns_process_compile_video'])
	  continue;
	$operations[$i] = $op_counts[0][$r];
	if ($operations[$i] == -1) $operations[$i] = 0;
	$r++;
      }

      $movement_info = array();
      for ($i=0; $i < sizeof($m); $i++){
	$movement_info[$m[$i][0]] = $m[$i];
      }
      $plates_ready_for_med_thresh = array();
      $plates_ready_for_detection = array();
      $plates_ready_for_movement = array();
      $plates_ready_for_validation = array();
      
      for ($i = 0; $i < sizeof($op_counts); $i++){
	$id = $op_counts[$i][0];
	$N = $op_counts[$i][1];
	$p_s = $op_counts[$i][$processing_jobs[$ns_processing_tasks["ns_process_spatial"]]]/$N;
	$p_t = $op_counts[$i][$processing_jobs[$ns_processing_tasks["ns_process_threshold"]]]/$N;
	$p_w = $op_counts[$i][$processing_jobs[$ns_processing_tasks["ns_process_region_vis"]]]/$N;
	if ($p_s < .9)
	  array_push($plates_ready_for_med_thresh,array($id,"Only ".floor($p_s*100)."% of images have median filter applied"));
	else if ($p_t < .9)
	  array_push($plates_ready_for_med_thresh,array($id,"Only ".floor($p_t*100)."% of images have the threshold applied"));
	else if ($p_w < .9)
	  array_push($plates_ready_for_detection,array($id,"Only ".floor($p_w*100)."% of images have worm detection performed."));
	else if ($movement_info[$id][4] == 0)
	  array_push($plates_ready_for_movement,array($id,"Movement Analysis has not been run"));
	else array_push($plates_ready_for_validation,array($id,$movement_info[$id][4]));
      }
      function plate_output_info($d){
	global $region_names;
	return 	"<span title=\"$d[1]\">" .$region_names[$d[0]]. "</span>";
      }

      echo '<table border="0" cellpadding="0" cellspacing="1" bgcolor="#000000"><tr><td>';
echo "<table cellspacing=0 cellpadding=3><tr><td $table_header_color colspan=4>Next step for each plate:</td></tr>";
      echo "<tr $table_header_color><td>Spatial Median / Threshold</td><td>Worm Detection</td><td>Movement Analysis</td><td>Validation</td></tr>";
      echo "<tr><td bgcolor=\"".$table_colors[0][1]."\" valign=\"top\">";
      foreach($plates_ready_for_med_thresh as $d){
	echo plate_output_info($d) ." ";
      }
      echo "</td>";
     echo "<td bgcolor=\"".$table_colors[0][0]."\" valign=\"top\">";
      foreach($plates_ready_for_detection as $d){
	echo plate_output_info($d) . " ";
     
      }
      echo "</td>";     
      echo "<td bgcolor=\"".$table_colors[0][1]."\" valign=\"top\">";
      foreach($plates_ready_for_movement as $d){
	echo plate_output_info($d) . " ";
      }
      echo "</td>";
      echo "<td bgcolor=\"".$table_colors[0][0]."\" valign=\"top\">";
      foreach($plates_ready_for_validation as $d){
	echo plate_output_info($d) . " ";
      }
      echo "</td></tr></table>";
      echo "</td></tr></table>";   
        
    }
    }
    catch (ns_exception $e){
      die($e->text);
    }
    
}
catch(ns_exception $e){
  die($e->text);
}
display_worm_page_footer();
?>
