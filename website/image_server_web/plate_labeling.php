<?php
require_once('worm_environment.php');
require_once('ns_dir.php');	
//require_once('ns_external_interface.php');
require_once('ns_experiment.php');
require_once('ns_processing_job.php');

$experiment_id=@$query_string['experiment_id'];
$selected_devices=@$_POST['selected_devices'];
$load_from_device_name = '';
$only_strain = @$query_string['only_strain']==1;
$only_experiment_conditions = @$query_string['only_experiment_conditions']==1;
#if ($only_experiment_conditions)
#  echo "WHWHWHW";
if (isset($_POST['load_from_device']))
	$load_from_device_name= $_POST['load_from_device_name'];
try{
 	if ($experiment_id == '' || $experiment_id == 0)
		throw new ns_exception("Please specify an experiment number");
	$query = "SELECT name FROM experiments WHERE id = $experiment_id";
	$sql->get_value($query,$experiment_name );


	$query = "SELECT DISTINCT device_name, incubator_location, incubator_name FROM capture_samples as s WHERE s.experiment_id = $experiment_id ORDER BY incubator_name,incubator_location,device_name";
    	$sql->get_row($query,$devices);
	$query = "SELECT r.name, RIGHT(s.name,1) FROM sample_region_image_info as r, capture_samples as s WHERE r.sample_id = s.id AND s.experiment_id = $experiment_id AND s.censored = 0
	ORDER BY RIGHT(s.name,1) DESC";
	$sql->get_row($query,$positions_raw);
	$positions_map = array();
	$positions = array();
	foreach ($positions_raw as $p){
		$positions[$p[1]] = array();
		$positions_map[$p[1]] = array();
}
	foreach($positions_raw as $p){
		$positions_map[ $p[1] ][$p[0]] = '';
	}
	foreach($positions_map as $p => $k){
		foreach($k as $r => $rv)
			array_push($positions[$p],$r);
	}
$strain_positions = $positions_map;
$strain_condition_1_positions = $positions_map;
$strain_condition_2_positions = $positions_map;

  if ($load_from_device_name != ''){
	$query = "SELECT r.name, RIGHT(s.name,1), r.strain, r.strain_condition_1, r.strain_condition_2, r.strain_condition_3, r.culturing_temperature,r.experiment_temperature,r.food_source,r.environmental_conditions FROM sample_region_image_info as r, capture_samples as s WHERE r.sample_id = s.id AND s.experiment_id = $experiment_id
	AND s.device_name = '$load_from_device_name'";
	$sql->get_row($query,$res);
	foreach($res as $r){
		$strain_positions[$r[1]][$r[0]] = $r[2];
		$strain_condition_1_positions[$r[1]][$r[0]] = $r[3];
		$strain_condition_2_positions[$r[1]][$r[0]] = $r[4];
		$strain_condition_3_positions[$r[1]][$r[0]] = $r[5];
		$culturing_temperature_positions[$r[1]][$r[0]] = $r[6];
		$experiment_temperature_positions[$r[1]][$r[0]] = $r[7];
		$food_source_positions[$r[1]][$r[0]] = $r[8];
		$environment_condition_positions[$r[1]][$r[0]] = $r[9];
	}
}
$save = FALSE;
if ($_POST['save_to_all_devices']){
	$save = TRUE;
	$devices_to_save = array();
	foreach($devices as $d)
		array_push($devices_to_save,$d[0]);
}
  if ($_POST["save_to_devices"] != ''){
	$save = true;
	$devices_to_save = $_POST['save_to_device_names'];
}
if ($save){
	//die(var_dump($devices_to_save));
	if (sizeof($devices_to_save) == 0)
		throw new ns_exception("No devices specified");
    $region_data = array();
    foreach($_POST as $key => $value){
      $is_strain = $key[0] == 's';
      $is_strain_condition_1 = $key[0] == '1';
      $is_strain_condition_2 = $key[0] == '2';
      $is_strain_condition_3 = $key[0] == '3';
      $is_culturing_temperature = $key[0] == '4';
      $is_experiment_temperature = $key[0] == '5';
      $is_food_source = $key[0] == '6';
      $is_environmental_conditions = $key[0] == '7';

      if ($key[1] != '_' || $key[3] != '_' || (!$is_strain && !$is_strain_condition_1 && !$is_strain_condition_2 && !$is_strain_condition_3 && !$is_culturing_temperature && !$is_experiment_temperature && !$is_food_source && !$is_environmental_conditions))
	continue;
      $col = substr($key,2,1);
      $row = substr($key,4,1);
 
      if ($is_strain)
	$data[$col][$row]['strain'] = $value;
      else if ($is_strain_condition_1)
	$data[$col][$row]['condition_1'] = $value;
      else if ($is_strain_condition_2)
	$data[$col][$row]['condition_2'] = $value;
      else if ($is_strain_condition_3)
	$data[$col][$row]['condition_3'] = $value;
      else if ($is_culturing_temperature)
	$data[$col][$row]['culturing_temperature'] = $value;
      else if ($is_experiment_temperature)
	$data[$col][$row]['experiment_temperature'] = $value;
      else if ($is_food_source)
	$data[$col][$row]['food_source'] = $value;
      else if ($is_environmental_conditions)
	$data[$col][$row]['environmental_conditions'] = $value;
      
      else throw new ns_exception("Unknown option");
      
    }
    $sql->send_query($query);
   // $query = "LOCK TABLES sample_region_image_info as r WRITE";
   // $sql->send_query ($query);
   // var_dump($data);
    //  die("");

    if ($only_strain){

      foreach ($data as $column_name => $r){
	foreach($r as $row_name => $props){
	  if (!isset($props['strain'])){
	    ob_start();
	    var_dump($props);
	    $vv = ob_get_contents();
	    ob_end_clean();
	    throw new ns_exception("Incomplete information found for " . $column_name . "_" . $row_name . ": " . $vv);
	  }
	  
	  
	  $query = "UPDATE sample_region_image_info as r, capture_samples as s SET r.strain='" . $props['strain'] 
	    . "' WHERE " .
	    "r.name = '" . $row_name . "' AND RIGHT(s.name,1) = '" . $column_name . "' " .
	    "AND r.sample_id = s.id AND s.experiment_id = $experiment_id AND (s.device_name = '" . $devices_to_save[0] ."'";
	  
	  for($i = 1; $i < sizeof($devices_to_save); $i++){
	    $query .= " || s.device_name = '" . $devices_to_save[$i] . "'";
	  }
	  $query .=")";
	  $sql->send_query($query);
	  
	}
      }
    }
    else if ($only_experiment_conditions){

      foreach ($data as $column_name => $r){
	foreach($r as $row_name => $props){
	  if (!isset($props['environmental_conditions'])){
	    ob_start();
	    var_dump($props);
	    $vv = ob_get_contents();
	    ob_end_clean();
	    throw new ns_exception("Incomplete information found for " . $column_name . "_" . $row_name . ": " . $vv);
	  }
	  
	  
	  $query = "UPDATE sample_region_image_info as r, capture_samples as s SET r.environmental_conditions='" . $props['environmental_conditions'] 
	    . "' WHERE " .
	    "r.name = '" . $row_name . "' AND RIGHT(s.name,1) = '" . $column_name . "' " .
	    "AND r.sample_id = s.id AND s.experiment_id = $experiment_id AND (s.device_name = '" . $devices_to_save[0] ."'";
	  
	  for($i = 1; $i < sizeof($devices_to_save); $i++){
	    $query .= " || s.device_name = '" . $devices_to_save[$i] . "'";
	  }
	  $query .=")";
	  $sql->send_query($query);
	  
	}
      }
    }
    else{
      foreach ($data as $column_name => $r){
	foreach($r as $row_name => $props){
	  
	  if (!isset($props['strain']) || !isset($props['condition_1']) || !isset($props['condition_2']) || !isset($props['condition_3']) || !isset($props['condition_3']) || !isset($props['culturing_temperature']) || !isset($props['experiment_temperature']) || !isset($props['food_source']) || !isset($props['environmental_conditions'])){
	    ob_start();
	    var_dump($props);
	    $vv = ob_get_contents();
	    ob_end_clean();
	    throw new ns_exception("Incomplete information found for " . $column_name . "_" . $row_name . ": " . $vv);
	    
	  }
	  $query = "UPDATE sample_region_image_info as r, capture_samples as s SET r.strain='" . $props['strain'] . "', r.strain_condition_1=  '" . $props['condition_1'] . "', r.strain_condition_2='" . $props['condition_2'] . "', r.strain_condition_3='" . $props['condition_3'] . "', r.culturing_temperature='" . $props['culturing_temperature'] . "', r.experiment_temperature='" . $props['experiment_temperature'] . "', r.food_source='" . $props['food_source'] . "', r.environmental_conditions='" . $props['environmental_conditions'] . 
	    
	    
	    "' WHERE " .
	    "r.name = '" . $row_name . "' AND RIGHT(s.name,1) = '" . $column_name . "'" .
	    "AND r.sample_id = s.id AND s.experiment_id = $experiment_id AND (s.device_name = '" . $devices_to_save[0] ."'";
	  
	  for($i = 1; $i < sizeof($devices_to_save); $i++){
	    $query .= " || s.device_name = '" . $devices_to_save[$i] . "'";
	  }
	  $query .=")";
	  //echo $query . "<br>";
	  $sql->send_query($query);
	  
	}
      }
      //  $query = "UNLOCK TABLES";      //  $sql->send_query($query);
      //   $refresh_page = TRUE;
      //die("");
    }
 }
//die('');
  if ($refresh_page === TRUE){
    $s = $only_strain?"&only_strain=1":"";
    header("Location: plate_labeling.php?experiment_id=$experiment_id$s\n\n");
    die("");
  }
  
}
catch (ns_exception $e){
  die($e->text);
}

