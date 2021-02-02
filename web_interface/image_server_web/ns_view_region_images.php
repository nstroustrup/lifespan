<?php
require_once('worm_environment.php');
require_once('ns_dir.php');	
//require_once('ns_external_interface.php');
require_once('ns_experiment.php');
require_once('ns_processing_job.php');

try{
  $experiment_id = @$query_string['experiment_id'];
  $region_id = @$query_string['region_id'];
  $region_image_id = @$query_string['region_image_id'];
  $sort_forwards = @$query_string['sort_forwards']==1;
  
  if (array_key_exists('censor_after',$query_string)){
     $censor_after = @$query_string['censor_after'];
     $query = "SELECT MIN(censored) FROM sample_region_images WHERE capture_time >= $censor_after AND region_info_id = $region_id";
     $sql->get_value($query,$val);
     if ($val == 0)
     	$to_set = 1;
     else $to_set = 0;
     $query = "UPDATE sample_region_images SET censored=$to_set WHERE capture_time >= $censor_after AND region_info_id = $region_id";
   
     $sql->send_query($query);
  }
  if (array_key_exists('exclude',$query_string)){
     $exclude = @$query_string['exclude'];
      $query = "UPDATE sample_region_images SET censored= !censored WHERE capture_time = $exclude AND region_info_id = $region_id";
   
     $sql->send_query($query);
  } 

  if ($region_image_id != ''){
    $query = "SELECT r.region_info_id, s.experiment_id, s.name FROM sample_region_images as r, sample_region_image_info as i, capture_samples as s WHERE r.id = $region_image_id AND i.id = r.region_info_id AND i.sample_id = s.id";
    $sql->get_row($query,$res);
    if (sizeof($res) == 0)
      throw new ns_exception("Could not load specified capture sample region image.");
    $region_id = $res[0][0];
    $experiment_id = $res[0][1];
    $sample_name = $res[0][2];
  }
  if ($region_id == 0 || $region_id == '')
    throw new ns_exception("No Region Specified.");
  if ($experiment_id != 0){
    $query = "SELECT name FROM experiments WHERE id=$experiment_id";
    $sql->get_value($query,$experiment_name);
  }
  $query = "SELECT name, sample_id FROM sample_region_image_info WHERE id=$region_id";
  $sql->get_row($query,$res2);
  //var_dump($res2);
  $region_name = $res2[0][0];
  $sample_id = $res2[0][1];
  //die($sample_id);
  $query = "SELECT name FROM capture_samples WHERE id=$sample_id";
  $sql->get_value($query,$sample_name);
  
  $query = "SELECT id, capture_time, image_id, problem, currently_under_processing, censored";
  for ($i = 2; $i <= $NS_LAST_PROCESSING_JOB; $i++)
    $query .= ", op{$i}_image_id";
  
  $query .= " FROM sample_region_images WHERE ";
  if ($region_image_id == '')
      $query.= "region_info_id = $region_id";
  else
    $query.="id = $region_image_id";
  if ($sort_forwards)
  $query .= " ORDER BY capture_time ASC";
  else $query .= " ORDER BY capture_time DESC";
  $sql->get_row($query,$region_images);

$job_offset = 5;
$op_ids_to_use = array();
$op_ids_to_use[0] = 1;
$op_ids_to_use[1] = 0;
for ($i = 2; $i < $NS_LAST_PROCESSING_JOB; $i++){
    $op_ids_to_use[$i] = 0;
    //never show these
    if ( $i == $ns_processing_tasks['ns_process_add_to_training_set'] ||
       $i == $ns_processing_tasks['ns_process_analyze_mask'] ||
       $i == $ns_processing_tasks['ns_process_compile_video'])
       continue;

    for ($j = 0; $j < sizeof($region_images); $j++)
    	if ($region_images[$j][$i+$job_offset] != 0){
	   $op_ids_to_use[$i] = 1;
	   break;
	 }	   
}

  $page_title = $experiment_name . "::" .$sample_name . "::" . $region_name;
}
catch (ns_exception $ex){
  die ($ex->text);
}
display_worm_page_header($page_title, "<a href=\"manage_samples.php?experiment_id=$experiment_id\">[Back to Experiment Samples]</a>");
?>
<?php 
if ($sort_forwards){
  echo "<a href=\"ns_view_region_images.php?sort_forwards=0&region_id=$region_id&experiment_id=$experiment_id\">[Reverse order by time]</a>";
 }
 else echo "<a href=\"ns_view_region_images.php?sort_forwards=1&region_id=$region_id&experiment_id=$experiment_id\">[Order by time]</a>";
