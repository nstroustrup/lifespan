<?php
require_once('worm_environment.php');
require_once('ns_dir.php');	
//require_once('ns_external_interface.php');
require_once('ns_experiment.php');
require_once('ns_processing_job.php');

$region_id=@$query_string['region_id'];
try{
  $query = "SELECT name, sample_id FROM sample_region_image_info WHERE id=$region_id";
  $sql->get_row($query,$res);
  if (sizeof($res) == 0)
    throw new ns_exception("Could not load region with id " . $region_id);
  $sample_id = $res[0][1];
  $region_name = $res[0][0];
  $query = "SELECT experiment_id, name FROM capture_samples WHERE id=$sample_id";
  $sql->get_row($query,$res);
  if (sizeof($res) == 0)
    throw new ns_exception("Could not load sample with id = $sample_id");
 
  $sample_name = $res[0][1];
  $experiment_id = $res[0][0];
  $experiment = new ns_experiment($experiment_id,'',$sql);
  
  $query = "SELECT id, capture_time FROM sample_region_images WHERE region_info_id=$region_id";

  $sql->get_row($query,$res);
  for ($i = 0; $i < sizeof($res); $i++)
    $capture_times[$res[$i][0]] = $res[$i][1];

  $query = "SELECT time, region_id_short_1, region_id_short_2, region_id_long, region_id_previous_short, region_id_previous_long,calculated FROM worm_movement WHERE region_info_id=$region_id ORDER BY TIME ASC";
  $sql->get_row($query,$movement_records);


}
catch (ns_exception $e){
	die($e->text);
}
if ($refresh_page === TRUE){
  header("Location: manage_samples.php?experiment_id=$experiment_id\n\n");
  die("");

 }
display_worm_page_header($experiment->name . " samples");

function out_time($var){
  global $capture_times;
  if ($var == "0")
    echo "N/A";
  else echo format_time($capture_times[$var]);
}
function out_delta($var1,$var2){
  global $capture_times;
  if ($var1=="0" || $var2== "0")
    echo "N/A";
  else{
    $t1 = $capture_times[$var1];
    $t2 = $capture_times[$var2];
    $d = $t2-$t1;
    
    $s = $d%60;
    $m = floor(($d-$s)/60)%60;
    $h = floor(($d-$s-60*$m)/3600);
    
    //echo $t2-$t1 . " ";

        if ($h != 0)
    echo $h . "h";
    if ($m != 0)
      echo $m . "m";
    if ($s != 0)
     echo $s . "s";

  }

}
?>
<!--
<span class="style1">Movement Management</span>
<table border="0" cellpadding="0" cellspacing="1" bgcolor="#000000"><tr><td>
<table border="0" cellpadding="4" cellspacing="0"><tr <?php echo $table_header_color?> ><td >Times<td>Delta</td><td><Calculated</td><td>&nbsp;</td></tr>
<tr><td bgcolor="<?php echo $clrs[0]?>">

</td>
  </tr>
  <tr>
</table>
</table><br>
--!>
<span class="style1">Movement Info</span>
<table border="0" cellpadding="0" cellspacing="1" bgcolor="#000000"><tr><td>
<table border="0" cellpadding="4" cellspacing="0"><tr <?php echo $table_header_color?> ><td >Times<td>Delta</td><td><Calculated</td><td>&nbsp;</td></tr>
<?php
	for ($i = 0; $i < sizeof($movement_records); $i++){
		$clrs = $table_colors[$i%2];

		echo "<tr><td valign=\"top\" bgcolor=\"$clrs[0]\">\n";  
  
		out_time($movement_records[$i][1]);
		echo "<BR>";
		out_time($movement_records[$i][2]);
		echo "<BR>";
		out_time($movement_records[$i][3]);
		echo "<BR>";
		/*out_time($movement_records[$i][4]);
		echo  "</td>\n<td bgcolor=\"$clrs[1]\">\n";
		out_time($movement_records[$i][5]);*/
		echo  "</td>\n<td bgcolor=\"$clrs[1]\">\n";
		out_delta($movement_records[$i][1],$movement_records[$i][2]);
		echo  "<BR>";
		out_delta($movement_records[$i][1],$movement_records[$i][3]);
		echo "</td>\n<td bgcolor=\"$clrs[0]\">\n";
		if ($movement_records[$i][6])
		  echo "Yes";
		else echo "No";
		echo "</td>\n<td bgcolor=\"$clrs[1]\">\n";
		echo "<a href=\"view_processing_job.php?job_id={$experiment_jobs[$i]->id}\">[Edit]</a>";
		echo "\n</td></tr>\n";
	}
?></td>
  </tr>
  <tr>
</table>
</table><br>
<?php
display_worm_page_footer();
?>
