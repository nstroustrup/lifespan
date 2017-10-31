<?php
require_once('worm_environment.php');
require_once('ns_dir.php');
//require_once('ns_external_interface.php');
require_once('ns_experiment.php');
require_once('ns_processing_job.php');

$experiment_id=@$query_string['experiment_id'];
$sample_id=@$query_string['sample_id'];
$save_data=@$query_string['save_data'];
$region_id=@$query_string['region_id'];
$start_position = @$query_string['start_position'];

$thumbnail_column = "op" . $ns_processing_tasks['ns_process_resized_sample_image'] . "_image_id";
$video_column = "op0_video_id";

if (@$query_string['show_images']=='1')
  $show_images = TRUE;
 else $show_images = FALSE;

if ($start_position == '')
  $start_position = 0;
$count = @$query_string['count'];
if ($count == '')
  $count = 4;
try{
  if ($experiment_id == '' || $experiment_id == 0)
    throw new ns_exception("Please specify an experiment number");
  $experiment = new ns_experiment($experiment_id,'',$sql,false);

  if (ns_param_spec($_POST,"save")){
    $sample_data = array();
    $region_data = array();
    foreach($_POST as $key => $value){
      $is_sample = $key[0] == 's';
      $is_region = $key[0] == 'r';
      if (!$is_sample && !$is_region)
	continue;
      $is_description = $key[1] == 'd';
      $is_strain = $key[1] == 's';
      $is_censored = $key[1] == 'c';
      $is_censored_reason = $key[1] == 'r';
      $is_strain_condition_1 = $key[1] == '1';
      $is_strain_condition_2 = $key[1] == '2';
      if (!$is_description && !$is_censored && !$is_strain && !$is_censored_reason && !$is_strain_condition_1 && !$is_strain_condition_2)
	continue;
      $id = substr($key,2);
      if ($is_sample)
	$data =& $sample_data;
      else $data =& $region_data;
      if ($is_description)
	$data[$id]['description'] = $value;
      else if ($is_strain)
	$data[$id]['strain'] = $value;
      else if ($is_censored)
	$data[$id]['censored'] = $value;
      else if ($is_censored_reason)
	$data[$id]['censored_reason'] = $value;
      else if ($is_strain_condition_1)
	$data[$id]['condition_1'] = $value;
      else if ($is_strain_condition_2)
	$data[$id]['condition_2'] = $value;
      else throw new ns_exception("Unknown option");
	//die($data[$id]['condition_1']);

    }
    //$query = "LOCK TABLES capture_samples WRITE";
    //$sql->send_query($query);
    foreach ($sample_data as $key => $value){
      $censoring_requested = $value['censored'];
      $censored_reason = $value['censored_reason'];

      $was_censored = $censoring_requested || ($censored_reason != '' && $censored_reason != 'none');


      if ($was_censored){
	if ($censored_reason == 'none')
		$censored_reason = 'unknown';
	else if ($censored_reason != 'empty' &&
		$censored_reason != 'condensation' &&
		$censored_reason != 'contamination' &&
		$censored_reason != 'contamination' &&
		$censored_reason != 'desiccation' &&
		$censored_reason != 'starved' &&
		$censored_reason != 'larvae'){
			die("Unknown sample censored reason: $censored_reason");
			//$censored_reason = 'unknown';
		}
	}
	else $censored_reason='';
      $query = "UPDATE capture_samples SET censored = " . ($was_censored?"1":"0") . ", description='" . $value['description'] . "', "
		."reason_censored='$censored_reason' WHERE id=" . $key;
//	echo $query . "<BR>";
      $sql->send_query($query);
    }
    //$query = "UNLOCK TABLES";
    //$sql->send_query($query);
    //$query = "LOCK TABLES sample_region_image_info WRITE";
    //$sql->send_query ($query);
    foreach ($region_data as $key => $value){

    // echo $key . "=" ;
//var_dump($value); echo "<BR>";
      $censoring_requested = $value['censored'];
      $censored_reason = $value['censored_reason'];

      $was_censored = $censoring_requested || ($censored_reason != '' && $censored_reason != 'none');
      if ($was_censored){
	if ($censored_reason == 'none')
		$censored_reason = 'unknown';
	else if ($censored_reason != 'empty' &&
		$censored_reason != 'condensation' &&
		$censored_reason != 'contamination' &&
		$censored_reason != 'contamination' &&
		$censored_reason != 'desiccation' &&
		$censored_reason != 'starved' &&
		$censored_reason != 'larvae' &&
		$censored_reason != 'other'){
			die("Unknown region censored reason: $censored_reason in $key");
			//$censored_reason = 'unknown';
		}
	}

      $query = "UPDATE sample_region_image_info SET censored = " . ($was_censored?"1":"0") . ", details='" . $value['details'] . "', strain='" . $value['strain'] . "',reason_censored='$censored_reason', strain_condition_1=  '" . $value['condition_1'] . "', strain_condition_2='" . $value['condition_2'] . "' WHERE id=" . $key;
    //  echo $query . "<br>";
      $sql->send_query($query);
    }
    //$query = "UNLOCK TABLES";
    //$sql->send_query($query);
 //   $refresh_page = TRUE;
  }
$refresh_page = FALSE;
//die('');
  if ($refresh_page === TRUE){
    header("Location: experiment_overview.php?experiment_id=$experiment_id&start_position=$start_position&count=$count\n\n");
    die("");
  }




  $experiment->get_sample_information($sql);

  $regions = array();

  $sample_list='';
  $query = "SELECT v from constants WHERE k='mask_time=$experiment_id'";
  $sql->get_row($query,$res);

  $mask_time = 0;
  if (sizeof($res) > 0)
    $mask_time = $res[0][0];
  //die($mask_time);

  for ($i = $start_position; $i < ($start_position + $count) && $i< sizeof($experiment->samples); $i++){

    $sample_list .= $experiment->samples[$i]->id() . "o";

    $query = "SELECT id,name,details,censored,reason_censored,strain,$video_column, time_at_which_animals_had_zero_age, time_of_last_valid_sample,strain_condition_1,strain_condition_2 FROM sample_region_image_info WHERE sample_id = " . $experiment->samples[$i]->id() . " ORDER BY name";

    $region_info_data_column_size = 11;
    $reg =&  $regions[$experiment->samples[$i]->id()];
    $sql->get_row($query,$reg);
	//var_dump($reg);
//echo "<br><br>";
  //  $reg =& $regions[$experiment->samples[$i]->id()];
    if (!$show_images)
	continue;
    for ($j = 0; $j < sizeof($reg); $j++){
      $d = 15;
      $query1 = "SELECT image_id,$thumbnail_column,capture_time FROM sample_region_images WHERE region_info_id = " . $reg[$j][0] . " AND problem = 0 AND currently_under_processing=0 ";
	//die($query);
	if ($reg[$j][8] != 0)
		$query1 .= " AND capture_time <= " .$reg[$j][8];
	else if ($mask_time != 0)
	  $query1 .= "AND capture_time <= " . $mask_time;

 	$query1 .= " ORDER BY capture_time DESC LIMIT $d";

	//die($query);
       $sql->get_row($query1,$tmp);
	$reg[$j][$region_info_data_column_size+3] = $query1;
	//echo sizeof($tmp) . " ";
      if (sizeof($tmp) == 0){
	$reg[$j][$region_info_data_column_size] = "";
      }
      else {
	$best_raw = -1;
	$best_thumbnail= -1;
	for ($k = 0; $k < sizeof($tmp); $k++){
		if ($best_raw == -1 && $tmp[$k][0] != 0)
			$best_raw = $k;
		if ($best_thumbnail == -1 && $tmp[$k][1] != 0)
			$best_thumbnail = $k;
	}
	if ($best_thumbnail !=-1){
		$reg[$j][$region_info_data_column_size] =  $tmp[$best_thumbnail][1];
		$reg[$j][$region_info_data_column_size+1] = 'thumbnail';
		$reg[$j][$region_info_data_column_size+2] = $tmp[$best_thumbnail][2];
	}
	else if ($best_raw != -1){
		$reg[$j][$region_info_data_column_size] = $tmp[$best_raw][0];
		$reg[$j][$region_info_data_column_size+1] = 'full image';
		$reg[$j][$region_info_data_column_size+2] = $tmp[$best_raw][2];
	}
	else{
		$reg[$j][$region_info_data_column_size] = '';
		$reg[$j][$region_info_data_column_size+1] = '';
		$reg[$j][$region_info_data_column_size+2] = '';
	}
      }
    }
  }
//if ($show_images)

//	die('');
  if (sizeof($experiment->samples) == 0)
    throw new ns_exception("No samples present in experiment");
  $sample_list = substr($sample_list,0,-1);

  $reached_end = FALSE;
  if ($start_position > sizeof($experiment->samples))
    $start_position = sizeof($experiment->samples)-1;
  if ($start_position + $count > sizeof($experiment->samples)){
    $reached_end=TRUE;
    $count = sizeof($experiment->samples)-$start_position;
  }
  if ($start_position + $count >= sizeof($experiment->samples))
    $reached_end = TRUE;
}
catch (ns_exception $e){
  die($e->text);
}
function output_nav_links(){
  global $reached_end,$start_position,$count,$experiment_id,$show_images;
  if ($reached_end)
    return;
  if ($start_position != 0){
  echo "<a href=\"experiment_overview.php?experiment_id=$experiment_id&show_images=" . ($show_images?"1":"0") . "&start_position=";
  echo $start_position - $count;
  echo "&count=" . $count . "\">[Previous]</a>";
}
  echo "<a href=\"experiment_overview.php?experiment_id=$experiment_id&show_images=" . ($show_images?"1":"0") . "&start_position=";
  echo $start_position + $count;
  echo "&count=" . $count . "\">[Next]</a>";
}
display_worm_page_header($experiment->name . " Overview");
?>
<!--
<video src="/image_server_web/long_term_storage/partition_000/2010_01_13_daf16_uno/video/regions/2010_01_13_daf16_uno=cube_a=3=.m4v" controls="controls" onerror="failed(event)">No video support</video>
-->

