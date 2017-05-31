<?php
require_once("ns_processing_job.php");
require_once("worm_environment.php");

$current_device_cutoff = 60*3;

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

if (ns_param_spec($_POST,"pause")){
	$query = "UPDATE processing_job_queue SET paused = 1";
	$sql->send_query($query);

}
if (ns_param_spec($_POST,"unpause")){
	$query = "UPDATE processing_job_queue SET paused = 0";
	$sql->send_query($query);
}
if (ns_param_spec($_POST,"delete_all_jobs")){
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
	 return $res_row[1] . ' @ ' . $res_row[3] . ( (strlen($res_row[4])>0 )? (" (" . $res_row[5] . ")"): "");
}
$last_ping_index = 2;
$query = "SELECT id, name, last_ping,system_hostname,additional_host_description,system_parallel_process_id FROM hosts ORDER BY name";
$sql->get_row($query,$hosts);
$host_names = array();
for ($i = 0; $i < sizeof($hosts); $i++)
	$host_names[$hosts[$i][0]] = host_label($hosts[$i]);

$query = "SELECT host_id, node_id, current_processing_job_queue_id, current_processing_job_id, state, UNIX_TIMESTAMP(ts), current_output_event_id FROM processing_node_status ORDER BY ts DESC";
$sql->get_row($query,$processing_node_status);

$processing_jobs_referenced = array();
$experiments_referenced = array();
$samples_referenced = array();
$regions_referenced = array();
for ($i = 0; $i < sizeof($processing_node_status); $i++){
	$processing_jobs_referenced[$processing_node_status[$i][3]] = 1;
}

$query = "SELECT id, job_id,priority,experiment_id,capture_sample_id,sample_region_info_id,sample_region_id,image_id,processor_id,problem, captured_images_id,job_submission_time,paused FROM processing_job_queue ORDER BY job_submission_time";
$sql->get_row($query,$processing_job_queue);

for ($i = 0; $i < sizeof($processing_job_queue); $i++){
	$processing_jobs_referenced[$processing_job_queue[$i][1]] = 1;
	$experiments_referenced[$processing_job_queue[$i][3]] = 1;
	$samples_referenced[$processing_job_queue[$i][4]] = 1;
	$regions_referenced[$processing_job_queue[$i][5]] = 1;
}

