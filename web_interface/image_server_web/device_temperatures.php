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
$incubators = array();
$simple_table = @$query_string['simple_table']==1;
try{
$device_temperatures = array();
  if (ns_param_spec_true($_POST,'save')){
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
  $query = "SELECT device_name,id,incubator_location, incubator_name FROM capture_samples WHERE experiment_id = $experiment_id ORDER BY incubator_name, incubator_location, device_name";
  $sql->get_row($query,$devices);
  foreach($devices as $d){
    $device_names[$d[0]] = $d[0];
    $device_locations[$d[0]] = $d[2];
    $device_incubator[$d[0]] = $d[3];
    $incubators[$d[3]] = 1;
  }
  foreach($devices as $d){
    $query = "SELECT experiment_temperature FROM sample_region_image_info WHERE sample_id = " . $d[1] . " LIMIT 1";
    $sql->get_row($query,$temperatures);
    if (sizeof($temperatures)>0)
    $device_temperatures[$d[0]] = $temperatures[0][0];
    else $device_temperatures[$d[0]] = "";
  }
}
catch (ns_exception $e){
  die($e->text);
}

display_worm_page_header("Device Temperatures for " . $experiment_name);
?>
<div align="center">
<form action="<?php
echo "device_temperatures.php?experiment_id=$experiment_id";
?>" method="post">

<table border="0" cellpadding="0" cellspacing="1" bgcolor="#000000">

  <tr>
<?php echo "<td $table_header_color align=\"center\"><b>Incubator</b></td>";?>
<td <?php echo $table_header_color?> align="center"><b>Device Name</b></td><td <?php echo $table_header_color?> align="center">Temperature</td></tr>
<?php
$row_color = 0;
	foreach($device_names as $d){
$row_color = !$row_color;
 echo "<tr>";
 echo "<td bgcolor=\"" . $table_colors[0][$row_color]. "\" valign=\"top\" align=\"center\">\n";
		echo "<center><b>" . $device_incubator[$d]. "</b></center>";
		echo "</td>";

echo "<td bgcolor=\"" . $table_colors[0][$row_color]. "\" valign=\"top\" align=\"center\">\n";
		echo "<center><b>" . $d . "</b> (" . $device_locations[$d] . ")</center>";
echo "</td><td  bgcolor=\"" . $table_colors[1][$row_color]. "\" valign=\"top\" align=\"center\">";
 if (!$simple_table)
   output_editable_field("t_" . $d, $device_temperatures[$d],TRUE);
 else echo $device_temperatures[$d];
echo "</td></tr>";
	}


?>
</table>
<?php if (!$simple_table){?>
<input type="submit" name="save" value="Save"><br><br>
	    <?php }?>
</div>
<center>
</form><?php if (!$simple_table){?>
<a href="device_temperatures.php?simple_table=1&experiment_id=<?php echo $experiment_id?>">[Simple Table]</a>
	    <?php } else {?>
<a href="device_temperatures.php?simple_table=0&experiment_id=<?php echo $experiment_id?>">[Editable Table]</a>
	  <?php }?>
</center>
<?php
display_worm_page_footer();
?>
