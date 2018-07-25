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

if (ns_param_spec($query_string,"q_highlight_id")){
	$q_highlight_id = $query_string['q_highlight_id'];
}else $q_highlight_id = 0;
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
if (ns_param_spec($_POST,"delete_problem_jobs")){
   $query = "DELETE FROM processing_job_queue WHERE problem != 0";
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
	 return $res_row[1] . ' @ ' . $res_row[3] . " " . ( (strlen($res_row[4])>0 )? (" (" . $res_row[5] . ")"): "");
}
$last_ping_index = 2;

$query = "SHOW TABLE STATUS WHERE name='processing_job_queue'";
$sql->get_row($query,$queue_size);
$queue_size = $queue_size[0][4];

$query = "SELECT id, name, last_ping,system_hostname,additional_host_description,system_parallel_process_id FROM hosts ORDER BY name";
$sql->get_row($query,$hosts);
$host_names = array();
$host_info = array();
for ($i = 0; $i < sizeof($hosts); $i++){
	$host_names[$hosts[$i][0]] = host_label($hosts[$i]);
	$host_info[$hosts[$i][0]] = $hosts[$i];
}
//echo "node status";
//ob_flush();
flush();
$query = "SELECT host_id, node_id, current_processing_job_queue_id, current_processing_job_id, state, UNIX_TIMESTAMP(ts), current_output_event_id FROM processing_node_status ORDER BY ts DESC";
$sql->get_row($query,$processing_node_status);

$processing_jobs_referenced = array();
$experiments_referenced = array();
$samples_referenced = array();
$regions_referenced = array();
for ($i = 0; $i < sizeof($processing_node_status); $i++){
	$processing_jobs_referenced[intval($processing_node_status[$i][3])] = 1;
}
//echo "loading queue...";
//ob_flush();
//flush();
if (!array_key_exists("queue_lim",$query_string))
   $queue_limit = 20;
else $queue_limit = $query_string["queue_lim"];

$query = "SELECT id, job_id,priority,experiment_id,capture_sample_id,sample_region_info_id,sample_region_id,image_id,processor_id,problem, captured_images_id,job_submission_time,paused FROM processing_job_queue ORDER BY priority DESC,id DESC LIMIT $queue_limit";
$sql->get_row($query,$processing_job_queue);
//echo "more info";
//ob_flush;
//flush();
$region_images_referenced = array();
for ($i = 0; $i < sizeof($processing_job_queue); $i++){
	$processing_jobs_referenced[intval($processing_job_queue[$i][1])] = 1;
	$experiments_referenced[intval($processing_job_queue[$i][3])] = 1;
	$samples_referenced[intval($processing_job_queue[$i][4])] = 1;
	$regions_referenced[intval($processing_job_queue[$i][5])] = 1;
	if ($processing_job_queue[$i][6] != '0')
	   $region_images_referenced[intval($processing_job_queue[$i][6])] = 0;
}

