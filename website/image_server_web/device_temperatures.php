<?php
require_once('worm_environment.php');
require_once('ns_dir.php');	
//require_once('ns_external_interface.php');
require_once('ns_experiment.php');
require_once('ns_processing_job.php');

$experiment_id=@$query_string['experiment_id'];
$selected_devices=@$_POST['selected_devices'];
$load_from_device_name = '';
$only_strain = @$query_string['only_strain'];

try{
  if ($_POST['save']){
    //die(var_dump($devices_to_save));
    foreach($_POST as $key => $value){
      
      if ($key[0] != 't' || $key[1] != '_')
	continue;
      
      $device_name = substr($key,2);
      
      $device_temperatures[$device_name] = $value;
      
    }
    foreach($device_temperatures as $d=>$t){
      $query = "UPDATE sample_region_image_info as r, capture_samples as s SET r.experiment_temperature = '" . $t . "' WHERE s.experiment_id = $experiment_id AND s.device_name = '" . $d . "' AND s.id = r.sample_id";
      $sql->send_query($query);
    }
    
    header("Location: device_temperatures.php?experiment_id=$experiment_id\n\n");
    die(""); 
  }
  $query = "SELECT name FROM experiments WHERE id = $experiment_id";
  $sql->get_value($query,$experiment_name);
  $query = "SELECT device_name,id,incubator_location FROM capture_samples WHERE experiment_id = $experiment_id ORDER BY incubator_location";
  $sql->get_row($query,$devices);
  foreach($devices as $d){
    $device_names[$d[0]] = $d[0];
    $device_locations[$d[0]] = $d[2];
  }
  foreach($devices as $d){
    $query = "SELECT experiment_temperature FROM sample_region_image_info WHERE sample_id = " . $d[1] . " LIMIT 1";
    $sql->get_row($query,$temperatures);
    $device_temperatures[$d[0]] = $temperatures[0][0];
  }
}
catch (ns_exception $e){
  die($e->text);
}

display_worm_page_header($experiment->name . " Device Temperatures for " . $experiment_name);
?>
<div align="center">
<form action="<?php
echo "device_temperatures.php?experiment_id=$experiment_id";
?>" method="post">

<table border="0" cellpadding="0" cellspacing="1" bgcolor="#000000">

  <tr>
<td <?php echo $table_header_color?> align="center"><b>Device Name<?php echo $column_name?></b></td><td <?php echo $table_header_color?> align="center">Temperature</td></tr>
<?php
$row_color = 0;
	foreach($device_names as $d){
$row_color = !$row_color;
		echo "<tr><td bgcolor=\"" . $table_colors[0][$row_color]. "\" valign=\"top\" align=\"center\">\n";
		echo "<center><b>" . $d . "</b> (" . $device_locations[$d] . ")</center>";
echo "</td><td  bgcolor=\"" . $table_colors[1][$row_color]. "\" valign=\"top\" align=\"center\">";
output_editable_field("t_" . $d, $device_temperatures[$d],TRUE);
echo "</td></tr>";
	}


?>
</table>
<input type="submit" name="save" value="Save"><br><br>
</div>

</form>
<?php
display_worm_page_footer();
?>
