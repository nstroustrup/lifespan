<?php
require_once("ns_processing_job.php");
require_once("worm_environment.php");

function output_wrap($s,$interval=20){
  $p=1;
  $lc = '0';
  for ($i = 0; $i < strlen($s); $i++){
    $c = $s[$i];
    if ($c== "\n"){
      echo "<br>";
      $p = 0;
    }
    else if ($c == " "){
      if ($lc == " ")
	echo "&nbsp;";
      else echo " ";
    }
    else echo $c;
    if ($p==$interval){
      echo "<wbr>";
      $p=0;
    }
    $p++;
    $lc = $c;
  }
}
$show_idle_threads = false;
if (array_key_exists("show_idle_threads",$query_string))
   $show_idle_threads = $query_string["show_idle_threads"] != "0";
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

$show_offline_hosts = @$query_string["show_offline_hosts"]==1;
display_worm_page_header("Cluster Activity","<a href=\"view_experiments.php\">[Back to Experiment Index]</a>",FALSE,$header_text);

$host_id = @$query_string["h"];
if (array_key_exists("n",$query_string))
  $node_id = $query_string["n"];
else
  $node_id = -1;
$single_device = $host_id != 0;
function host_label($res_row){
	 return $res_row[1] . ' @ ' . $res_row[6] . ( (strlen($res_row[7])>0 )? (" (" . $res_row[7] . ":" . $res_row[8].")"): "");
}
$query = "SELECT id, name, last_ping,software_version_major,software_version_minor,software_version_compile,system_hostname,additional_host_description,system_parallel_process_id, ((UNIX_TIMESTAMP(NOW()) - last_ping) > dispatcher_refresh_interval) FROM hosts ";
if ($single_device)
  $query .= " WHERE id = $host_id";
$query .=" ORDER BY ((UNIX_TIMESTAMP(NOW()) - last_ping) > 2*dispatcher_refresh_interval) ASC, name";
$sql->get_row($query,$hosts);
//var_dump($hosts);
?>
<?php
$cur_time = ns_current_time();
$sid = 0;