display_worm_page_header($experiment->name . " Plate Metadata for " . $experiment_name);
?>
<form action="<?php
echo "plate_labeling.php?experiment_id=$experiment_id";
if ($only_strain)
echo "&only_strain=1";
if ($only_experiment_conditions)
echo "&only_experiment_conditions=1";
?>" method="post">
<table border = 0 cellspacing = 0 cellpadding="0" width="100%"><TR><TD>
<span class="style1">Experiment Metadata by Plate Position</span><br>
<?php
if ($only_strain == 1 || $only_experiment_conditions==1){
  echo "<a href=\"plate_labeling.php?experiment_id=" . $experiment_id . "&only_strain=0\">[Show All Fields]</a>";
}
else{
  echo "<a href=\"plate_labeling.php?experiment_id=" . $experiment_id . "&only_strain=1\">[Only Show Strain Data]</a>";
  echo "<a href=\"plate_labeling.php?experiment_id=" . $experiment_id . "&only_experiment_conditions=1\">[Only Show Experiment Conditions]</a>";
}
?>
</TD><td>

<div align="right">
<table border="0" cellpadding="0" cellspacing="1" bgcolor="#000000">

  <tr>
    <td <?php echo $table_header_color?> align="center">Load From Device</td>
</tr>
<tr><td bgcolor="<?php echo $table_colors[0][0];?>">
<select name="load_from_device_name">
<?php 