$j = new ns_processing_job();
$query = $j->provide_query_stub() . " FROM processing_jobs WHERE ";
$query2 = "SELECT r.id,r.name, s.name,e.name FROM sample_region_image_info as r, capture_samples as s, experiments as e WHERE e.id = s.experiment_id AND s.id = r.sample_id AND (";
$query3 = "SELECT s.id,s.name,e.name FROM  capture_samples as s, experiments as e WHERE e.id = s.experiment_id AND (";
$query4 = "SELECT id, name FROM experiments WHERE ";
$first = true;
foreach($processing_jobs_referenced as $key => $value){
	if ($first){ $query .= " processing_jobs.id = " . $key; $first=false;}
	else $query .= " OR processing_jobs.id = " . $key;
}
$first = true;
foreach($regions_referenced as $key => $value){
	if ($first){ $query2 .= " r.id = " . $key; $first = false;}
	else $query2 .= " OR r.id = " . $key;
}
$query2 .= ")";
$first = true;
foreach($samples_referenced as $key => $value){
	if ($first){ $query3 .= " s.id = " . $key; $first = false;}
	else $query3 .= " OR s.id = " . $key;
}
$query3 .= ")";
$first = true;
foreach($experiments_referenced as $key => $value){
	if ($first){ $query4 .= " id = " . $key; $first = false;}
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

<span class="style1">Processing Job Queue</span>
<table class ="tw" bgcolor="#555555" cellspacing='0' cellpadding='1' width="100%"><tr><td>
<table cellspacing='0' cellpadding='3' width="100%">
<tr <?php echo $table_header_color?>><td>Job</td><td>Subject</td><td>Status</td><td>Priority</td><td>Submission Time</td></tr>
<?php
	if (sizeof($processing_job_queue) == 0)
	 echo "<tr><td valign=\"top\" bgcolor=\"".$clrs[0] . "\"> There do not appear to be any items on the job queue</td></tr>";
	 
	for ($i = 0; $i < sizeof($processing_job_queue); $i++){
	$queue = &$processing_job_queue[$i];
	$job = &$processing_jobs[$queue[1]];
    $clrs = $table_colors[$i%2];
       echo "<tr><td valign=\"top\" bgcolor=\"".$clrs[0] . "\">";
       
     
echo $job->get_concise_description();
       echo" </td>";
       echo "<td valign=\"top\" bgcolor=\"".$clrs[1] . "\">";
       if ($job->region_id != 0){
       		$r = &$regions[$job->region_id];
        	echo $r[3] ."::" .$r[2]. "::". $r[1];
       }else if ($job->sample_id != 0){
       		$r = &$samples[$job->sample_id];
        	echo $r[2] ."::". $r[1] ;
       }else if ($job->experiment_id != 0){
       		$r = &$experiment[$job->experiment_id];
        	echo $r[2]. "::". $r[1] ;
       }
       if ($queue[6] != 0)
       	echo " id: " . $queue[7];
       	if ($queue[7] != 0)
       	echo " id: " . $queue[7];
       echo " </td>";
       echo "<td valign=\"top\" bgcolor=\"".$clrs[0] . "\">";
       if ($queue[8] != 0)
       	echo "<a href=\"view_hosts_log.php?host_id=" . $queue[8] . "\">(Busy)</a>";
       if ($queue[9] != 0)
       	echo "<a href=\"view_hosts_log.php?event_id=" . $queue[9] . "\">(Problem)</a>";
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
<input name="delete_all_jobs" type="submit" value="Delete" onClick="javascript: return confirm('Are you sure you want to delete all pending processing jo\
bs?')">
</form>
<br>
<span class="style1">Processing Nodes</span>
<table class ="tw" bgcolor="#555555" cellspacing='0' cellpadding='1' width="100%"><tr><td>
<table cellspacing='0' cellpadding='3' width="100%">
<tr <?php echo $table_header_color?>><td>Host</td><td>Status</td><td>Job</td><td>Last update</td></tr>
<?php
        $cur_time = ns_current_time();
	$num_displayed = 0;
        for ($i = 0; $i < sizeof($processing_node_status); $i++){

        $p = &$processing_node_status[$i];
        if ( ($cur_time - $p[$last_ping_index]) >= $current_device_cutoff)
           continue;
	   $num_displayed++;
    $clrs = $table_colors[$i%2];
       echo "<tr><td valign=\"top\" bgcolor=\"".$clrs[0] . "\">";
       echo $cur_time . " " . $p[$last_ping_index];
       if (ns_param_spec($host_names,$p[0]))
          echo  $host_names[$p[0]];
         echo "HID" . $p[0];
         echo  " p" . $p[1] ." </td>";
       echo "<td valign=\"top\" bgcolor=\"".$clrs[1] . "\">";
       if ($p[2]!='0'){
          if ($p[3]!='0'){
             echo "Job " . $p[2] . " queue " . $p[3];
          }
       }
       echo " </td>";
       echo "<td valign=\"top\" bgcolor=\"".$clrs[1] . "\">". $p[4]  ." </td>";
       echo "<td valign=\"top\" bgcolor=\"".$clrs[1] . "\">". format_time($p[5])  ." </td>";

        }
	if ($num_displayed == 0){
	echo "<tr><td  valign=\"top\" bgcolor=\"".$clrs[0] . "\" colspan = 4> There do not appear to be any active nodes. </td></tr>";
	}
?>
</tr></table></td></tr></table>

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