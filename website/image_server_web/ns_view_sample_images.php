<?php
require_once('worm_environment.php');
require_once('ns_dir.php');	
//require_once('ns_external_interface.php');
require_once('ns_experiment.php');
require_once('ns_processing_job.php');


try{
	$experiment_id = @(int)$query_string['experiment_id'];
	$sample_id = @(int)$query_string['sample_id'];
	$all_running_experiments = @((int)$query_string['all_running_experiments'] === 1);
	$start_time = @(int)$query_string['start_time'];
	$stop_time = @(int)$query_string['stop_time'];
	if (!isset($query_string['start_time']))
		$start_time = 24*60*60;
	if (!isset($query_string['stop_time']))
		$stop_time = 0;

	if ($experiment_id == 0 && $sample_id == 0 && !$all_running_experiments)
		throw new ns_exception("No experiment or sample id supplied");

	$q_spec =" FROM captured_images as s, capture_samples as cs, experiments as e WHERE s.sample_id = cs.id AND cs.experiment_id = e.id ";

	if ($experiment_id != 0){
		$q_spec.="AND cs.experiment_id=$experiment_id";
	}
	else{
		if ($sample_id != 0)
			$q_spec.= "AND s.sample_id = $sample_id";
	}

	$query = "SELECT s.capture_time " . $q_spec . " ORDER BY capture_time DESC LIMIT 1";
	$sql->get_row($query,$last_capture_v);

	if (sizeof($last_capture_v) == 0)
		throw new ns_exception("The specified sample/experiment has no images");
	$last_capture = $last_capture_v[0][0];

	$s1 = (int)($last_capture -  $start_time);
	$s2 = (int)($last_capture -$stop_time);
	$query = "SELECT s.capture_time, s.image_id, s.small_image_id, s.mask_applied,  s.currently_being_processed, s.problem,s.censored,s.registration_offset_calculated, s.registration_vertical_offset,s.registration_horizontal_offset,s.never_delete_image, cs.name, e.name, e.id "
	. $q_spec;
	
	$query.= " AND s.capture_time >= " . $s1 . " AND s.capture_time <= " . $s2;
	$query.=" ORDER BY s.capture_time DESC";
	//echo $query;
	$sql->get_row($query,$images);
	if (sizeof($images) == 0)
	throw new ns_exception("Could not load any images from ($experiment_id::$sample_id) between " . format_time($s1) . " and " . format_time($s2));
	
	$page_title = "Captured image information";
}
catch (ns_exception $ex){
  die ($ex->text);
}
if($all_running_experiments == 1)
	$back = "<a href=\"view_experiments.php\">[Back to Experiments]</a>";
else if ($experiment_id != 0)
	$back = "<a href=\"manage_samples.php?experiment_id=$experiment_id\">[Back to Experiment Samples]</a>";
else if (sizeof($images)!= 0)
	$back = "<a href=\"manage_samples.php?experiment_id=" . $images[0][12] . "\">[Back to Experiment Samples]</a>";
else $back = "";

display_worm_page_header($page_title, $back);
?>
<?php 
if (sizeof($images) == 0) echo "(No region images available)";
 else {

	if ($stop_time > 0){
		$offset = $stop_time - $start_time;
		$forward = $stop_time;
		if ($forward > $offset)
			$forward = $offset;
		//die($forward . " " . $offset);
		echo "<a href=\"ns_view_sample_images.php?experiment_id=".(int)$experiment_id."&all_running_experiments=".(int)$all_running_experiments."&sample_id=".(int)$sample_id."&start_time=".(int)($start_time+$offset)."&stop_time=".(int)($stop_time + $offset)."\">[Later Time Points]</a>";
	}
	//echo $start_time . " ". $stop_time ."<BR>";
   ?>
   <table border="0" cellpadding="0" cellspacing="1" bgcolor="#000000"><tr><td>
     <table border="0" cellpadding="4" cellspacing="0" width="100%">
     <tr <?php echo $table_header_color?> ><td colspan=<?php echo $NS_LAST_PROCESSING_JOB  ?>>Captured Images</td></tr>
     <tr>
	<td bgcolor="<?php echo  $table_colors[1][0] ?>" >Subject</td>
	<td bgcolor="<?php echo  $table_colors[1][1] ?>" >Capture Time</td>
	<td bgcolor="<?php echo  $table_colors[1][0] ?>" >Raw Image</td>
	<td bgcolor="<?php echo  $table_colors[1][1] ?>" >Small Image</td>
	<td bgcolor="<?php echo  $table_colors[1][0] ?>" >Mask Applied?</td>
	<td bgcolor="<?php echo  $table_colors[1][1] ?>" >Registration</td>
     </tr>  
     <?php for ($i = 0; $i < sizeof($images); $i++){ 
	$c = $i%2;
     ?>
     <tr><td bgcolor="<?php echo  $table_colors[$c][0] ?>" nowrap ><font size="-1">
	<?php echo $images[$i][11] . "::" . $images[$i][11];?>
	</font> </td>
     <td bgcolor="<?php echo  $table_colors[$c][1] ?>" nowrap ><font size="-1">
     <?php echo format_time($images[$i][0]); 
      //  echo "(" . $region_images[$i][0] . ") ";

       if ($images[$i][5] != '0') echo "<a href=\"view_hosts_log.php?event_id=" . $images[$i][5] . '"><b>(Prob)</b></a>';
       if ($images[$i][4] != '0') echo '(Busy)';
       if ($images[$i][6] != '0') echo '(Censored)';
?>     </font> </td>
     <td bgcolor="<?php echo  $table_colors[$c][0] ?>"><center><font size="-1">
<?php 
	if ($images[$i][1] != '0') echo "<a href=\"ns_view_image.php?image_id= {$images[$i][1]}\">[Image]</a>";
	
      if ( $images[$i][10]=='1') echo "<br>(Protected)";
;?>
    </font></center> </td>   
<td bgcolor="<?php echo  $table_colors[$c][1] ?>"><center><font size="-1">
<?php 
	if ($images[$i][2] != '0') echo "<a href=\"ns_view_image.php?image_id= {$images[$i][2]}\">[Small Image]</a>";
	
      if ( $images[$i][10]=='1') echo "<br>(Protected)";
;?>
    </font></center> 
     </td>
   <td bgcolor="<?php echo  $table_colors[$c][0] ?>">
<center><font size="-1">
<?php if ( $images[$i][3]=='1') echo "Mask Applied";
?>
</font></center></td>
   <td bgcolor="<?php echo  $table_colors[$c][1] ?>"><center><font size="-1">
<?php if ( $images[$i][7]=='1') echo $images[$i][9] . 'x' .  $images[$i][8];?>
</font></center></td>
     </tr>  
     <?php }?>
     </table>
	 </table>
 <?php 

	
	$offset = $stop_time - $start_time;
	$forward = $stop_time+$offset;
	echo "<a href=\"ns_view_sample_images.php?experiment_id=".(int)$experiment_id."&all_running_experiments=".(int)$all_running_experiments."&sample_id=".(int)$sample_id."&start_time=".(int)($start_time-$offset)."&stop_time=".(int)($stop_time - $offset)."\">[Earlier Time Points]</a>";
echo "<a href=\"ns_view_sample_images.php?experiment_id=".(int)$experiment_id."&all_running_experiments=".(int)$all_running_experiments."&sample_id=".(int)$sample_id."&start_time=".(int)(ns_current_time()-365*24*60*60)."&stop_time=\">[View All Time Points]</a>";


}?>
	 
	 <?php
	 display_worm_page_footer();
?>
	 