foreach($devices as $d){
echo "<option value=\"{$d[0]}\"";

if ($load_from_device_name==$d[0]) echo " selected";
echo ">{$d[0]} ({$d[2]} {$d[1]})</option>\n";
}
?>
</select>
<input type="submit" name="load_from_device" value="Load From Device">
</TD></tr>
</table>
</div></td>
</TR></table>
<br>

<table border="0" cellpadding="0" cellspacing="1" bgcolor="#000000" width="100%">

  <tr>
<?php 
$column_id = 0;
foreach ($positions as $column_name => $rows){
?>
    <td <?php echo $table_header_color?> align="center"><br><b>Column <?php echo $column_name?></b><br><br>
    <table cellpadding="0" cellspacing="0" width="100%">
<?php 
	$clrs = $table_colors[$column_id];
	$column_id = !$column_id;
	$row_color = 0;
	foreach($rows as $row){
		echo "<tr><td bgcolor=\"" . $clrs[$row_color]. "\" valign=\"top\" align=\"center\">\n";
		echo "<center><b>Row " . $row . "</b></center>";
		echo "<table border=0><tr>";
if (!$only_experiment_conditions){
  echo "<td>";
		  echo "Strain:</td><td>";
		  output_editable_field('s_' . $column_name . '_' . $row, ns_slash($strain_positions[$column_name][$row]),TRUE,20,FALSE); 
}

if (!$only_strain && !$only_experiment_conditions){
		  echo "</td></tr><tr><td>Culturing T:</td><td>";
		  output_editable_field('4_' . $column_name . '_' . $row, ns_slash($culturing_temperature_positions[$column_name][$row]),TRUE,20,FALSE);	  echo "</td></tr><tr><td>Experiment T:</td><td>";
		  output_editable_field('5_' . $column_name . '_' . $row, ns_slash($experiment_temperature_positions[$column_name][$row]),TRUE,20,FALSE);	  echo "</td></tr><tr><td>Food Source:</td><td>";
		  output_editable_field('6_' . $column_name . '_' . $row, ns_slash($food_source_positions[$column_name][$row]),TRUE,20,FALSE);	 
}
if (!$only_strain){
  echo "</td></tr><tr><td>Environment:</td><td>";
		  output_editable_field('7_' . $column_name . '_' . $row, ns_slash($environment_condition_positions[$column_name][$row]),TRUE,20,FALSE);
}
if (!$only_strain && !$only_experiment_conditions){
		  echo "</td></tr><tr><td>Condition 1:</td><td>";
		  output_editable_field('1_' . $column_name . '_' . $row, ns_slash($strain_condition_1_positions[$column_name][$row]),TRUE,20,FALSE);
		  echo "</td></tr><tr><td>Condition 2:</td><td>";
		  output_editable_field('2_' . $column_name . '_' . $row, ns_slash($strain_condition_2_positions[$column_name][$row]),TRUE,20,FALSE);
		  echo "</td></tr><tr><td>Condition 3:</td><td>";
		  output_editable_field('3_' . $column_name . '_' . $row, ns_slash($strain_condition_3_positions[$column_name][$row]),TRUE,20,FALSE);
}
		  echo "</td></tr></table>"; 
		  echo "</td></tr>";
		if ($row_color)
		  $row_color = 0;
		else $row_color = 1;
	}
	echo "</table></td>";
}

