<?php
require_once("ns_processing_job.php");
require_once("worm_environment.php");
display_worm_page_header("Cluster Activity");

$current_device_cutoff = 60*10;


$query = "SELECT id, name, last_ping,software_version_major,software_version_minor,software_version_compile FROM hosts ORDER BY name";
$sql->get_row($query,$hosts);
for ($i = 0; $i < sizeof($hosts); $i++){
  $query = "SELECT time, event, processing_job_op FROM host_event_log WHERE host_id = " . $hosts[$i][0] . " ORDER BY time DESC LIMIT 2";
  $sql->get_row($query, $host_events[$i]);
 }

?>
<span class="style1">Capture/Processing Nodes and Imaging Devices</span>
<form action="view_hosts_and_devices.php" method="post">
<table bgcolor="#555555" cellspacing='0' cellpadding='1'><tr><td>
<table cellspacing='0' cellpadding='3' >
<tr <?php echo $table_header_color?>><td>Host Name</td>
<td>Last Action</td>
<td>Last Ping</td>
<?php
$cur_time = ns_current_time();
$k = 0;
for ($i = 0; $i < sizeof($hosts); $i++){
	$clrs = $table_colors[$i%2];
	$edit_host = ($host_id == $hosts[$i][0]);
	echo "<tr><td bgcolor=\"$clrs[0]\">";
	$current = $cur_time - $hosts[$i][2] < $current_device_cutoff;
	if ($current)
		echo "<b>{$hosts[$i][1]}</b>";
	else echo $hosts[$i][1];
	echo "({$hosts[$i][3]}.{$hosts[$i][4]}.{$hosts[$i][5]})</td>";
	echo "<td bgcolor=\"$clrs[1]\">";
	echo format_time($host_events[$i][0][0]) . ":" . $host_events[$i][0][1];
	if ($host_events[$i][0][2] != 0)
	  echo "ns_image_processing_pipeline::Calculating" . $ns_processing_task_labels[$host_events[$i][0][2]];
	echo "</td><td bgcolor=\"$clrs[0]\">";
	echo "<a href=\"view_hosts_log.php?host_id={$hosts[$i][0]}&limit=50\">";
	echo format_time($hosts[$i][2]) . "</a>";
	echo "</td></tr>";
	//	$clrs = $table_colors[($k+1)%2];
	echo "<tr><td bgcolor=\"$clrs[0]\">";
	echo "&nbsp;";
	echo "</td><td bgcolor=\"$clrs[1]\">";
	echo format_time($host_events[$i][1][0]) . ":" . $host_events[$i][1][1];
	echo "</td><td bgcolor=\"$clrs[0]\">";
	echo "&nbsp;";
	echo "</td></tr>";
}
?>
</table></td></tr></table></form>

<?php 
display_worm_page_footer();
?>