echo "Hosts:";
for ($i = 0; $i < sizeof($hosts); $i++){
    $host_is_online = $hosts[$i][9]==0;
    if ($show_offline_hosts || $host_is_online)
       echo "<a href=\"#h" . $hosts[$i][0] . "\">[".host_label($hosts[$i])."]</a>&nbsp;&nbsp;";
}
echo "<BR>";
echo "<a href=\"view_cluster_status.php?show_offline_hosts=";
if ($show_offline_hosts) echo "0\">[Hide";
else echo "1\">[Show";
echo " Offline Hosts]</a>";
for ($i = 0; $i < sizeof($hosts); $i++){
    
	$host_is_online = $hosts[$i][9]==0;
	if (!$host_is_online && !$show_offline_hosts)
	   continue;
	if ($host_is_online || $single_device){
	  $query = "SELECT DISTINCT node_id, state,current_output_event_id,node_id=0 FROM processing_node_status WHERE host_id = " . $hosts[$i][0];
	  if ($node_id != -1)
	    $query .= " AND node_id = $node_id";
	  $query .=" ORDER BY node_id=0 DESC,state DESC, node_id";
	  //echo $query;
	  $sql->get_row($query,$node_ids);
	}
	else $node_ids = array();

	?><a name="h<?php echo $hosts[$i][0]?>"><table class ="tw" bgcolor="#555555" cellspacing='0' cellpadding='1' width="100%"><tr><td>
<table cellspacing='0' cellpadding='3' width="100%">
<tr <?php echo $table_header_color?>><td colspan = 2>
<?php
	echo "<table cellspacing='0' cellpadding='0', width='100%'><tr><td>";	
	if ($host_is_online)
		echo "<b>" . host_label($hosts[$i]) . "</b> ";
	else echo host_label($hosts[$i]);
	echo "</td><td><div align=\"right\">";
	
	if (!$host_is_online){
	  echo "This host does not appear to be online.  ";
	  if (!$single_device) echo "<a href=\"view_cluster_status.php?h=" . $hosts[$i][0]. "&rt=".$refresh_time . "\">[View logs]</a>";
	}
	if ($single_device)
	echo "<a href=\"view_cluster_status.php\">(view all hosts)</a>";
	echo "</div></td></tr></table></td></tr>";
$col = 0;   
     //make sure we display main thread even if some client was dumb enough not to register it.
     $main_thread_is_registered=false;
  for ($j = 0; $j < sizeof($node_ids); $j++){
      if ($node_ids[$j][0] == 0){
      	 $main_thread_is_registered = true;
	 break;
  	 }
  }
  if (!$main_thread_is_registered){
 
     $pp = sizeof($node_ids);
     for ($j = $pp; $j>0; $j--)
     	 $node_ids[$j] = $node_ids[$j-1];
     $node_ids[0]= array(0,0,0);
     $node_ids[0][0] = 0;
     $node_ids[0][1] = "Simulated";
     
  }
  for ($j = 0; $j < sizeof($node_ids); $j++){
    $node_is_idle = $node_ids[$j][1] == "Idle";
    $node_i = $node_ids[$j][0];
    if ($node_i == 0)
      $thread_name = "main";
    else 
      $thread_name = $node_i;
    
   $thread_status = $node_ids[$j][1];

    if ($node_is_idle && !$show_idle_threads){
    $col2 = $table_colors[$j%2][0];
    $url = "<a href=\"view_cluster_status.php?h=" . $hosts[$i][0]. "&rt=".$refresh_time . "&show_idle_threads=1\"> [" . $thread_name . "] </a>";
 echo "<tr><td valign=\"top\" bgcolor=\"$col2\" width=\"0%\" id=\"rw\"> $url </td><td bgcolor = \"$col2\" width=\"100%\"><font size=-1>Thread is idle</font></td></tr>";
 continue;
}
    $query = "SELECT time, event, processing_job_op, node_id, sub_text FROM host_event_log WHERE host_id = " . $hosts[$i][0] . " AND ";
    if ($node_i == 0){
       //we want to display output from /all/ threads not explicity registered in the main window.
       $query .= " node_id NOT IN (";
       for ($k = 0; $k < sizeof($node_ids); $k++){
       	   if ($node_ids[$k][0] != 0)
	      $query .= $node_ids[$k][0] . ",";
       }
       $query .= "-1) ";
    }
    else{
	$query .=" node_id = " . $node_ids[$j][0];
    }
    $query.= " ORDER BY time DESC, id DESC";
    #die($query);
    if ($single_device)
      $limit = 200;
    if ($node_id != -1)
      $limit = 2000;
    else $limit = 20;
    $query .=" LIMIT " . $limit;
    $sql->get_row($query, $host_events);
    //var_dump($host_events);
    if ($j != 0) echo "</td></tr>";
    if ($node_i == 0){
      $col2 = $table_colors[0][0];
    }
    else{ 
      $col2 = $table_colors[1][$j%2];
    }
    $thread_name = "<a href=\"view_cluster_status.php?h=" . $hosts[$i][0]. "&rt=".$refresh_time . "&n=".$node_ids[$j][0] . "\"> [" . $thread_name . "] </a> <br><br><font size=-2>($thread_status)</font>";

    echo "<tr><td valign=\"top\" bgcolor=\"".$col2 . "\" width=\"0%\" id=\"rw\"><div align=\"center\" $thread_name </div></td><td bgcolor = \"" . $col2 . "\" width=\"100%\">";
      echo "<table cellpadding=0 border=0 cellspacing=0 width =\"100%\" id=\"tw\"><tr><td id=\"rw\">";
      echo "<div id=\"foo".$sid."\" class = \"ns_scroll\"";
      if ($node_id == -1)
	echo "style=\"height:200px\"";
      else echo "style=\"height:600px\"";
      echo ">";
    $col = 1-$col;
    
    for ($k = sizeof($host_events)-1; $k >= 0; $k--){
      
      echo "<b>" . format_time($host_events[$k][0]) . ": </b>";
      output_wrap($host_events[$k][1]);
      	if ($host_events[$k][2] != 0)
		echo "ns_image_processing_pipeline::Calculating " . $ns_processing_task_labels[$host_events[$k][2]];
      
		echo "<br>";
      if ($host_events[$k][4]){
	output_wrap($host_events[$k][4]);
	echo "<br>";
      }
     
    }
//    var_dump($host_events);
    echo "</div></td></tr></table>";
echo "<script type=\"text/javascript\">
    gotoBottom('foo".$sid."'); </script>";
$sid++;
echo "</td></tr>";
  }
  
  ?>
    </table></td></tr></table><br><?php
	}?>
<?php
if ($refresh_time ==0)
  $r = 5;
else if ($refresh_time < 2)
  $r = 0;
else $r = $refresh_time-2;
	echo "<a href=\"view_cluster_status.php?h=" . $host_id. "&rt=" . $r. "&show_offline_hosts=".($show_offline_hosts?"1":"0")."\">[Monitor in real time]</a>";
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