?>
</tr>
</table>

<div align="right">
<table border="0" cellpadding="0" cellspacing="1" bgcolor="#000000">

  <tr>
    <td colspan=2 <?php echo $table_header_color?> align="center">Save to Devices</td>
</tr>
<tr><td><table cellpadding="2" cellspacing="0" border = "0"><TR>

<TD bgcolor="<?php echo $table_colors[0][0];?>">

<select name="save_to_device_names[]" multiple size=<?php echo sizeof($devices);?>>
<?php 

foreach($devices as $d){
echo "<option value=\"{$d[0]}\"";
if ($load_from_device_name==$d[0]) echo " selected";
	echo ">{$d[0]} ({$d[2]} {$d[1]})</option>\n";
}
?>
</select><br><br>
</td><td valign="bottom" bgcolor="<?php echo $table_colors[0][0];?>">
<input type="submit" name="save_to_devices" value="Save to Selected Devices"><br><br>
</TD></tr>
<tr>
<TD bgcolor="<?php echo $table_colors[1][1];?>">&nbsp;</TD>

<TD bgcolor="<?php echo $table_colors[1][1];?>" align="right">
<input type="submit" name="save_to_all_devices" value="Save to All Devices">
<//d></TR></table></TD></tr>
</table>
<br></div>

</form>
<?php
display_worm_page_footer();
?>
