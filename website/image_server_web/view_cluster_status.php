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

display_worm_page_header("Cluster Activity","<a href=\"view_experiments.php\">[Back to Experiment Index]</a>",FALSE,$header_text);

$current_device_cutoff = 60*10;
$host_id = @$query_string["h"];
if (array_key_exists("n",$query_string))
  $node_id = $query_string["n"];
else
  $node_id = -1;
$single_device = $host_id != 0;
function host_label($res_row){
	 return $res_row[1] . ' @ ' . $res_row[6] . ( (strlen($res_row[7])>0)? (" (" . $res_row[7] . ")"): "");
}
$query = "SELECT id, name, last_ping,software_version_major,software_version_minor,software_version_compile,system_hostname,additional_host_description FROM hosts";
if ($single_device)
  $query .= " WHERE id = $host_id";
$query .=" ORDER BY name";
$sql->get_row($query,$hosts);
//var_dump($hosts);
?>
<?php
$cur_time = ns_current_time();
$sid = 0;
echo "Go to host: ";
for ($i = 0; $i < sizeof($hosts); $i++){
  echo "<a href=\"#h" . $hosts[$i][0] . "\">[".host_label($hosts[$i])."]</a> ";
}
echo "<BR>";
for ($i = 0; $i < sizeof($hosts); $i++){
	$host_is_online = $cur_time - $hosts[$i][2] < $current_device_cutoff;
	if ($host_is_online || $single_device){
	  $query = "SELECT DISTINCT node_id FROM host_event_log WHERE host_id = " . $hosts[$i][0];
	  if ($node_id != -1)
	    $query .= " AND node_id = $node_id";
	  $query .=" ORDER BY node_id";
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
	else echo $hosts[$i][1];
	echo "</td><td><div align=\"right\">";
	
	if (!$single_device && !$host_is_online){
	  echo "This host does not appear to be online.  <a href=\"view_cluster_status.php?h=" . $hosts[$i][0]. "&rt=".$refresh_time . "\">[View logs]</a>";
	}
	if ($single_device)
	echo "<a href=\"view_cluster_status.php\">(view all hosts)</a>";
	echo "</div></td></tr></table></td></tr>";
$col = 0;
  for ($j = 0; $j < sizeof($node_ids); $j++){
    $query = "SELECT time, event, processing_job_op, node_id, sub_text FROM host_event_log WHERE host_id = " . $hosts[$i][0] . " AND node_id = " . $node_ids[$j][0]. " ORDER BY time DESC";
    if ($single_device)
      $limit = 200;
    if ($node_id != -1)
      $limit = 2000;
    else $limit = 20;
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

    echo "<tr><td valign=\"top\" bgcolor=\"".$col2 . "\" width=\"0%\" id=\"rw\"> $thread_name </td><td bgcolor = \"" . $col2 . "\" width=\"100%\">";
      echo "<table cellpadding=0 border=0 cellspacing=0 width =\"100%\" id=\"tw\"><tr><td id=\"rw\">";
      echo "<div id=\"foo".$sid."\" class = \"ns_scroll\"";
      if ($node_id == -1)
	echo "style=\"height:200px\"";
      else echo "style=\"height:600px\"";
      echo ">";
    $col = 1-$col;
    
    for ($k = sizeof($host_events)-1; $k > 0; $k--){
      
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