if (sizeof($region_images_referenced)>0){
   $first = true;

   $query = "SELECT id, capture_time FROM sample_region_images WHERE ";

   foreach ($region_images_referenced as $id => $val){
   	   if (!$first)
	      $query .= " OR ";
	   else $first = false;
	   $query .= "id = " . $id;
   }
   $sql->get_row($query,$res);
   for ($i = 0; $i < sizeof($res); $i++)
       $region_image_data[intval($res[$i][0])] = $res[$i];
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
if (sizeof($processing_jobs_referenced)>0)
$sql->get_row($query,$processing_job_data);
else $processing_job_data = array();
if (sizeof($regions_referenced) > 0)
$sql->get_row($query2,$region_data);
else $region_data = array();
if (sizeof($samples_referenced)>0)
$sql->get_row($query3,$sample_data);
else $sample_data = array();
if (sizeof($experiments_referenced)>0)
$sql->get_row($query4,$experiment_data);
else $experiment_data = array();
$processing_jobs = array();
for ($i = 0; $i < sizeof($processing_job_data); $i++){
	$processing_jobs[intval($processing_job_data[$i][0])] = new ns_processing_job();
	$processing_jobs[intval($processing_job_data[$i][0])]->load_from_result($processing_job_data[$i]);
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

$clrs = $table_colors[0];
?>

<span class="style1">Processing Job Queue</span>
<table class ="tw" bgcolor="#555555" cellspacing='0' cellpadding='1' width="100%"><tr><td>
<table cellspacing='0' cellpadding='3' width="100%">
<tr <?php echo $table_header_color?>><td>Job</td><td>Subject</td><td>Status</td><td>Priority</td><td>Submission Time</td></tr>
<?php
	$cur_time = ns_current_time();
	if (sizeof($processing_job_queue) == 0){
	 echo "<tr><td valign=\"top\" bgcolor=\"".$clrs[0] . "\"> There do not appear to be any items on the job queue</td></tr>";
}

	for ($i = 0; $i < sizeof($processing_job_queue); $i++){
	
	$queue = &$processing_job_queue[$i];
	$job_is_missing = false;
	if (!ns_param_spec($processing_jobs,$queue[1]))
	   $job_is_missing = true;
	else  $job = &$processing_jobs[$queue[1]];
	
	$h = $q_highlight_id == $queue[0];
	$job_is_busy = $queue[8] != 0;
	$job_is_problem = $queue[9] != 0;
	$job_is_paused = $queue[12] != 0;
	$host_id = $queue[8];

	$host_is_offline = $host_id!=0 && (!ns_param_spec($host_info,$host_id) ||
			    $cur_time - $host_info[$host_id][$last_ping_index] >= $current_device_cutoff);
	$idle_job = $job_is_problem || $job_is_paused || $host_is_offline || $job_is_missing;

    	$clrs = $table_colors[$i%2];
       echo "<tr><td valign=\"top\" bgcolor=\"".$clrs[0] . "\">";
       $ft1 = $ft2 = "";
       if ($h){ $ft1 = "<b>"; $ft2 = "</b>";}
       if($idle_job){
	$ft1.="<font color=\"#666666\"><i>";
	$ft2.="</i></font>";
       }  
     
       echo $ft1;
       if (!$job_is_missing)
       echo $job->get_concise_description();
       else "(Deleted)";
       echo $ft2;
       echo" </td>";
       echo "<td valign=\"top\" bgcolor=\"".$clrs[1] . "\">";
       echo $ft1;
       if (!$job_is_missing){
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
       }
       if ($queue[6] != '0'){
       if (ns_param_spec($region_image_data,intval($queue[6])))
       	  echo " " . format_time($region_image_data[intval($queue[6])][1]);
       	else echo " id: " . $queue[6];
       }
       if ($queue[7] != '0')
       	   echo " id: " . $queue[7];
       echo $ft2;
       echo " </td>";
       echo "<td valign=\"top\" bgcolor=\"".$clrs[0] . "\">";
       echo $ft1;
       if ($job_is_busy)
       	echo "<a href=\"view_hosts_log.php?host_id=" . $host_id . "\">(Busy)</a>";
       if ($job_is_problem)
       	echo "<a href=\"view_hosts_log.php?event_id=" . $queue[9] . "\">(Problem)</a>";
       if ($job_is_paused)
       	echo "(Paused)";
	if ($job_is_missing){
	echo "(Job Deleted)";
	}
	echo $ft2;
       echo " </td>";
       echo "<td valign=\"top\" bgcolor=\"".$clrs[1] . "\">$ft1";
       echo $queue[2];
       echo " $ft2</td>";
       echo "<td valign=\"top\" bgcolor=\"".$clrs[0] . "\">$ft1";
       if (!$job_is_missing)
       echo format_time($job->time_submitted);
       echo " $ft2</td>";
	}
?>
<tr <?php echo $table_header_color?>><td colspan=5>
<div align="right"><?php
echo "Showing " . $queue_limit . " of " . $queue_size . " items. ";
if (sizeof($processing_job_queue) >= $queue_limit) echo "<a href=\"view_processing_queue_status.php?queue_lim=" . 5*$queue_limit . "\">[View more]</a>";
?></div></td></tr>
</tr></table></td></tr></table>
<form action="view_processing_queue_status.php" method="post">
<input name="delete_problem_jobs" type="submit" value="Delete problematic jobs">
<input name="pause" type="submit" value="Pause All Jobs" >
<input name="unpause" type="submit" value="Resume All Jobs" >
<input name="delete_all_jobs" type="submit" value="Delete all Jobs" onClick="javascript: return confirm('Are you sure you want to delete all pending processing jo\
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
	$node_last_ping = $p[5];
	$host_last_ping = ns_param_spec($host_info,$p[0])?$host_info[$p[0]][$last_ping_index]:0;
	$most_recent_ping = max($node_last_ping,$host_last_ping);
	//echo ($cur_time - $most_recent_ping) . "<br>";
        if ( ($cur_time - $most_recent_ping) >= $current_device_cutoff)
           continue;
	   $num_displayed++;
    $clrs = $table_colors[$i%2];
       echo "<tr><td valign=\"top\" bgcolor=\"".$clrs[0] . "\" >";
       //echo $cur_time . " " . $p[$last_ping_index];
       if (ns_param_spec($host_names,$p[0]))
          echo  $host_names[$p[0]];
         else echo "HID" . $p[0];
         echo  " p" . $p[1] ." </td>";
       echo "<td valign=\"top\" bgcolor=\"".$clrs[1] . "\" >";
       if ($p[2]!='0'){
          if ($p[3]!='0'){
	     if (ns_param_spec($processing_jobs,intval($p[3]))){
	     	     echo "<a href=\"view_processing_queue_status.php?q_highlight_id=$p[2]\">";
             	     echo $processing_jobs[ intval($p[3])]->get_concise_description(); 
	     	     echo "</a>";
	     }else echo "Job information was deleted";
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
	echo "<a href=\"view_processing_queue_status.php?rt=" . $r. "\">(monitor in real time)</a>";
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