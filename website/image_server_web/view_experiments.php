<?php
require_once('worm_environment.php');
require_once('ns_experiment.php');
require_once('ns_processing_job.php');

try{


  $db_name_set = @$_POST['db_name_set'];
  if ($db_name_set==1){

    $db_req_name = $_POST['requested_db_name'];
    $db_ref = $_POST['db_ref'];
  //  die($db_ref);
    if ($db_ref != '')
	$forward = $db_ref;
    else $forward = "view_experiments.php";
    ns_set_database_name($db_req_name);
    header("Location: $forward\n\n");
    die("");
  }
 ns_load_experiment_groups($experiment_groups,$group_order,$sql);
	$show_hidden_experiments = @$query_string['show_hidden_experiments'] === '1';
	$show_disk_usage = @$query_string['show_disk_usage']==='1';
	$show_plate_stats = @$query_string['show_plate_stats']==='1';

	$edit_id = $query_string['edit_id'];
	$id = @$_POST['id'];
	if ($edit_id != '' || $id != ''){
	  $mid = $edit_id;
	  if ($edit_id == '') 
	    $mid = $id;
	  $query = "SELECT long_capture_interval, short_capture_interval, apply_vertical_image_registration, model_filename,id FROM capture_samples WHERE experiment_id=$mid ORDER BY id DESC LIMIT 1";
	  $sql->get_row($query,$res);
	  if (sizeof($res) != 0){
	    $long_capture_interval = $res[0][0];
	    $short_capture_interval = $res[0][1];
	    $apply_vertical_image_registration = $res[0][2];
	    $model_filename = $res[0][3];
	    $query = "SELECT YEAR(FROM_UNIXTIME(time_at_which_animals_had_zero_age)),"
	      ."MONTH(FROM_UNIXTIME(time_at_which_animals_had_zero_age)),"
	      ."DAYOFMONTH(FROM_UNIXTIME(time_at_which_animals_had_zero_age)),"
	      ."HOUR(FROM_UNIXTIME(time_at_which_animals_had_zero_age)),"
	      ."MINUTE(FROM_UNIXTIME(time_at_which_animals_had_zero_age)),"
	      ."number_of_frames_used_to_mask_stationary_objects, maximum_number_of_worms_per_plate "
	      
	      ."FROM sample_region_image_info WHERE sample_id=".$res[0][4] . " LIMIT 1";
	    $sql->get_row($query,$res2);
	    if (sizeof($res2) != 0){
	      $age_0_year = $res2[0][0];
	      $age_0_month = $res2[0][1];
	      $age_0_day = $res2[0][2];
	      $age_0_hour = $res2[0][3];
	      $age_0_minute = $res2[0][4];
	      $number_stationary_mask = $res2[0][5];
	      $max_worms_per_plate = $res2[0][6];
	    }
	    else{
	      $age_0_year = 0;
	      $age_0_month = 0;
	      $age_0_day = 0;
	      $age_0_hour = 0;
	      $age_0_minute = 0;
	      $number_stationary_mask = 0;
	      $max_worms_per_plate = 0;
	    }
	  }
	}
	
	
	if ($_POST['to_jpg'] != ''){
		$id = $_POST['id'];
		$ex = new experiment($id,$sql);
		$ex->generate_jpegs($sql,FALSE);
	
	}
	if ($_POST['spatial_normalize'] != ''){
			$id = $_POST['id'];
			$ex = new experiment($id,$sql);
			$ex->normalize_spatially($sql,FALSE);
		
	}
	if ($_POST['temporal_normalize'] != ''){
			$id = $_POST['id'];
			$ex = new experiment($id,$sql);
			$ex->normalize_temporally($sql);
		
	}
	
	if ($_POST['save'] != ''){
		$experiment_group_id = $_POST['experiment_group_id'];
		$id = $_POST['id'];
		$name = addslashes($_POST['name']);
		$comments = ($_POST['comments']);
		$delete_captured = $_POST['delete_captured_after_mask'];
		$query = "UPDATE experiments SET description='$comments', group_id = $experiment_group_id WHERE id=$id";
	//die($query);
		$sql->send_query($query);
		//$long = $_POST['long_capture_interval'];
		//$short = $_POST['short_capture_interval'];
		//$v_reg = $_POST['apply_vertical_registration'];
		//$model = trim($_POST['model_filename']);
		//	$age_0_day = trim($_POST['age_0_day']);
		//$age_0_month = trim($_POST['age_0_month']);
		//	$age_0_year = trim($_POST['age_0_year']);
		//$age_0_hour = trim($_POST['age_0_hour']);
		//$age_0_minute = trim($_POST['age_0_minute']);
		//$number_stationary_mask = trim($_POST['number_stationary_mask']);
		//$max_worms_per_plate = trim($_POST['max_worms_per_plate']);
		//	die ($max_worms_per_plate);
		//if ($model != ''){
		//$query = "UPDATE capture_samples SET model_filename='$model' WHERE experiment_id=$id";
		//$sql->send_query($query);
		//}
		//if ($age_0_hour == 0 || $age_0_hour == "00")
		  //  $age_0_hour = "0";
		//if ($age_0_minute == 0 || $age_0_minute == "00")
		  //  $age_0_minute = "0";
		//$query = "UPDATE sample_region_image_info as r, capture_samples as s SET
		//	r.number_of_frames_used_to_mask_stationary_objects = '". $number_stationary_mask . "', r.maximum_number_of_worms_per_plate='" . $max_worms_per_plate . "'";
		  //, sample_region_image_info.time_at_which_animals_had_zero_age=UNIX_TIMESTAMP('".
		  //		  $age_0_year . "-" . $age_0_month . "-" . $age_0_day ." ".$age_0_hour.":".$age_0_minute.":00')";
		
		//	$query .= " WHERE s.experiment_id=$id AND s.id=r.sample_id";
		//	die($query);
		//	$sql->send_query($query);

		//	if ($long != $long_capture_interval || $short != $short_capture_interval || $v_reg != $apply_vertical_registration){

		  // $query = "UPDATE capture_samples SET " 
		//long_capture_interval=$long, short_capture_interval=$short,
		//." apply_vertical_image_registration=$v_reg WHERE experiment_id = $id";
		  //$sql->send_query($query);

		//	}
	}
	$lock_cs = "LOCK TABLES capture_schedule WRITE";
	$unlock_cs = "UNLOCK TABLES";
	if (0 && $_POST['delete_future'] != ''){
		$id = $_POST['id'];
		$sql->send_query($lock_cs);
		//$query = "DELETE FROM capture_schedule WHERE experiment_id='$id' AND time_at_start='0' AND missed='0' AND time_at_start='0'";
		$query = "UPDATE capture_schedule SET censored=1 WHERE experiment_id='$id' AND time_at_start='0' AND missed='0'";
$sql->send_query($query);
	        $sql->send_query($unlock_cs);
		$query = "SELECT device_name FROM capture_samples WHERE experiment_id='$id'";
		$sql->get_row($query,$res);
		$dev = array();
		for ($i = 0; $i < sizeof($res); $i++)
			$dev[$res[$i][0]] = 1;
		foreach($dev as $key=>$value){
			
			$query = "DELETE FROM autoscan_schedule WHERE device_name ='" . $key . "'";
			//echo $query . "<br>";
			$sql->send_query($query); 
		}
	}

	if ($_POST['toggle_hide_experiment'] != ''){
	  //	  die("Why would you want to do this??");
		$id = $_POST['id'];
		$query = "UPDATE experiments SET hidden=NOT(hidden) WHERE id='$id'";
		//echo $query;
		$sql->send_query($query);
		header("Location: view_experiments.php\n\n");
		die("");
	}
	if ($_POST['toggle_experiment_priority'] != ''){
	  //	  die("Why would you want to do this??");
		$id = $_POST['id'];
		//if priority==1000, =>500
		//if priority==500, => 1000
		$query = "UPDATE experiments SET priority=(1500-priority) WHERE id='$id'";
		//echo $query;
		$sql->send_query($query);
		header("Location: view_experiments.php\n\n");
		die("");
	}
	if ($_POST['delete_all'] != ''){
	  $id = $_POST['id'];
	  ns_delete_experiment_from_database($id,$sql);

	  header("Location: view_experiments.php\n\n");
	  die("");
	 
	}
	
	$query = "SELECT id, name, description, num_time_points, first_time_point, last_time_point, hidden, delete_captured_images_after_mask,size_unprocessed_captured_images,size_processed_captured_images,size_unprocessed_region_images,size_processed_region_images,size_metadata, size_video, size_calculation_time, priority, group_id FROM experiments ";
	if (!$show_hidden_experiments) $query .= "WHERE hidden = 0 ";
	$query .="ORDER BY group_id,priority DESC, first_time_point DESC";
	$sql->get_row($query,$experiments);
	$experiments_by_group = array();
	for($i = 0; $i < sizeof($group_order); $i++){
		$experiments_by_group[$group_order[$i]] = array();
	}
	foreach($experiments as &$r){

	  		array_push($experiments_by_group[$r[16]], $r);
	}
//var_dump($experiments_by_group);
	//	$query = "SELECT id, name, description, num_time_points, first_time_point, last_time_point FROM experiments WHERE hidden = '1'";
	//	$sql->get_row($query,$hidden);
	
	$query = "SELECT DISTINCT device_name, experiment_id FROM capture_samples";
	$sql->get_row($query,$devices_raw);
	$device_count = array();
	for ($i = 0; $i < sizeof($devices_raw);$i++)
		$device_count[$devices_raw[$i][1]] = 0;
	for ($i = 0; $i < sizeof($devices_raw);$i++)
		$device_count[$devices_raw[$i][1]] = $device_count[$devices_raw[$i][1]] + 1;

	if ($query_string['recalc_stats'] != ''){

		for ($i = 0; $i < sizeof($experiments);$i++){
			$id = $experiments[$i][0];
			$query = "SELECT count(*), min(scheduled_time), max(scheduled_time) FROM capture_schedule WHERE experiment_id='$id' AND censored=0";
			$sql->get_single_row($query,$res);
			
			$query = "UPDATE experiments SET num_time_points = '". ($res[0]+0) . "', first_time_point = '". ($res[1]+0) . "', last_time_point = '".($res[2]+0)."' WHERE id='$id'";
			$sql->send_query($query);
		}

	header("Location: view_experiments.php\n\n");
	}

$OTHER_CENSOR = 0;
$FOGGING = 1;
$EMPTY = 2;
$CONTAMINATED = 3;
$DESICCATION = 4;
$STARVED = 5;
$LARVAE = 6;
$EXCLUDED = 7;
$GOOD = 8;
$TOTAL = 9;

if ($show_plate_stats){
  $strain_info = array();
	foreach ($experiments as $i => &$exp){
	

		$query = "SELECT id,censored, excluded_from_analysis, reason_censored FROM capture_samples WHERE experiment_id=" . $exp[0];
		$sql->get_row($query,$samples_to_count);
		foreach ($samples_to_count as $r)
				$sample_problems[$r[0]] = array($r[1],$r[2],$r[3]);


		$query = "SELECT DISTINCT r.strain FROM capture_samples as s, sample_region_image_info as r WHERE r.sample_id = s.id AND s.experiment_id = $exp[0]";
		$sql->get_row($query,$raw_experiment_strains);
		$experiment_strains[$exp[0]] = array();
		

		$query = 
		$cur_strain_id = 0;
		
		$total = array($OTHER_CENSOR=>0,$FOGGING =>0,
			$EMPTY=>0,$CONTAMINATED=>0,
			$EXCLUDED=>0,$GOOD=>0);
		foreach($raw_experiment_strains as &$cur_strain){
		  
		  if ($cur_strain[0] != ''){
		    $query = "SELECT genotype FROM strain_aliases WHERE strain='{$cur_strain[0]}'";
		    //  die($query);
		    $sql->get_row($query,$r);
		    if (sizeof($r) > 0)
		      $strain_info[$cur_strain[0]] = $r[0][0];
		    else $strain_info[$cur_strain[0]] = '';
		    //  var_dump($strain_info);
		  }
		  
		  
			if (strlen($cur_strain[0]) > 0)
				array_push($experiment_strains[$exp[0]],$cur_strain[0]);
else
			array_push($experiment_strains[$exp[0]],"(Unspecified)");
			$stats =& $plate_statistics[$exp[0]][$cur_strain_id];
			$cur_strain_id++;
			$stats = array($OTHER_CENSOR=>0,$FOGGING =>0,
				$EMPTY=>0,$CONTAMINATED=>0,
				$EXCLUDED=>0,$GOOD=>0);

	
		

			
			$query = "SELECT r.sample_id,r.id, r.censored, r.excluded_from_analysis, r.reason_censored FROM sample_region_image_info as r, capture_samples as s WHERE r.sample_id = s.id AND s.experiment_id=" . $exp[0] . " AND r.strain = '" . $cur_strain[0] . "'";
			$sql->get_row($query,$res);
			$stats[$TOTAL] = sizeof($res);
			foreach ($res as $r){
				$sp &= $sample_problems[$r[0]];
				if ($sp[0] || $r[2]){
					if ($r[2]) $reason = $r[4];
					else $reason = $sp[2];
					if ($reason == 'empty')
						$stats[$EMPTY]++;
					else if ($reason == 'condensation')
						$stats[$FOGGING]++;
					else if ($reason == 'contamination')
						$stats[$CONTAMINATED]++;	
					else if ($reason == 'desiccation')
						$stats[$DESICCATION]++;		
					else if ($reason == 'starved')
						$stats[$STARVED]++;		
					else if ($reason == 'larvae')
						$stats[$LARVAE]++;	
					else $stats[$OTHER_CENSOR]++;
				}
				else if ($sp[1] || $r[3])	$stats[$EXCLUDED]++;
				else 				$stats[$GOOD]++;
			}
			$stats[$TOTAL]-=$stats[$EMPTY];

			foreach($stats as $key=>&$value){
				$total[$key] += $value;
			} 
		}
		if (sizeof($experiment_strains[$exp[0]]) > 1){
			$plate_statistics[$exp[0]][$cur_strain_id] = $total;
			$experiment_strains[$exp[0]][$cur_strain_id] = "All";
		}
		
	}

}
$file_size_totals = array(0,0,0,0,0,0);
foreach($experiments as $file_sizes){
	$file_size_totals[0] += $file_sizes[8];
	$file_size_totals[1] += $file_sizes[9];
	$file_size_totals[2] += $file_sizes[10];
	$file_size_totals[3] += $file_sizes[11];
	$file_size_totals[4] += $file_sizes[12];
	$file_size_totals[5] += $file_sizes[13];
}
$ALL_EXPERIMENTS_ID = 99999999;
$experiments[sizeof($experiments)] = array($ALL_EXPERIMENTS_ID,"All Experiments","[Automatically Generated Data for All Experiments]",0,0,0,0,0,$file_size_totals[0],$file_size_totals[1],$file_size_totals[2],$file_size_totals[3],$file_size_totals[4],$file_size_totals[5]);

$total_data_rate = 0;
 $current_time = ns_current_time();
}
catch(ns_exception $e){
	die("Error: ". $e->text);
}
display_worm_page_header("Home", "",TRUE);
?>
<span class="style1">Welcome to the Lifespan Machine!</span><br><br>
Forward any questions to <?php echo $contact_name . " at <a href=\"mailto:" . $contact_email . "\">$contact_email</a>";  if ($contact_phone != '') echo ", " . $contact_phone?><br>
<table width="100%"><TR><TD valign="top">
	<span class="style1">Image Acquisition</span><br>
	<table bgcolor="#555555" cellspacing='0' cellpadding='1'  width="100%"><tr><td>
	<table cellspacing='0' cellpadding='3'  width="100%">
	<tr <?php echo $table_header_color?>> <td>Image Acquisition Action</td><td>Description</td></tr>
	<tr>
	<td bgcolor="<?php echo $table_colors[0][0]?>">
															   <a href="view_hosts_and_devices.php">Capture Devices and Image Servers</a>
	</td>
	<td bgcolor="<?php echo $table_colors[0][1]?>">View all running hardware and software</td></tr>
	<tr>
	<td bgcolor="<?php echo $table_colors[1][0]?>">
	<a href="view_scanner_activity.php">Image Capture Device Activity</a>
	</td>
	<td bgcolor="<?php echo $table_colors[1][1]?>">View recent capture device activity </td></tr>
	<tr>
	<td bgcolor="<?php echo $table_colors[0][0]?>">
	<a href="ns_view_sample_images.php?all_running_experiments=1"> View Recently Captured Images</a>
	</td>
