<?php
require_once("ns_processing_job.php");
require_once("worm_environment.php");
display_worm_page_header("Hosts and Devices", "<a href=\"view_hosts_and_devices.php\">[Back to Hosts and Devices]</a>");

$current_device_cutoff = 60*10;

$host_id = @$query_string['host_id'];
$event_id = @$query_string['event_id'];
$region_id = @$query_string['region_id'];
$sample_id = @$query_string['sample_id'];
$experiment_id = @$query_string['experiment_id'];
$show_errors = @$query_string['show_errors'];

$start_time = @$query_string['start_time'];
$limit = @$query_string['limit'];

if (ns_param_spec($_POST,'jump_to_time')){
  $query = "SELECT  UNIX_TIMESTAMP('"
    . $_POST['jump_year'] . "-" . $_POST['jump_month'] . "-" . $_POST['jump_day'] ." 23:59:59Ab')";

  $sql->get_row($query,$res);
  $start_time = $res[0][0];
  header("Location: view_hosts_log.php?host_id=$host_id&start_time=" . $start_time . "&limit=" . $limit .  "\n\n");
  die("");
 }

if ($start_time == 0)
  $start_time = ns_current_time();

ns_expand_unix_timestamp($start_time,$cur_min,$cur_hour,$cur_day,$cur_month,$cur_year);

if (ns_param_spec($query_string,'delete_before') && $query_string['delete_before'] != 0){
  $query = "DELETE FROM host_event_log WHERE host_id='$host_id' AND time <= " . $query_string['delete_before'];
  $sql->send_query($query);
 }

$samples = array();
if ($experiment_id != ''){
  $query = "SELECT id FROM capture_samples WHERE experiment_id = $experiment_id";
  $sql->get_row($query,$res);
  for ($i = 0; $i < sizeof($res); $i++)
    array_push($samples,$res[$i][0]);
}
if ($sample_id != '' && $sample_id != "all")
  array_push($samples,$sample_id);

$regions = array();
$output_region_problems = false;
if(sizeof($samples) != 0){
  if ($region_id == "all"){
    $output_region_problems = true;
    for ($i = 0; $i < sizeof($samples); $i++){
      $query = "SELECT id FROM sample_region_image_info WHERE sample_id = " . $samples[$i];
      $sql->get_row($query,$res);
      for ($j = 0; $j < sizeof($res); $j++)
	array_push($regions,$res[$j][0]);
    }
  }
 }
if ($region_id != '' && $region_id != 'all')
  array_push($regions,$region_id);

$events=array();

if ($host_id != '' || $event_id != ''){
  $query = "SELECT event, time, processing_job_op, host_id  FROM host_event_log";
  if ($host_id != '')
    $query.=" WHERE host_id=$host_id";
  else
    $query.=" WHERE id=" . $event_id;
  if ($show_errors)
    $query .= " AND error=1";
  if ($start_time > 0)
   $query .= " AND time < " . $start_time;
  $query.=" ORDER BY time DESC";
 if ($limit != '')
    $query .=" LIMIT " . $limit;
 // die($query);
  $sql->get_row($query,$events);
}
 else if ($output_region_problems || $region_id != ''){
   for ($i = 0; $i < sizeof($regions); $i++){
     $query = "SELECT e.event,e.time,e.processing_job_op,e.host_id FROM host_event_log as e, sample_region_images as s WHERE s.region_info_id = " . $regions[$i] . " AND s.problem != 0 AND s.problem=e.id";
     $sql->get_row($query,$res);
     for ($j = 0; $j < sizeof($res); $j++)
       array_push($events,$res[$j]);
     //die($query . "<BR>" . $res);
   }
 }
 else{
   $query = "SELECT e.event, e.time, e.processing_job_op,e.host_id FROM host_event_log as e, captured_images as i WHERE i.problem != 0 AND i.problem=e.id";
   $sql->get_row($query,$res);
   for ($j = 0; $j < sizeof($res); $j++)
     array_push($events,$res[$j]);
 }




$host_names = array();
$query = "SELECT id, name FROM hosts";
$sql->get_row($query,$host_names_raw);
for ($i = 0; $i < sizeof($host_names_raw); $i++)
  $host_names[$host_names_raw[$i][0]] = $host_names_raw[$i][1];


?>
<span class="style1">Event log <?php
if ($host_id  == 0) echo " for all hosts ";
 else {
   echo " for host " . $host_names[$host_id];
 }
?></span><br><br>

<form action="view_hosts_log.php?host_id=<?php echo $host_id;?>&limit=<?php echo $limit?>&show_errors=<?php echo $show_errors?>" method="post">
<?php
if (!$show_errors)
  echo "<a href=\"view_hosts_log.php?host_id=$host_id&limit=$limit&show_errors=1\">[Show Only Errors]</a>";
 else
   echo "<a href=\"view_hosts_log.php?host_id=$host_id&limit=$limit&show_errors=0\">[Show All Events]</a>";
echo "<BR><br>";
echo "Jump to date ";
output_editable_field("jump_month",$cur_month,TRUE,2);
echo "/";
output_editable_field("jump_day",$cur_day,TRUE,2);
echo "/";
output_editable_field("jump_year",$cur_year,TRUE,4);
//echo " $cur_hour:$cur_min:$cur_sec";
echo "<input type=\"submit\" name=\"jump_to_time\" value=\"Go\">";
?>
</form><br>

<table bgcolor="#555555" cellspacing='0' cellpadding='1'><tr><td>
<table cellspacing='0' cellpadding='3' >
<tr <?php echo $table_header_color?>><td>Time</td><td>Host</td>
   <td>Event</td><td>&nbsp;</td></tr>
<?php
if (sizeof($events) == 0)
echo "<tr><td bgcolor=\"{$table_colors[0][0]}\" colspan=\"3\">(No events in log)</td></tr>";
for ($i = 0; $i < sizeof($events); $i++){
	$clrs = $table_colors[$i%2];
	echo "<tr><td bgcolor=\"$clrs[1]\">";
	echo "<a href=\"view_hosts_log.php?host_id=$host_id&start_time=" . $events[$i][1] . "&limit=" . $limit .  "\">";
	echo format_time($events[$i][1]);
	echo "</a>";
	echo "</td><td bgcolor=\"$clrs[0]\">";
	echo $host_names[$events[$i][3]];
	echo "</td><td bgcolor=\"$clrs[1]\">";
	echo $events[$i][0];
	if ($events[$i][2] != 0)
	  echo "ns_image_processing_pipeline:: Calculating " .  $ns_processing_task_labels[$events[$i][2]];
	echo "</td><td bgcolor=\"$clrs[0]\">";
	echo "<a href=\"view_hosts_log.php?host_id=$host_id&delete_before=" . $events[$i][1] . "\">[del]</a>";
	echo"</td></tr>";
}
?>
</table></td></tr></table>

<?php

if ($limit != ''){
  echo "<div align=\"right\">";
  echo "<a href=\"view_hosts_log.php?host_id=$host_id&limit=";
  echo ($limit + 100);
  echo "\">[View More]</a>";
  echo "<a href=\"view_hosts_log.php?host_id=$host_id\">[View All]</a>";
  echo "</div>";
 }

?>

<br>


<?php
display_worm_page_footer();
?>