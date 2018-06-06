<?php
require_once("ns_processing_job.php");
require_once("worm_environment.php");
display_worm_page_header("View Alerts");

$show_acknowledged = 0;
$show_acknowledged = @$query_string['show_acknowledged'];

if (!array_key_exists('limit',$query_string))
$limit = 25;
else
$limit = $query_string['limit'];

if (array_key_exists("acknowledge_all",$_POST)){
 $query = "UPDATE alerts SET acknowledged=UNIX_TIMESTAMP(NOW()) WHERE acknowledged=0";
  $sql->send_query($query);

}
if (array_key_exists("acknowledge_alert",$query_string)){
   $query = "UPDATE alerts SET acknowledged=UNIX_TIMESTAMP(NOW()) WHERE id = " . $query_string["acknowledge_alert"];
  $sql->send_query($query);
}


$query = "SELECT host_id, time, text, recipients,acknowledged,id,critical_alert,detailed_text FROM alerts ";
if ($show_acknowledged!=1)
   $query.=" WHERE acknowledged=0";
$query .=" ORDER BY TIME DESC";
if ($limit != '' && $limit != 0)
  $query.= " LIMIT " . (string)((int)$limit);   
$sql->get_row($query,$events);

$host_names = array();
$query = "SELECT id, name, system_hostname,additional_host_description,system_parallel_process_id FROM hosts";
$sql->get_row($query,$host_names_raw);
for ($i = 0; $i < sizeof($host_names_raw); $i++)
  $host_names[$host_names_raw[$i][0]] = $host_names_raw[$i][1] . '@' . $host_names_raw[$i][2] . " ( " . $host_names_raw[$i][3] . " ) p" . $host_names_raw[$i][4];


?>
<span class="style1">Alerts</span><br><br>

<form action="view_alerts.php?limit=<?php echo $limit;?>&show_acknowledged=<?php echo $show_acknowledged?>" method="post">
<?php
if (!$show_acknowledged)
  echo "<a href=\"view_alerts.php?limit=$limit&show_acknowledged=1\">[Show Acknowledged Alerts]</a>";
 else
   echo "<a href=\"view_alerts.php?limit=$limit&show_acknowledged=0\">[Hide Acknowledged Alerts]</a>";
echo "<BR><br>";
?>

<table bgcolor="#555555" cellspacing='0' cellpadding='1'><tr><td>
<table cellspacing='0' cellpadding='3' >
<tr <?php echo $table_header_color?>><td nowrap>Time</td><td>Host</td>
   <td>Text</td><td>Details</td><td>Action</td></tr>
<?php
if (sizeof($events) == 0)
echo "<tr><td bgcolor=\"{$table_colors[0][0]}\" colspan=\"5\">(No alerts)</td></tr>";
for ($i = 0; $i < sizeof($events); $i++){
	$clrs = $table_colors[$i%2];
	echo "<tr><td bgcolor=\"$clrs[1]\" nowrap>";
	echo "<a href=\"view_hosts_log.php?host_id=" .$events[$i][0] ."\">";
	echo format_time($events[$i][1]);
	echo "</a>";
	echo "</td><td bgcolor=\"$clrs[0]\">";
	if (array_key_exists($events[$i][0],$host_names))
	echo $host_names[$events[$i][0]];
	else echo $events[$i][0];
	echo "</td><td bgcolor=\"$clrs[1]\">";
	echo $events[$i][2];  
	echo "</td><td bgcolor=\"$clrs[0]\">";
	echo "<font size=\"-2\">".$events[$i][7]."</font>";
	echo "</td><td bgcolor=\"$clrs[1]\">";
	echo "<a href=\"view_alerts.php?acknowledge_alert=" . $events[$i][5] . "\">[Acknowledge]</a>";
	echo"</td></tr>";
}
?>
</table></td></tr></table>
<?php

if ($limit==0){
  $l=25;
  $text = "Show fewer alerts";
}
else if ($limit < 400){
  $l = 2*$limit;
  $text = "Show more alerts";
}
else{
  $l = 0;
  $text = "Show all alerts";
}
if (sizeof($events) > 0){
  echo "<div align=\"right\">";
  echo "<a href=\"view_alerts.php?show_acknowledged=$show_acknowledged&limit=$l";
  echo "\">[$text]</a>";
?>
<input type="submit" name="acknowledge_all" value = "Acknowledge all alerts" onClick="javascript:return confirm('Are you sure you wish to acknowledge all alerts?')">
<?php } ?>
</form>
</div>
<br>


<?php
display_worm_page_footer();
?>