<td  bgcolor="<?php echo $table_colors[0][1]?>">Inspect all images captured by the image server
</td>
</tr>
	<tr>
	<td bgcolor="<?php echo $table_colors[1][0]?>">
	<a href="manage_experiment_groups.php"> Manage Experiment Groups</a>
	</td>
	<td bgcolor="<?php echo $table_colors[1][1]?>">Add or modify experiment groups</td></tr>
</table></td></tr></table>
</td><td valign="top">
<span class="style1">Image Analysis</span><br>
<table bgcolor="#555555" cellspacing='0' cellpadding='1'  width="100%"><tr><td>
<table cellspacing='0' cellpadding='3'  width="100%">
 <tr <?php echo $table_header_color?>> <td>Image Analysis Action</td><td>Description</td></tr>
<tr>
<td bgcolor="<?php echo $table_colors[0][0]?>">
<a href="view_cluster_status.php"> Cluster Status</a>
</td>
<td bgcolor="<?php echo $table_colors[0][1]?>">View the current status and logs of all servers</td></tr>
<tr>
<td bgcolor="<?php echo $table_colors[1][0]?>">
<a href="view_cluster_efficiency.php"> Analysis Performance Statistics</a>
</td><td bgcolor="<?php echo $table_colors[1][1]?>">View estimates of the speed at which images are currently being processed.</td></tr>
<tr>
<td bgcolor="<?php echo $table_colors[0][0]?>">
<a href="cluster_configuration.php">Server Configuration</a>
</td>
<td bgcolor="<?php echo $table_colors[0][1]?>">View cluster configuration parameters stored in the database</td></tr>
<tr>
<td bgcolor="<?php echo $table_colors[1][0]?>">
<a href="manage_file_deletion_jobs.php">Manage File Deletions</a>
</td>
<td bgcolor="<?php echo $table_colors[1][1]?>">Confirm or reject pending file deletions.</td></tr>
<td bgcolor="<?php echo $table_colors[0][0]?>">
<a href="find_genotype.php">List All Genotypes Observed</a>
</td><td bgcolor="<?php echo $table_colors[0][1]?>">Identify experiments including a specific strain or genotype
</td></tr>
</table></td></tr></table>
</td></tr></table>

