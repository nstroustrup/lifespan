<?php
class ns_stat{
  public $mean,$var,$count;
};

require_once("worm_environment.php");
require_once("ns_processing_job.php");
display_worm_page_header("Cluster Throughput Statistics");

$query = "SELECT id, name, last_ping,software_version_major,software_version_minor,software_version_compile FROM hosts WHERE last_ping > " . ns_current_time() . " - 600 ORDER BY name";
$sql->get_row($query,$hosts);
$stats = array();
for ($i = 0; $i < sizeof($hosts); $i++){
  $query = "SELECT operation,mean,variance,count FROM performance_statistics WHERE host_id = " . $hosts[$i][0] . " ORDER BY operation";
  $sql->get_row($query, $stats[$i]);
 }
$host_stats = array();
$host_total_stats = array();
for ($i = 0; $i < sizeof($hosts); $i++){
  $host_stats[$i] = array();
  for ($j = 0; $j < sizeof($stats[$i]); $j++){
    $host_stats[$i][(int)$stats[$i][$j][0]] = new ns_stat;
    $host_stats[$i][(int)$stats[$i][$j][0]]->mean = $stats[$i][$j][1];
    $host_stats[$i][(int)$stats[$i][$j][0]]->var = $stats[$i][$j][2];
    $host_stats[$i][(int)$stats[$i][$j][0]]->count = $stats[$i][$j][3];
  }
 }
//var_dump($host_stats[0]);
$operations_specified = array();
foreach ($stats as $s)
	foreach ($s as $sa)
	$operations_specified[$sa[0]] = $sa[0];

$cluster_total_average = array();
$cluster_total_rate = 0;
for ($i = 0; $i < sizeof($NS_LAST_PROCESSING_JOB); $i++)
  $cluster_rate[$i] = 0;

for ($i = 0; $i < sizeof($hosts); $i++){

  foreach($host_stats[$i] as $task => $speed){
    if ($speed->count != 0)
      $cluster_rate[$task] += 1.0/$speed->count;
  }
  
  foreach($host_stats[$i] as $task => $stat){
    $host_total_stats[$i] = new ns_stat;
  }
  foreach($host_stats[$i] as $task=>$stat){
    $host_total_stats[$i]->mean+=$stat->mean;
    $host_total_stats[$i]->var+=$stat->var;
    $host_total_stats[$i]->count+=$stat->count;
    //var_dump($host_total_stats[$i]) ;
    //echo "<BR><BR><BR>";
  }
  //var_dump($host_total_stats);
  foreach($host_stats[$i] as $task => $stat)
    if ($stat->count>0){
      //  echo $host_total_average[$i] . "<BR>";
      $cluster_total_rate += 1.0/$stat->count;
    }
 }

$scaling_factor = array(30=>1000, 31=>1000);
$units = array(30=>'ms',31=>'ms');
?>

<span class="style1">Cluster Throughput Statistics</span>
<br><table bgcolor="#555555" cellspacing='0' cellpadding='1'><tr><td>
<table cellspacing='0' cellpadding='4' >
<tr <?php echo $table_header_color?>><td>Host Name</td>
<?php 
$i = 0;
foreach($operations_specified as $task){
	echo "<td bgcolor=\"$clrs[$i]\"><center>";
	if ($task >= $NS_LAST_PROCESSING_JOB)
		echo $ns_operation_state_labels[$task-$NS_LAST_PROCESSING_JOB - 1];
	else echo $ns_processing_task_labels[$task];
	$u = 's';
	if (isset($units[$task]))
		$u = $units[$task];
	if ($u != 's') echo "<i>";
	echo " ($u)";
	if ($u != 1) echo "</i>";
	echo "</center></td>";
	$i= !$i;
}
echo "</tr>";

for ($i = 0; $i < sizeof($hosts); $i++){
  $clrs = $table_colors[$i%2];
  echo "<tr><td bgcolor=\"$clrs[1]\">";
		//var_dump($hosts[$i]);
  echo $hosts[$i][1] . "</td>";
	$j = 0;
  foreach ($operations_specified as $task){
  	echo "<td bgcolor=\"$clrs[$j]\" nowrap><center>";
	if (isset($host_stats[$i][$task])){
		$s = $host_stats[$i][$task];
		$l = 1;
		if (isset($scaling_factor[$task]))
			$l = $scaling_factor[$task];
		if ($l != 1)
			echo "<i>";
		 printf("<b>%.2f</b>",$l*$s->mean);
		if ($l != 1)
			echo "</i>";
		echo "<br><font size=\"-1\">";
   		 echo " &#177;";
    		printf("%.2f",$l*sqrt($s->var));
    		echo " (" . $s->count . ")";
		echo "</font>";
		echo "</center></td>";

	}
	$j = !$j;
  }
   

  echo "</tr>";
}
?>
</table>
</table>
<?php
display_worm_page_footer();

?>