if (sizeof($region_images) == 0) echo "(No region images available)";
 else {
   ?>
   <table border="0" cellpadding="0" cellspacing="1" bgcolor="#000000"><tr><td>
     <table border="0" cellpadding="4" cellspacing="0" width="100%">
     <tr <?php echo $table_header_color?> ><td colspan=<?php echo $NS_LAST_PROCESSING_JOB  ?>>Region Samples</td></tr>
     <tr><td bgcolor="<?php echo  $table_colors[0][0] ?>" >Capture Time</td>
    
   <?php
     $j=1;
   for ($i = 1; $i <= $NS_LAST_PROCESSING_JOB; $i++){
      if ($i != 1 && $op_ids_to_use[$i-1] == 0) continue;
      if ($i == 1)
	$name = 'Unprocessed';
      else $name = $ns_processing_task_labels[$i];
      echo "<td bgcolor=\"" . $table_colors[0][$j%2] . "\"><font face=\"Arial\" size=\"-1\"><b>" . $name . "</b></font></td>";
      $j++;
    }
   echo "<td bgcolor=\"" . $table_colors[0][$j%2] . "\"><font face=\"Arial\" size=\"-1\"><b>Edit";
   echo  "</b></font></td>";
?>
     </tr>  
     <?php for ($i = 0; $i < sizeof($region_images); $i++){ 
     ?>
     <tr>
     <td bgcolor="<?php echo  $table_colors[1][0] ?>" nowrap ><font size="-1">
     <?php echo format_time($region_images[$i][1]); 
      //  echo "(" . $region_images[$i][0] . ") ";
     if ($region_images[$i][3] != '0' ||
	 $region_images[$i][4] != '0' ) echo "<br>";
       if ($region_images[$i][3] != '0') echo "<a href=\"view_hosts_log.php?event_id=" . $region_images[$i][3] . '">(Prob)</a>';
       if ($region_images[$i][4] != '0') echo "<a href=\"view_hosts_and_devices.php?host_id={$region_images[$i][4]}\">(Busy)</a>";
       if ($region_images[$i][5] != '0') echo '(Censored)';
?>     </font> </td>
     <td bgcolor="<?php echo  $table_colors[1][1] ?>">
     <a href="ns_view_image.php?image_id=<?php echo $region_images[$i][2];?>"><center><font size="-1">[Image]</font></center></a>
     </td>
     <?php 
	 $k = 0;
	 for ($j = 2; $j <= $NS_LAST_PROCESSING_JOB; $j++){
	if ($op_ids_to_use[$j-1] == 0) continue;
	echo "<td bgcolor=\"{$table_colors[1][$k%2]}\"><center>";
	
	if ($region_images[$i][$j + -1 +$job_offset] != 0){
	  echo "<a href=\"ns_view_image.php?image_id=" . $region_images[$i][$j+$job_offset-1 ] . "\"><font size=\"-1\">[Image]</font></a>";
	}
	else echo "&nbsp;";
	echo "</center></td>";
	$k++;
      }
       echo "<td bgcolor=\"{$table_colors[1][$k%2]}\"><center>";
       echo "<font size=\"-1\">";
       echo "<a href=\"ns_view_region_images.php?region_id=$region_id&experiment_id=$experiment_id&censor_after=".$region_images[$i][1]."\">[Censor_After]</a> ";
        echo "<a href=\"ns_view_region_images.php?region_id=$region_id&experiment_id=$experiment_id&exclude=".$region_images[$i][1]."\">[Censor_This]</a>";
       echo "</font></center></td>";
     ?>
     </tr>  
     <?php }?>
     </table>
	 </table>
	 <?php }?>
	 
	 <?php
	 display_worm_page_footer();
?>
	 