<span class="style1">Experiment List</span><br>

<?php
 echo "<a href=\"view_experiments.php?"
	."show_hidden_experiments=". ($show_hidden_experiments?"1":"0")
	."&show_plate_stats=" . ($show_plate_stats?"1":"0")
	. "&show_disk_usage=" . ($show_disk_usage?"0":"1");
	 echo ($show_disk_usage?"\">[Hide Disk Usage]</a>":
				"\">[Show Disk Usage]</a>");

 echo "<a href=\"view_experiments.php?"
	."show_hidden_experiments=". ($show_hidden_experiments?"1":"0")
	. "&show_disk_usage=" . ($show_disk_usage?"1":"0")
	."&show_plate_stats=" . ($show_plate_stats?"0":"1");
	 echo ($show_plate_stats?"\">[Hide Plate Statistics]</a>":
				"\">[Show Plate Statistics]</a>");

 echo "<a href=\"view_experiments.php?"
	."show_plate_stats=" . ($show_plate_stats?"1":"0")
	."&show_disk_usage=" . ($show_disk_usage?"1":"0")
	."&show_hidden_experiments=". ($show_hidden_experiments?"0":"1");
	 echo ($show_hidden_experiments?"\">[Hide Specified Experiments]</a>":
				"\">[Show All Experiments]</a>");
