<?php
require_once("ns_processing_job.php");
require_once("worm_environment.php");
$refresh_time = @$query_string["rt"];
if ($refresh_time == '')
  $refresh_time = 0;
if ($refresh_time > 0)
  $header_text = "<meta http-equiv=\"refresh\" content=\"" . $refresh_time ."\">";

display_worm_page_header("Cluster Activity","<a href=\"view_experiments.php\">[Back to Experiment Index]</a>",FALSE,$header_text);

$current_device_cutoff = 60*10;
$host_id = @$query_string["h"];
$node_id = @$query_string["n"];
if ($node_id == '')
  $node_id = 0;
$single_device = $host_id != 0;
$query = "SELECT id, name, last_ping,software_version_major,software_version_minor,software_version_compile FROM hosts";
if ($single_device)
  $query .= " WHERE id = $host_id";
$query .=" ORDER BY name";
$sql->get_row($query,$hosts);
//var_dump($hosts);
?>
<?php
$cur_time = ns_current_time();
$k = 0;
for ($i = 0; $i < sizeof($hosts); $i++){
 $query = "SELECT DISTINCT node_id FROM host_event_log WHERE host_id = " . $hosts[$i][0];
 if ($node_id != 0)
   $query .= " AND node_id = $node_id";
$query .=" ORDER BY node_id";
  //echo $query;
  $sql->get_row($query,$node_ids);
	?><table bgcolor="#555555" cellspacing='0' cellpadding='1'><tr><td>
<table cellspacing='0' cellpadding='3' >
<tr <?php echo $table_header_color?>><td colspan=1>
<?php
	$current = $cur_time - $hosts[$i][2] < $current_device_cutoff;
	if ($current)
		echo "<b>{$hosts[$i][1]}</b> ";
	else echo $hosts[$i][1];
	echo "</td><td><div align=\"right\">";
	if (!$single_device)
	echo "<a href=\"view_cluster_status.php?h=" . $hosts[$i][0]. "&rt=" . $refresh_time . "\">(view more)</a>";
	else{
	  for ($j = 0; $j < sizeof($node_ids); $j++)
	    echo "<a href=\"view_cluster_status.php?h=" . $hosts[$i][0]. "&rt=".$refresh_time . "&n=".$node_ids[$j][0] . "\"> [" . $node_ids[$j][0] . "] </a>";
	}
	echo "</div></td></tr>";
 
  for ($j = 0; $j < sizeof($node_ids); $j++){
    $query = "SELECT time, event, processing_job_op, node_id, sub_text FROM host_event_log WHERE host_id = " . $hosts[$i][0] . " AND node_id = " . $node_ids[$j][0]. " ORDER BY time DESC";
    if ($single_device)
      $limit = 20;
    if ($node_id != 0)
      $limit = 200;
    else $limit = 4;
    $query .=" LIMIT " . $limit;
    $sql->get_row($query, $host_events);
    
    $clrs = $table_colors[$j%2];
    if ($j != 0) echo "</td></tr>";
    $node_i = $node_ids[$j][0];
    if ($node_i == 0){
      $col2 = "#CCCCCC";
      $thread_name = "main";
    }
    else{ 
      if ($j % 2 == 0)
      $col2 = "#EEEEEE";
      else $col2 = "#CCEEEE";
      $thread_name = $node_i;
    }
    $thread_name = "<a href=\"view_cluster_status.php?h=" . $hosts[$i][0]. "&rt=".$refresh_time . "&n=".$node_ids[$j][0] . "\"> [" . $thread_name . "] </a>";

    echo "<tr><td valign=\"top\" bgcolor=\"".$col2 . "\"> $thread_name </td><td bgcolor = \"" . $col2 . "\"><font size = \"-2\">";
    $col = 1-$col;
    
    for ($k = sizeof($host_events)-1; $k > 0; $k--){
      
      echo format_time($host_events[$k][0]) . ": " . $host_events[$k][1];
      	if ($host_events[$k][2] != 0)
		echo "ns_image_processing_pipeline::Calculating " . $ns_processing_task_labels[$host_events[$k][2]];
      
		echo "<br>";
      if ($host_events[$k][4]){
	echo $host_events[$k][4];
	echo "<br>";
      }
      echo "<BR>";
    }
    echo "</font></td></tr>";
  }
  
  ?>
    </table><?php
	}?>
</td></tr></table>
<?php
if ($refresh_time ==0)
  $r = 5;
else if ($refresh_time < 2)
  $r = 0;
else $r = $refresh_time-2;
	echo "<a href=\"view_cluster_status.php?h=" . $host_id. "&rt=" . $r. "\">(monitor in real time)</a>";
?>
<?php 
display_worm_page_footer();
?>