<span class="style1">Experiment Samples</span><br>
Displaying <?php
echo "(";
echo $start_position;
echo " - ";
echo $start_position + $count;
echo ") / ";
echo sizeof($experiment->samples);?>
<br>

<?php
echo "<a href=\"view_sample_videos.php?samples=$sample_list\">[View All Samples]</a> ";
output_nav_links()
?>
<form action="<?php
echo "experiment_overview.php?experiment_id=$experiment_id&show_images=" . ($show_images?"1":"0") . "&start_position=";
echo $start_position + $count;
echo "&count=";
echo $count;
echo "&show_images=";
if ($show_images==TRUE)
echo "1";
else echo "0";
?>" method="post">
<table border="0" cellpadding="0" cellspacing="1" bgcolor="#000000" width="100%">
  <tr>
    <td>
<table border="0" cellpadding="4" cellspacing="0" width="100%">
  <tr <?php echo $table_header_color?> ><td>Name</td><td >Latest Image</td><td>Censor</td><td>Strain / Description</td><td>&nbsp;</td></tr>


 <?php
	$row_color = 0;
	for ($i = $start_position; $i < $start_position + $count; $i++){
		$clrs =& $table_colors[$row_color];
		$cur_id = $experiment->samples[$i]->id();

		echo "<tr><td bgcolor=\"$clrs[1]\" valign=\"top\">\n";
		echo "<a name=\"" . $experiment->samples[$i]->id() . "\">";
		echo "<b>";
		echo ns_slash($experiment->samples[$i]->name()) . "</b><br><br>";
		echo "<font size=\"-2\"><a href=\"ns_view_sample_images.php?sample_id=" . $experiment->samples[$i]->id() . "\">[View Images]</a></font>";
		echo "</td>";
		echo "<td bgcolor=\"$clrs[1]\" valign=\"top\">\n" ;
		if ($show_images)
		    echo "<a href=\"experiment_overview.php?experiment_id=$experiment_id&start_position=$start_position&count=$count&show_images=0\">[Hide Images]</a>";
		echo "</td><td bgcolor=\"$clrs[1]\" valign=\"center\" align=\"center\">\n";
		echo "<input type=\"checkbox\" name=\"sc" . $experiment->samples[$i]->id() . "\"";
		if ($experiment->samples[$i]->censored)
		  echo " checked";
		echo " value=\"1\">";
		echo "\n</td><td bgcolor=\"$clrs[1]\" valign=\"top\" width=\"200\"><font size=\"-1\">";
		$det_height = 5;
		if (!$show_images)
			$det_height = 1;
		output_editable_field('sd' . $experiment->samples[$i]->id(), ns_slash($experiment->samples[$i]->description),TRUE,30,TRUE,$det_height);
		echo "\n</font><br>";
		echo "Reason Censored: ";
		echo "<select name=\"sr" .  $experiment->samples[$i]->id() . "\" size=1>\n";

		echo "<option value = \"none\" " .
			(($experiment->samples[$i]->censored == 0)?'selected="selected"':'') .
 			">None</option>\n";
		echo "<option value=\"condensation\" " .
			(($experiment->samples[$i]->reason_censored == 'condensation')?'selected="selected"':'') .
 			">Fogging</option>\n";
		echo "<option value= \"contamination\" "  .
			(($experiment->samples[$i]->reason_censored == 'contamination')?'selected="selected"':'') .
			">Contamination</option>\n";
		echo "<option value=\"desiccation\" " .
			(($experiment->samples[$i]->reason_censored == 'desiccation')?'selected=1':'') .
			">Desiccation</option>\n";
		echo "<option value=\"starved\" " .
			(($experiment->samples[$i]->reason_censored == 'starved')?'selected=1':'') .
			">Starved</option>\n";
		echo "<option value=\"larvae\" " .
			(($experiment->samples[$i]->reason_censored == 'larvae')?'selected=1':'') .
			">Larvae</option>\n";
		echo "<option value=\"empty\" " .
			(($experiment->samples[$i]->reason_censored == 'empty')?'selected=1':'') .
			">Empty</option>\n";
		echo "<option value=\"other\" " . (
			($experiment->samples[$i]->censored != 0 &&
			$experiment->samples[$i]->reason_censored != 'condensation'&&
			$experiment->samples[$i]->reason_censored != 'contamination'&&
			$experiment->samples[$i]->reason_censored != 'empty'&&
			$experiment->samples[$i]->reason_censored != 'starved'&&
			$experiment->samples[$i]->reason_censored != 'larvae')?'selected="selected"':'') .
			 ">Other</option>\n";
		echo "</select>\n";


		echo "</td><td bgcolor=\"$clrs[1]\" valign=\"top\" align=\"right\">\n";
		echo "<input name=\"save\" type=\"submit\" value=\"Save\"><br>\n";
		echo "</td>";
		echo "</td></tr>";

		$regs =& $regions[$experiment->samples[$i]->id()];
		for ($j = 0; $j < sizeof($regs); $j++){
		  if ($row_color)
		    $row_color = 0;
		  else $row_color = 1;
		  $clrs =& $table_colors[$row_color];
		  echo "<tr><td bgcolor=\"$clrs[1]\" valign=\"top\">\n";
		  echo "<a name=\"" . $regs[$j][0] . "\">";
		  echo ns_slash($experiment->samples[$i]->name() . "::" . $regs[$j][1]);

			echo  "<br><font size=\"-2\"><a href=\"ns_view_region_images.php?region_id={$regs[$j][0]}&experiment_id={$experiment->id()}\">[View Images]</a></font><br>";
						echo "<font size=\"-2\"><a href=\"view_movement_data.php?region_id={$regs[$j][0]}\"><font size=\"-2\">[View Movement]</font></a></font><br>";
						echo "<font size=\"-2\"><a href=\"view_hosts_log.php?region_id={$regs[$j][0]}\"><font size=\"-2\">[View Problems]</font></a><br>";

		  echo "</td>";
		  echo "<td bgcolor=\"$clrs[0]\" valign=\"top\">\n" ;
		 // var_dump($regs[$j]);
		if ($show_images!==TRUE)
		    echo "<a href=\"experiment_overview.php?experiment_id=$experiment_id&start_position=$start_position&count=$count&show_images=1\">[Display Images]</a>";

		  else if ($regs[$j][$region_info_data_column_size] === '')
		    echo "(No Images Available)";
		  else{
			//echo "image_id={$regs[$j][6]}&video_id={$regs[$j][5]}";
			$image_height = 300;
			$image_width = 300;
			$image_frame_y_buf = 50;
			$image_frame_x_buf = 20;
			echo "<iframe src=\"ns_view_image.php?image_id={$regs[$j][$region_info_data_column_size]}&video_image_id={$regs[$j][6]}&height=$image_height&width=$image_width&redirect=1\" width=\"".($image_width + $image_frame_x_buf)."\" height=\"".($image_height+$image_frame_y_buf)."\"></iframe>";
			echo "<br><font size=\"-2\"><b>";
			echo "Showing " . $regs[$j][$region_info_data_column_size+1] ." : ";
			echo format_time($regs[$j][$region_info_data_column_size+2]);
			echo "</b></font>";
		}
		  echo "<br><font size=\"-2\">";
		  echo 'Experiment boundaries: ['.format_time($regs[$j][7]) . '] - [' . format_time($regs[$j][8]) . ']';
		  echo "</font>";
		  echo "</td><td bgcolor=\"$clrs[1]\" valign=\"center\" width=20 align=\"center\">\n";
		  echo "<input type=\"checkbox\" name=\"rc" . $regs[$j][0] . "\"";
		  if ($regs[$j][3])
		    echo " checked";
		  echo " value = \"1\">";

		  echo "\n</td><td bgcolor=\"$clrs[0]\" valign=\"top\" width=\"200\"><font size=\"-1\">";
		  echo "<table border=0 cellpadding = 1 cellspacing = 0><tr><td>";
		  echo "Strain:</td><td>";
		  output_editable_field('rs' . $regs[$j][0], ns_slash($regs[$j][5]),TRUE,20,FALSE);
		  echo "</td></tr><tr><td>Condition 1:</td><td>";
		  output_editable_field('r1' . $regs[$j][0], ns_slash($regs[$j][9]),TRUE,20,FALSE);
		  echo "</td></tr><tr><td>Condition 2:</td><td>";
		  output_editable_field('r2' . $regs[$j][0], ns_slash($regs[$j][10]),TRUE,20,FALSE);
		  echo "</td></tr></table>";
		  echo "Details: ";
		output_editable_field('rd' . $regs[$j][0], ns_slash($regs[$j][2]),TRUE,30,TRUE,$det_height);
		//  echo $regs[$j][11];
		  echo "\n</font>";

		echo "Reason Censored: ";
		echo "<select name=\"rr" .  $regs[$j][0] . "\" size=1>\n";

		echo "<option value = \"none\" " .
			(($regs[$j][3] == 0)?'selected="selected"':'') .
 			">None</option>\n";
		echo "<option value=\"condensation\" " .
			(($regs[$j][4] == 'condensation')?'selected="selected"':'') .
 			">Fogging</option>\n";
		echo "<option value= \"contamination\" "  .
			(($regs[$j][4] == 'contamination')?'selected="selected"':'') .
			">Contamination</option>\n";
		echo "<option value=\"desiccation\" " .
			(($regs[$j][4] == 'desiccation')?'selected=1':'') .
			">Desiccation</option>\n";
		echo "<option value=\"starved\" " .
			(($regs[$j][4] == 'starved')?'selected=1':'') .
			">Starved</option>\n";
		echo "<option value=\"larvae\" " .
			(($regs[$j][4] == 'larvae')?'selected=1':'') .
			">Larvae</option>\n";
		echo "<option value=\"empty\" " .
			(($regs[$j][4] == 'empty')?'selected=1':'') .
			">Empty</option>\n";
		echo "<option value=\"other\" " . (
			($regs[$j][3] != 0 &&
			$regs[$j][4] != 'condensation'&&
			$regs[$j][4] != 'contamination'&&
			$regs[$j][4] != 'desiccation'&&
			$regs[$j][4] != 'empty' &&
			$regs[$j][4] != 'larvae'&&
			$regs[$j][4] != 'starved')?'selected="selected"':'') .
			 ">Other</option>\n";
		echo "</select>\n";


		  echo "<br><br></td><td bgcolor=\"$clrs[1]\" valign=\"top\" align=\"right\">\n";
		  echo "<input name=\"save\" type=\"submit\" value=\"Save\"><br>\n";
		  echo "</td>";
		  echo "</td></tr>";
		}
		if ($row_color)
		  $row_color = 0;
		else $row_color = 1;

	}

?>
</table>
</table>
<?php output_nav_links()?>
</form>
<?php
display_worm_page_footer();
?>