echo "<br>";
?>
<hr width="100%">
  Jump to Experiment Group:
<?php  foreach($experiment_groups as $name => $v){
  echo '<a href="view_experiments.php#'.$v[0]."\">[" . $v[1] . "]</a> ";
}
echo "<br>";
?>
<?php
if (sizeof($experiments) != 0){?>

<form action="view_experiments.php" method="post">
<?php
$j = 0;
for ($i = 0; $i < sizeof($group_order); $i++){
?>
<hr width="100%">
<table border=0 width="100%" cellpadding=0 cellspacing="0"><TR><TD valign="top">
<span class="style1" valign="top"><?php echo  $experiment_groups[$group_order[$i]][1] ?></span></TD><a id = "<?php echo $experiment_groups[$group_order[$i]][0]?>"></a><td align="right"><a href="view_processing_job.php?sample_id=all&region_id=all&experiment_list=<?php
$exps = '';
if (sizeof($experiments_by_group[$group_order[$i]]) > 0){
$exps.= $experiments_by_group[$group_order[$i]][0][0];
for ($exp_i = 1; $exp_i < sizeof($experiments_by_group[$group_order[$i]]); $exp_i++){
	$exps.=";" . $experiments_by_group[$group_order[$i]][$exp_i][0];
}
}
echo $exps;
?>" >[Job for All Regions in Group]</a>
<a href="view_processing_job.php?sample_id=all&experiment_list=<?php echo $exps?>" > [Job for All Samples in Group]</a>
<a href="view_processing_job.php?experiment_list=<?php echo $exps?>" > [Job for All Experiments in Group]</a>


</td></TR></table>
<br>
<table bgcolor="#555555" cellspacing='0' cellpadding='1' width="100%" ><tr><td>
<table cellspacing='0' cellpadding='3'  width="100%">
<?php
//echo "<tr $table_header_color><td colspan = " . ($number_of_columns-1) ." valign='top'><font size=\"+1\">";
//echo $experiment_groups[$group_order[$i]][1] . "</font></td></tr>";
?><tr <?php echo $table_header_color?>><td>Experiment Name</td>

<?php 
if (!$show_plate_stats){
?>
<td><center>Image Analysis</center></td>
<td><center>Capture<br>Schedule</center></td>
<td><center>Annotate Plate Information</center></td>
<td><center>Comments</td><td>Devices</center></td>
<td><center>Duration</center></td>
<?php
$number_of_columns = 9;
}
if ($show_disk_usage){
?>
<td><font size="-2"><center>Total</center></font></td>
<td><font size="-2"><center>Data Rate</center></font></td>
<td><font size="-2"><center>Sample<br>Raw</center></font></td>
<td><font size="-2"><center>Sample<br>Processed</center></font></td>
<td><font size="-2"><center>Region<br>Raw</center></font></td>
<td><font size="-2"><center>Region<br>Processed</center></font></td>
<td><font size="-2"><center>Metadata</center></font></td>
<td><font size="-2"><center>Video</center></font></td>
<?php

$number_of_columns+= 8;
} 
if ($show_plate_stats){
?>
<td><font size="-2"><center>Strain Name</center></font></td>
<td><font size="-2"><center>Fogged Plates</center></font></td>
<td><font size="-2"><center>Contaminated Plates</center></font></td>
<td><font size="-2"><center>Desiccated Plates</center></font></td>
<td><font size="-2"><center>Starved Plates</center></font></td>
<td><font size="-2"><center>Plates with Larvae     </center></font></td>
<td><font size="-2"><center>Misc Censored Plates</center></font></td>
<td><font size="-2"><center>Excluded Plates</center></font></td>
<td><font size="-2"><center>Good Plates</center></font></td>
<td><font size="-2"><center>Total Plates</center></font></td>
<td><font size="-2"><center>Empty Slots</center></font></td>

<?php 
$number_of_columns+= 10;} ?>
<td>&nbsp;</td></tr>


<?php
	for ($exp_i = 0; $exp_i < sizeof($experiments_by_group[$group_order[$i]]); $exp_i++){
	$experiment =$experiments_by_group[$group_order[$i]][$exp_i];
	//var_dump($experiments_by_group[$group_order[$i]]);
	$experiment_id = $experiment[0];
        if ($experiment[6] == '1' && !$show_hidden_experiments){
            continue;
}
        
	$delete_captured_after_mask = ($experiment[7]=='1');
	//die ($delete_captured_after_mask);
	$clrs = $table_colors[$j%2];
        $j++;
	
	$edit = $edit_id==$experiment_id;
	echo "<tr><td bgcolor=\"$clrs[0]\"  valign='top'><font size=\"+1\">";
echo "<a name=\"" . $experiment[1] . "\">";
	output_editable_field("name",$experiment[1],FALSE,35);
	echo "</font></td>";
if (!$show_plate_stats){
	echo "<td bgcolor=\"$clrs[1]\"  valign='top' nowrap><center>";
	echo "<a href=\"manage_samples.php?experiment_id=".$experiment_id . "&hide_region_jobs=1&hide_sample_jobs=1\"> [Run Analysis] </a><br>";
   
	echo "<a href=\"analysis_status.php?experiment_id=".$experiment_id ."\"> [Analysis Status] </a>";
	echo "</center></td>";
	echo "<td nowrap=\"nowrap\" bgcolor=\"$clrs[0]\"  valign='top'><center>";
	echo "<a href=\"view_scanner_schedule.php?experiment_id=".$experiment_id . "&show_past=1\">[View Schedule]</a><br>";
echo "<a href=\"view_processing_job.php?job_id=0&experiment_id=$experiment_id&live_dangerously=1&hide_entire_region_job=1\">[Cancel Pending Scans]</a>";
        echo "</center></td><td nowrap bgcolor=\"$clrs[1]\"  valign='top'>";
        echo "<center><a href=\"experiment_overview.php?experiment_id=".$experiment_id."\">[By Image]</a>"; 
echo "<a href=\"plate_labeling.php?experiment_id=".$experiment_id."\">[By Position]</a><br>";  
echo "<a href=\"device_temperatures.php?experiment_id=".$experiment_id."\">[Device Temperatures]</a>";
	echo "</center></td><td bgcolor=\"$clrs[0]\" valign='top'>";
if ($experiment[6] == '1')
echo "[Experiment is hidden]<br>";


	if ($edit){
	  

	  echo "<table>";
/*	 echo "<tr><td colspan=2>Minimal Interval between Long Capture Pairs:<br>";

	  output_editable_field("long_capture_interval",$long_capture_interval,TRUE,3);
	  echo " minutes</td></tr><tr><td colspan=2>Maximal Interval between Short Capture Pairs:<br>";
	  output_editable_field("short_capture_interval",$short_capture_interval,TRUE,3);

	  echo " minutes</td></tr>";
	*/	
/*
echo "<tr><td colspan = 2> Apply vertical image registration?\n";
	  echo "\n<select name=\"apply_vertical_registration\">\n<option value = \"1\" ";
	  if ($apply_vertical_image_registration) echo "selected";
	  echo ">Yes</option>\n<option value= \"0\" ";
	  if (!$apply_vertical_image_registration) echo "selected";
	  echo ">No</option>\n</select><br></td></tr>\n";*/
/*
	  echo "<tr><td colspan=2>Model Filename</td></tr><tr><td colspan=2>";
	  output_editable_field("model_filename",$model_filename,TRUE);*/
	  /*
	  echo "</td></tr>";
	  echo "<tr><td>Age Zero @</td><td>";
	  output_editable_field("age_0_hour",$age_0_hour,TRUE,2);
	  echo ":";
	  output_editable_field("age_0_minute",$age_0_minute,TRUE,2);
	  echo "<br>";
	  output_editable_field("age_0_month",$age_0_month,TRUE,2);
	  echo "/";
	  output_editable_field("age_0_day",$age_0_day,TRUE,2);
	  echo "/";
	  output_editable_field("age_0_year",$age_0_year,TRUE,4);
     	
	  echo "</td></tr>";*/

echo "<tr><td colspan=2>";
echo "<font size=\"-1\">Experiment Group<br>";
echo "<select name=\"experiment_group_id\">";
	foreach ($group_order as $g){
		echo "<option value=\"". $experiment_groups[$g][0] . "\"";
		if ($experiment[16] == $g[0])
			echo " selected";
		echo ">" . $experiment_groups[$g][1] . "</option>\n";
} 
echo "</td></tr>";
/*
	echo "<tr><td colspan = 2>";
	  echo "Delete captured images after mask?";
	  echo "\n<select name=\"delete_captured_after_mask\">\n<option value = \"0\" ";
	  if (!$delete_captured_after_mask ) echo "selected";
	  echo ">Do not Delete</option>\n<option value= \"1\" ";
	  if ($delete_captured_after_mask ) echo "selected";
	  echo ">Delete</option>\n</select><br>";
	 echo "</td></tr>";
 	 echo "<tr><td><font size=\"-1\">Number of images used to detect stationary plate features<br>(0 for automatic)</font></td><td>";
	 output_editable_field("number_stationary_mask",$number_stationary_mask,TRUE,2);
     	
	  echo "</td></tr>";	 

       	  echo "<tr><td><font size=\"-1\">Maximum Number of Worms per Plate</font></td><td>";
	 output_editable_field("max_worms_per_plate",$max_worms_per_plate,TRUE,2);
     	
	  echo "</td></tr>";*/	 
	  echo "</table>";
	}
	output_editable_field("comments",$experiment[2],$edit,20,TRUE);
	echo "</td>";
	echo "<td bgcolor=\"$clrs[1]\"  valign='top'><center>";
	if ($experiment[5] > $current_time)
		echo '<b><font size = "+1">' . $device_count[$experiment_id] . "</font></b>";
	else echo $device_count[$experiment_id];
	echo "</center></td>";

	echo "<td bgcolor=\"$clrs[0]\"  valign='top' nowrap><center><font size=\"-1\">";
if ($experiment[5] < $current_time){
echo "The experiment ran for " . (round(($experiment[5]-$experiment[4])/60.0/60.0/24.0*10)/10) . " days<br>";
	echo "Started:  " . format_time($experiment[4]) . "<br>";
	echo "Finished: " . format_time($experiment[5]). "<br> ";
}
else{
	echo "Started: " . format_time($experiment[4]) . "<br>";
 echo "The experiment has run for " . round(($current_time - $experiment[4])/60.0/60.0/24*10)/10 . " days.<br>";
	echo "Finish:  " . format_time($experiment[5]). "<br> ";
}
//	echo "<br>" . format_time($experiment[5]). "</b><br> ";
	//echo format_time($experiment[4]) . "</font></center></td>";
}
echo "</td>";
	if ($show_disk_usage){
		echo "<td bgcolor=\"$clrs[1]\"  valign='top'><center><b>";
		echo ns_display_megabytes((int)$experiment[8] +
					(int)$experiment[9] +
					(int)$experiment[10] +
					(int)$experiment[11] +
					(int)$experiment[12] +
					(int)$experiment[13]);
		echo "</b></center></td>";

		echo "<td bgcolor=\"$clrs[0]\"  valign='top'><center><b><font size=\"-1\">";
		$data_size = 	  (int)$experiment[8] +
					(int)$experiment[9] +
					(int)$experiment[10] +
					(int)$experiment[11] +
					(int)$experiment[12] +
					(int)$experiment[13];
	
		$start_time = (int)$experiment[4];
		$stop_time = (int)$experiment[5];
		$calc_time = (int)$experiment[14];
		if ($stop_time < $calc_time)
			$calc_time = $stop_time;
		$duration = $calc_time - $start_time;
		if ($duration != 0){
			$data_rate = $data_size/$duration;
		}
		else $data_rate = 0;
		$data_rate*=60*60*24;
		$data_rate = round($data_rate,2);
		if ($stop_time > $current_time || $experiment_id === $ALL_EXPERIMENTS_ID){
			if ($experiment_id === $ALL_EXPERIMENTS_ID)
				$data_rate = $total_data_rate;
			else 
				$total_data_rate += $data_rate;
			if ($data_rate === 0)
				echo "0b/s";
			else {
				echo ns_display_megabytes($data_rate,TRUE) . "/day<br>";
				echo ns_display_megabytes($data_rate*31,TRUE) . "/month";
			}
		}
		echo "</font></center></b></td>";
		echo "<td bgcolor=\"$clrs[1]\"  valign='top'><center>";
		echo ns_display_megabytes((int)$experiment[8]);
		echo "</center></td>";
		echo "<td bgcolor=\"$clrs[0]\"  valign='top'><center>";
		echo ns_display_megabytes((int)$experiment[9]);
		echo "</center></td>";
		echo "<td bgcolor=\"$clrs[1]\"  valign='top'><center>";
		echo ns_display_megabytes((int)$experiment[10]);
		echo "</center></td>";
		echo "<td bgcolor=\"$clrs[0]\"  valign='top'><center>";
		echo ns_display_megabytes((int)$experiment[11]);
		echo "</center></td>";
		echo "<td bgcolor=\"$clrs[1]\"  valign='top'><center>";
		echo ns_display_megabytes((int)$experiment[12]) ;
		echo "</center></td>";
		echo "<td bgcolor=\"$clrs[0]\"  valign='top'><center>";
		echo ns_display_megabytes((int)$experiment[13]);
		echo "</center></td>";
	}
	if($show_plate_stats){

		$ps =& $plate_statistics[$experiment_id];
		echo "<td bgcolor=\"$clrs[1]\"  valign='top' nowrap><center><font size=\"-1\">";
for ($ss = 0; $ss < sizeof($experiment_strains[$experiment_id]); $ss++){
	echo $experiment_strains[$experiment_id][$ss] . " (<i>" . $strain_info[$experiment_strains[$experiment_id][$ss]] . "</i>)<BR>";
}
echo "</td>";
echo "<td bgcolor=\"$clrs[0]\"  nowrap valign='top'><center><font size=\"-1\">";

for ($ss = 0; $ss < sizeof($experiment_strains[$experiment_id]); $ss++){
		if ($ps[$ss][$FOGGING] > 0){		
			echo round(100*$ps[$ss][$FOGGING]/$ps[$ss][$TOTAL],1)."%";
			echo '<i><font size=\"-1\">('.$ps[$ss][$FOGGING].')</font></i>';
}
echo "<BR>";
		}
		echo "</font></center></td>";
		echo "<td nowrap bgcolor=\"$clrs[1]\"  valign='top'><center><font size=\"-1\">";

for ($ss = 0; $ss < sizeof($experiment_strains[$experiment_id]); $ss++){
		
		if ($ps[$ss][$CONTAMINATED] > 0){
			echo round(100*$ps[$ss][$CONTAMINATED]/$ps[$ss][$TOTAL],1)."%";
			echo '<i><font size=\"-1\">('.$ps[$ss][$CONTAMINATED].')</font></i>';
		}
		echo "<BR>";
}
echo "</font></center></td>";

		echo "<td nowrap bgcolor=\"$clrs[0]\"  valign='top'><center><font size=\"-1\">";

for ($ss = 0; $ss < sizeof($experiment_strains[$experiment_id]); $ss++){
		
		if ($ps[$ss][$DESICCATION] > 0){
			echo round(100*$ps[$ss][$DESICCATION]/$ps[$ss][$TOTAL],1)."%";
			echo '<i><font size=\"-1\">('.$ps[$ss][$DESICCATION].')</font></i>';
		}
		echo "<br>";
	}
	echo "</font></center></td>";
		echo "<td nowrap bgcolor=\"$clrs[1]\"  valign='top'><center><font size=\"-1\">";

for ($ss = 0; $ss < sizeof($experiment_strains[$experiment_id]); $ss++){
		
		if ($ps[$ss][$STARVED] > 0){
			echo round(100*$ps[$ss][$STARVED]/$ps[$ss][$TOTAL],1)."%";
			echo '<i><font size=\"-1\">('.$ps[$ss][$STARVED].')</font></i>';
		}
		echo "<BR>";
}
	echo "</font></center></td>";

		echo "<td nowrap bgcolor=\"$clrs[0]\"  valign='top'><center><font size=\"-1\">";

for ($ss = 0; $ss < sizeof($experiment_strains[$experiment_id]); $ss++){
		
		if ($ps[$ss][$LARVAE] > 0){
			echo round(100*$ps[$ss][$LARVAE]/$ps[$ss][$TOTAL],1)."%";
			echo '<i><font size=\"-1\">('.$ps[$ss][$LARVAE].')</font></i>';
		}
		echo "<br>";
		}
		echo "</font></center></td>";
		echo "<td bgcolor=\"$clrs[1]\" nowrap valign='top'><center><font size=\"-1\">";

for ($ss = 0; $ss < sizeof($experiment_strains[$experiment_id]); $ss++){
		
		if ($ps[$ss][$OTHER_CENSOR] > 0){

			echo round(100*$ps[$ss][$OTHER_CENSOR]/$ps[$ss][$TOTAL],1)."%";
			echo '<i><font size=\"-1\">('.$ps[$ss][$OTHER_CENSOR].')</font></i>';
		}
echo "<BR>";
}
		echo "</font></center></td>";	
		echo "<td nowrap bgcolor=\"$clrs[0]\"  valign='top'><center><font size=\"-1\">";
for ($ss = 0; $ss < sizeof($experiment_strains[$experiment_id]); $ss++){
	
		if ($ps[$ss][$EXCLUDED] > 0){
			echo round(100*$ps[$ss][$EXCLUDED]/$ps[$ss][$TOTAL],1)."%";
			echo '<i><font size=\"-1\">('.$ps[$ss][$EXCLUDED].')</font></i>';
		}
		echo "<br>";
		}
		echo "</font></center></td>";
		
		
		echo "<td  nowrap bgcolor=\"$clrs[1]\"  valign='top'><center><font size=\"-1\">";
		
for ($ss = 0; $ss < sizeof($experiment_strains[$experiment_id]); $ss++){
		
		if ($ps[$ss][$GOOD] > 0){
			echo round(100*$ps[$ss][$GOOD]/$ps[$ss][$TOTAL],1)."%";
			echo '<i><font size=\"-1\">('.$ps[$ss][$GOOD].')</font></i>';
			}
			echo "<br>";
		}
		echo "</font></center></td>";
		echo "<td bgcolor=\"$clrs[0]\"  valign='top'><center><font size=\"-1\">";
		
for ($ss = 0; $ss < sizeof($experiment_strains[$experiment_id]); $ss++){

		if ($ps[$ss][$TOTAL] > 0)
			echo $ps[$ss][$TOTAL];
		echo "<BR>";
}
		echo "</font></center></td>";
		
		echo "<td bgcolor=\"$clrs[1]\"  valign='top'><center><font size=\"-1\">";
	
for ($ss = 0; $ss < sizeof($experiment_strains[$experiment_id]); $ss++){
	
		if ($ps[$ss][$EMPTY] > 0){
			echo round(100*$ps[$ss][$EMPTY]/($ps[$ss][$TOTAL]+$ps[$ss][$EMPTY]),1)."%";
			echo '<i><font size=\"-1\">('.$ps[$ss][$EXCLUDED].')</font></i>';
		}
		echo "<br>";
}
		echo "</font></center></td>";
}
	
	echo "<td bgcolor=\"$clrs[1]\"  valign='top'><center>";

	if ($edit){
		echo " <input type = \"hidden\" name =\"id\" value=\"" . $experiment_id . "\">";
		
		echo "<input type=\"submit\" name=\"save\" value=\"Save\"><br>";
		//		echo "<input type=\"submit\" name=\"delete_future\" value=\"Cancel Pending Scans\" onClick=\"javascript:return confirm('Are you sure you wish to cancel all pending scans?')\"><br>";		
		echo "<input type=\"submit\" name=\"toggle_hide_experiment\" value=\"";
                if ($experiment[6] == '1')
                      echo 'Un-Hide Experiment';
                else echo 'Hide Experiment';
                      echo '"><br>';
		echo "<input type=\"submit\" name=\"toggle_experiment_priority\" value=\"";
		if ($experiment[15] < 1000)
                      echo 'Set as Standard Priority';
                else echo 'Set as Low Priority';
                      echo '"><br>';
		    
		echo "<input type=\"submit\" name=\"delete_all\" value=\"Delete Experiment Data\" onClick=\"javascript:return confirm('Are you sure you wish to delete all data from this experiment?')\"><br>";
	}
	else {
		echo "<a href=\"view_experiments.php?"
	."show_hidden_experiments=". ($show_hidden_experiments?"1":"0")
	."&show_plate_stats=" . ($show_plate_stats?"1":"0")
	."&show_disk_usage=" . ($show_disk_usage?"1":"0");
		echo "&edit_id=".$experiment_id."#" . $experiment[1] . "\">[Edit]</a><br>";
		
	}
	echo "</center></td></tr>";
}
?>
</table></td></tr></table><br>

<?php }?>
</form>

<?php }?>
<a href="view_experiments.php?recalc_stats=1">[Recalculate Experiment Statistics]</a><br><br>

<span class="style1">Schedule Experiment Jobs</span>
<table bgcolor="#555555" cellspacing='0' cellpadding='1' ><tr><td>
<table cellspacing='0' cellpadding='3' >
<tr <?php echo $table_header_color?>> <td>Job Types</td></tr>
<tr><td bgcolor="<?php echo $table_colors[1][0]?>">
	<a href="view_processing_job.php?job_id=0&experiment_id=all">[Schedule a job to run on all experiments]</a>
</td></tr>
<tr><td bgcolor="<?php echo $table_colors[0][0]?>">
	<a href="view_processing_job.php?job_id=0&experiment_id=all&sample_id=all">[Schedule a job to run on all experiment samples]</a>
</td></tr>
<tr><td bgcolor="<?php echo $table_colors[1][0]?>">
	<a href="view_processing_job.php?job_id=0&experiment_id=all&sample_id=all&region_id=all">[Schedule a job to run on all experiment regions</a>
</td></tr>
</table>
</td></tr>
</table><br>
<?php display_worm_page_footer()?>