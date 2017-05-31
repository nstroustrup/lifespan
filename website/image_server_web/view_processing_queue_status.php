<?php
require_once("ns_processing_job.php");
require_once("worm_environment.php");

$refresh_time = @$query_string["rt"];
if ($refresh_time == '')
  $refresh_time = 0;
$header_text = "";
if ($refresh_time > 0)
  $header_text = "<meta http-equiv=\"refresh\" content=\"" . $refresh_time ."\">";
$header_text .= "<style type=\"text/css\">

.ns_scroll{
width:100%;
border:1px solid #ccc;
font:9px Courier, Georgia, Garamond, Serif;
overflow:scroll;
line-height: 1.5;
background-color:white;
white-space: normal;
word-wrap: break-word;
}

.rw{
  white-space: -o-pre-wrap;
    word-wrap: break-word;
    white-space: pre-wrap;
    white-space: -moz-pre-wrap;
    white-space: -pre-wrap;
}

.tw{
  table-layout: fixed;
  width: 100%
}</style>";
$header_text .="<script type=\"text/javascript\">
function gotoBottom(id){
   var div = document.getElementById(id);
   div.scrollTop = div.scrollHeight;
}</script>";

if (ns_param_spec($_POST","pause")){
	$query = "UPDATE processing_job_queue SET paused = 1";
	$sql->send_query($query);

}
if (ns_param_spec($_POST","unpause")){
	$query = "UPDATE processing_job_queue SET paused = 0";
	$sql->send_query($query);
}
if (ns_param_spec($_POST","delete_all_jobs")){
	$query = "DELETE FROM processing_job_queue";
	$sql->send_query($query);
	$query = "DELETE FROM processing_jobs";
	$sql->send_query($query);
}

display_worm_page_header("Processing Queue Status","<a href=\"view_experiments.php\">[Back to Experiment Index]</a>",FALSE,$header_text);

$current_device_cutoff = 60*10;
$host_id = @$query_string["h"];
if (array_key_exists("n",$query_string))
  $node_id = $query_string["n"];
else
  $node_id = -1;
$single_device = $host_id != 0;
function host_label($res_row){
	 return $res_row[1] . ' @ ' . $res_row[3] . ( (strlen($res_row[4])>0 )? (" (" . $res_row[5] . ":" . $res_row[6].")"): "");
}

$query = "SELECT id, name, last_ping,system_hostname,additional_host_description,system_parallel_process_id FROM hosts ORDER BY name";
$sql->get_row($query,$hosts);
$host_names = array;
for ($i = 0; $i < sizeof($hosts); $i++)
	$host_names[$res[$i][0]] = host_label($res[$i]);


$query = "SELECT host_id, node_id, current_processing_job_queue_id, current_processing_job_id, state, ts, current_output_event_id FROM processing_node_status ORDER BY ts";
$sql->get_row($query,$processing_node_status);

$processing_jobs_referenced = array();
$experiments_referenced = array();
$samples_referenced = array();
$regions_referenced = array();
for ($i = 0; $i < sizeof($processing_node_status); $i++){
	$processing_jobs_referenced[$processing_node_status[$i][3]] = 1;
}

$query = "SELECT id, job_id,priority,experiment_id,capture_sample_id,sample_region_info_id,sample_region_id,image_id,processor_id,problem, captured_image_id,job_submission_time,paused FROM processing_job_queue ORDER BY job_submission_time";
$sql->get_row($query,$processing_job_queue);

for ($i = 0; $i < sizeof($processing_job_queue); $i++){
	$processing_jobs_referenced[$processing_job_queue[$i][1]] = 1;
	$experiments_referenced[$processing_job_queue[$i][3]] = 1;
	$samples_referenced[$processing_job_queue[$i][4]] = 1;
	$regions_referenced[$processing_job_queue[$i][5]] = 1;
}

$j = new ns_processing_job() . " WHERE ";
$query = $j->provide_query_stub();
$query2 = "SELECT r.id,r.name, s.name,e.name FROM sample_region_image_info as r, capture_samples as s, experiments as e WHERE e.id = s.experiment_id AND s.id = r.sample_id AND (";
$query3 = "SELECT s.id,s.name,e.name FROM  capture_samples as s, experiments as e WHERE e.id = s.experiment_id AND (";
$query4 = "SELECT id, name FROM experiments WHERE ";
$first = false;
foreach($processing_jobs_referenced as $key => $value){
	if ($first) $query .= " processing_jobs.id = " . $key;
	else $query .= " OR processing_jobs.id = " . $key;
}
foreach($regions_referenced as $key => $value){
	if ($first) $query2 .= " r.id = " . $key;
	else $query2 .= " OR r.id = " . $key;
}
foreach($samples_referenced as $key => $value){
	if ($first) $query3 .= " s.id = " . $key;
	else $query3 .= " OR s.id = " . $key;
}
foreach($experiments_referenced as $key => $value){
	if ($first) $query4 .= " id = " . $key;
	else $query4 .= " OR id = " . $key;
}

$sql->get_row($query,$processing_job_data);
$sql->get_row($query2,$region_data);
$sql->get_row($query3,$sample_data);
$sql->get_row($query4,$experiment_data);

$processing_jobs = array();
for ($i = 0; $i < sizeof($processing_job_data); $i++){
	$processing_jobs[$processing_job_data[$i][0]] = new ns_processing_job();
	$processing_jobs[$processing_job_data[$i][0]]->load_from_result($processing_job_data[$i]);
}
$experiments = array();
for ($i = 0; $i < sizeof($experiment_data); $i++)
	$experiments[$experiment_data[$i][0]] = $experiment_data[$i];
$regions = array();
for ($i = 0; $i < sizeof($region_data); $i++)
	$regions[$region_data[$i][0]] = $region_data[$i];

$samples = array();
for ($i = 0; $i < sizeof($sample_data); $i++)
	$samples[$sample_data[$i][0]] = $sample_data[$i];


?>
<table class ="tw" bgcolor="#555555" cellspacing='0' cellpadding='1' width="100%"><tr><td>
<table cellspacing='0' cellpadding='3' width="100%">
<tr <?php echo $table_header_color?>><td>Host</td><td>Status</td><td>Job</td><td>Last update</td></tr>
<?php
	for ($i = 0; $i < sizeof($processing_node_status); $i++){
    $clrs = $table_colors[$i%2];
       echo "<tr><td valign=\"top\" bgcolor=\"".$clrs[0] . "\">" .$host_names[$processing_node_status[$i][0]] . " p" . $processing_node_status[$i][0]] ." </td>";
       echo "<td valign=\"top\" bgcolor=\"".$clrs[1] . "\">" .$host_names[$processing_node_status[$i][2]] . $host_names[$processing_node_status[$i][3]] " </td>";
       echo "<td valign=\"top\" bgcolor=\"".$clrs[1] . "\">". $host_names[$processing_node_status[$i][4]]  " </td>";
       echo "<td valign=\"top\" bgcolor=\"".$clrs[1] . "\">". $host_names[$processing_node_status[$i][5]]  " </td>";

	}
?>
</tr></table></td></tr></table>

<tr <?php echo $table_header_color?>><td>Job</td><td>Subject</td><td>Status</td><td>Priority</td><td>Submission Time</td></tr>
<?php

	for ($i = 0; $i < sizeof($processing_job_queue); $i++){
	$queue = &processing_job_queue[$i][1];
	$job = &$processing_jobs[$queue[$i][1]];
    $clrs = $table_colors[$i%2];
       echo "<tr><td valign=\"top\" bgcolor=\"".$clrs[0] . "\">" .$job->get_concise_description() ." </td>";
       echo "<td valign=\"top\" bgcolor=\"".$clrs[1] . "\">";
       if ($job->region_id != 0){
       		$r = &$regions[$job->region_id];
        	echo $r[3] "::" $r[2] "::" $r[1]
       }else if ($job->sample_id != 0){
       		$r = &$samples[$job->sample_id];
        	echo $r[2] "::" $r[1] ;
       }else if ($job->experiment_id != 0){
       		$r = &$experiment[$job->experiment_id];
        	echo $r[2] "::" $r[1] ;
       }
       if ($queue[6] != 0)
       	echo " id: " . $queue[7];
       	if ($queue[7] != 0)
       	echo " id: " . $queue[7];
       echo " </td>";
       echo "<td valign=\"top\" bgcolor=\"".$clrs[0] . "\">";
       if ($queue[8] != 0)
       	echo "<a href=\"view_hosts_and_devices.php?host_id=" . $queue[8] . "\>(Busy)</a>";
       if ($queue[9] != 0)
       	echo "<a href=\"view_host_event_log.php?id=" . $queue[9] . "\>(Problem)</a>";
       if ($queue[12])
       	echo "(Paused)";
       echo " </td>";
       echo "<td valign=\"top\" bgcolor=\"".$clrs[1] . "\">";
       echo $queue[2];
       echo " </td>";
       echo "<td valign=\"top\" bgcolor=\"".$clrs[0] . "\">";
       echo format_time($queue[11]);
       echo " </td>";

	}
?>
</tr></table></td></tr></table>

<form action="view_processing_queue_status.php" method="post">
<input name="pause" type="submit" value="Pause All" >
<input name="unpause" type="submit" value="Un-Pause All" >
<input name="delete_all_jobs" type="submit" value="Delete" onClick="javascript: return confirm('Are you sure you want to delete all pending processing jobs?')">
</form>
<?php
if ($refresh_time ==0)
  $r = 5;
else if ($refresh_time < 2)
  $r = 0;
else $r = $refresh_time-2;
	echo "<a href=\"view_cluster_status.php?h=" . $host_id. "&rt=" . $r. "\">(monitor in real time)</a>";
?>
   <script type="text/javascript">

    window.onresize = resize;

    function resize(){
      for (var i = 0; i < <?php echo $sid?>; i++)
	gotoBottom('foo' + i.toString());
    }
    </script>
<?php
display_worm_page_footer();
?>