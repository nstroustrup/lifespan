<?php
require_once ('worm_environment.php');
require_once('ns_experiment.php');

$experiment_id = @$query_string['experiment_id'];
//if ($experiment_id == '')
//	$experiment_id = 0;
$device_name = @$query_string['device_name'];
$show_past = @$query_string['show_past'];
$show_future = @$query_string['show_future'];
$show_cancelled = @$query_string['show_cancelled'];
if (!isset($show_cancelled))
   $show_cancelled = FALSE;
else $show_cancelled = $show_cancelled=="1";
$experiment = new ns_experiment($experiment_id,"",$sql);
if ($experiment->name == '')
	$experiment->name = "All";
if (!isset($show_past))
  $show_past = array(0,50);
 else if ($show_past === '1')
   $show_past = array(0,50);
 else if ($show_past === '0')
   $show_past = array(0,0);
 else $show_past = explode(",",$show_past);

if (!isset($show_future))
  $show_future = array(0,2);
 else if ($show_future === '1')
  $show_future = array(0,50);
 else if ($show_future === '0')
   $show_future = array(0,0);
 else $show_future = explode(",",$show_future);

function display_hash_name($id,&$name_hash){
	if ($name_hash[$id] == '')
		return "#" . $id;
	return $name_hash[$id];

}

function display_double_hash_name($id1,$id2,&$name_hash){
	if ($name_hash[$id1][$id2] == '')
		return "#" . $id1 . "/" . $id2;
	return $name_hash[$id1][$id2];

}
function out_link($ex_id='',$dev_name='', $show_p ='',$show_f='',$show_cancelled=true){
  global $experiment_id,$device_name,$show_past,$show_future; 
 if ($ex_id == '')
    $ex_id = $experiment_id;
  if ($dev_name == '')
    $dev_name = $device_name;
  if ($show_p == '')
    $show_p = $show_past;
  if ($show_f == '')
    $show_f = $show_future;
  $show_p = implode(",",$show_p);
  $show_f = implode(",",$show_f);
  $str = "<a href=\"view_scanner_schedule.php?";
  if ($ex_id != '')
    $str.="experiment_id=$ex_id&";
  if ($dev_name != '')
    $str.="device_name=$dev_name&";
    if ($show_cancelled)
    $str.="show_cancelled=1&";
  $str.= "show_past=$show_p&show_future=$show_f\">";
  return $str;
}

function out_limits_p(&$event_type,$limits,$show_cancelled){
  if ($event_type == 0)
    return out_link('','',$limits,'',$show_cancelled);
  else return out_link('','','',$limits,$show_cancelled);
}
function out_limits($event_type,&$data,$limits,$N,$show_cancelled){ 
  if ($limits[0] != 0){
    $lim = $limits[0] - 50;
    if ($lim < 0)
      $lim = 0;
    $out = out_limits_p($event_type, array($lim,min($lim+50,$N)),$show_cancelled) . "[prev]</a>";
  }
  else $out = "[prev]";
  if ($limits[1] - $limits[0] < $N){
     if ($limits[1] - $limits[0] < 50)
     $out .= out_limits_p($event_type,array($limits[0],min($limits[0]+50,$N)),$show_cancelled); 
   else $out .= out_limits_p($event_type,array($limits[0]+50,min($limits[1]+50,$N)),$show_cancelled);
$out .= " [next]</a>";
}
  else $out .= "[next]";
if ($limits[1] - $limits[0] < $N)
  $out .= out_limits_p($event_type,array(0,$N),$show_cancelled)."[Show all]</a>";
  return "Showing events " . ($limits[0]+1) . "-" . $limits[1] . " out of " . $N . "<br>" . $out;
}

function display_events(&$events,&$name_hash,&$sample_hash,&$limits){
	global $scan_url, $table_colors, $table_header_color;
	if (sizeof($events) == 0){
		echo '(No Events in this Category)';
		return;
	}
	echo "<table width=\"100%\" bgcolor=\"#555555\" cellspacing='0' cellpadding='1' ><tr><td>";
	echo "<table width=\"100%\" cellspacing='0' cellpadding='4'>";
	echo "<tr $table_header_color align=\"center\"><td>Scheduled Time</td><td>Device</td><td>Experiment.Sample</td><td>Started Time</td><td>Duration (minutes)</td><td>Problems</td><td>Obtained Image</td></tr>";	
	for ($i = 0; $i < sizeof($events); $i++){
		$clrs = $table_colors[$i%2];
	
		echo "<tr align=\"center\"><td bgcolor=\"$clrs[0]\">";
 echo format_time($events[$i][4]). " " . "</td>".
		  "<td bgcolor=\"$clrs[1]\">";  if ($events[$i][3] == "0") echo "(none)"; else echo $events[$i][3]; echo "</td>".
			"<td bgcolor=\"$clrs[0]\">". display_hash_name($events[$i][1],$name_hash)."." . display_double_hash_name($events[$i][1],$events[$i][2],$sample_hash) ."</td>".
			"<td bgcolor=\"$clrs[1]\">".format_time($events[$i][5])."</td>".
			"<td bgcolor=\"$clrs[0]\">";
			if ($events[$i][6] != 0)
			echo round(10*($events[$i][6] - $events[$i][5])/60)/10;
			else if ($events[$i][5] != 0)echo "<i>in progress</i>";
			echo "</td>";
		echo "<td bgcolor=\"$clrs[1]\">";
		
                if ($events[$i][9] != 0)
                echo "<i>(Cancelled by user)</i>";
		if ($events[$i][7] != 0)
		echo "<center> <a href=\"view_hosts_log.php?event_id=".$events[$i][7]."\">(Problem)</a></center>";
		echo "</td>".
			"<td bgcolor=\"$clrs[0]\">";
			if ($events[$i][8] != 0){
				echo ns_view_captured_image_link( $events[$i][8], "[Image]");
				//if ($events[$i][9] == '1')
				//	echo "[reg]";
				if ($events[$i][7] == '1')
					echo"[problem]";

			}
			else echo "&nbsp;";
			echo "</td></tr>\n";
	}
	echo "</table></td></tr></table>";
}


$sql_clauses = array();
//create mysql code for specifying experiment
if (isset($experiment_id) && $experiment_id != '')
	array_push($sql_clauses, "capture_schedule.experiment_id = '" . addslashes($experiment_id) . "'");
$experiment_mysql = '';

//create mysql code for specifying device
if (isset($device_name) && $device_name != '')
  array_push($sql_clauses,"capture_samples.device_name = '" . addslashes($device_name) . '\'');

//get experiment information and build hash
$query = "SELECT id ,name, hidden FROM experiments";
$sql->get_row($query,$experiments);
for ($i = 0; $i < sizeof($experiments); $i++)
	$experiment_names[$experiments[$i][0]] = $experiments[$i][1];

$query = "SELECT name FROM devices";
$sql->get_row($query,$res);
$device_names = array();
for ($i = 0; $i < sizeof($res); $i++)
  $device_names[$i] = $res[$i][0];
//ns_generate_device_hash($device_names, $sql);

ns_generate_sample_hash($experiment_id,$sample_names, $sql);

$info = "SELECT capture_schedule.id, capture_schedule.experiment_id, capture_schedule.sample_id, capture_samples.device_name, capture_schedule.scheduled_time, "
      . "capture_schedule.time_at_start, capture_schedule.time_at_finish, capture_schedule.problem, capture_schedule.captured_image_id, capture_schedule.censored FROM capture_schedule, capture_samples " ;
$postfix_ASC = "";
for ($i = 0; $i < sizeof($sql_clauses); $i++){
	$postfix_ASC .= " AND " . $sql_clauses[$i];
}
$postfix_ASC .= " AND capture_schedule.sample_id = capture_samples.id ORDER BY capture_schedule.scheduled_time";
$postfix_DESC = $postfix_ASC;
$postfix_ASC .= " ASC";
$postfix_DESC.= " DESC";
$str = ", capture_schedule.experiment_id DESC, capture_schedule.sample_id ASC";
$postfix_ASC .= $str;
$postfix_DESC .= $str;

$counting_prefix = "SELECT count(capture_schedule.id) FROM capture_schedule, capture_samples ";
if ($show_future[1] - $show_future[0] > 0){
  $suffix = " WHERE ";
  if (!$show_cancelled)
   $suffix .="capture_schedule.censored=0 AND ";
  $suffix .= "$experiment_mysql capture_schedule.scheduled_time >= '" . time() . "' AND capture_schedule.time_at_start = 0 $postfix_ASC ";
  $query = $info . $suffix . " LIMIT " . $show_future[0] . "," . $show_future[1];
//	echo $query; 
 	$sql->get_row($query,$future_events);
	$query = $counting_prefix . $suffix;
	$sql->get_value($query,$future_events_count);
}


$suffix = "WHERE ";
if (!$show_cancelled)
   $suffix .="capture_schedule.censored=0 AND ";
$suffix .=" $experiment_mysql capture_schedule.time_at_start != 0 AND capture_schedule.time_at_finish = 0 AND capture_schedule.scheduled_time >= " . (time()-60*4) . " $postfix_ASC";
//_die($query);
$query = $info . $suffix;
$sql->get_row($query,$current_events);
$query = $counting_prefix . $suffix;
$sql->get_value($query,$current_events_count);


$query = "$info WHERE $experiment_mysql capture_schedule.time_at_start != 0 AND capture_schedule.time_at_finish = 0 AND capture_schedule.scheduled_time < " . (time()-60*4) . " $postfix_ASC";
$sql->get_row($query,$delayed_events);
$query = $counting_prefix . $suffix;
$sql->get_value($query,$delayed_events_count);

if ($show_past[1] - $show_past[0] > 0){
	$suffix = " WHERE ";
	if (!$show_cancelled)
 	   $suffix .="capture_schedule.censored=0 AND ";
	$suffix .= "$experiment_mysql capture_schedule.time_at_finish != 0 $postfix_DESC ";
	$query = $info . $suffix ."LIMIT " .$show_past[0] . "," . $show_past[1];
       
	$sql->get_row($query,$completed_events);
	
	  //	  die("size: " . sizeof($completed_events));
	  $query = $counting_prefix . $suffix;
	  $sql->get_value($query,$completed_events_count);
}



display_worm_page_header("Capture Schedule (" . $experiment->name .")");

?>
<?php echo out_link($experiments[$i][0],$device_name,$show_past,$show_future,!$show_cancelled);
if ($show_cancelled)
	echo "[Hide Cancelled Scans]";
else echo "[Show Cancelled Scans]";
?>
<a href="#Current">[Current]</a> <a href="#future">[Future]</a> <a href="#completed">[Completed]<br><br>


</a>
<?php
 /*
echo "<b>Inspect Single Experiment</b><br>\n";
echo "<a href=\"view_scanner_schedule.php?experiment_id=&\">[all]</a> ";
for ($i = 0; $i < sizeof($experiments); $i++){
  if ($experiments[$i][2] == '0')
	echo out_link($experiments[$i][0]) . "[" . display_hash_name($experiments[$i][0],$experiment_names)."]</a> ";
}
echo "<br><b>Inspect Single Device</b><br>\n";
echo "<a href=\"view_scanner_schedule.php?experiment_id=&\">[all]</a> ";

$i = 0;
 
for($i = 0; $i < sizeof($device_names); $i++){
	echo out_link($experiment_id, $device_names[$i]) ."[" . $device_names[$i]."]</a> ";
       
	}*/
?>
<a name="Current"></a><br>
<?php
echo  "<h2>Currently Scanning</h2>";
display_events($current_events, $experiment_names,$sample_names,$device_names);

?><a name="future"></a><?php
if ($show_future[1] - $show_future[0] > 0)
  $r = array(0,0);
 else $r = array(0,50);
echo  "<br><h2>" . out_link($experiment_id, $device_name, $show_past, $r,$show_cancelled) . "Future Scans</a></h2>";
if ($show_future[1] - $show_future[0] != 0){

  echo out_limits(1,$future_events,$show_future,$future_events_count,$show_cancelled);
  display_events($future_events, $experiment_names,$sample_names,$show_future);
    
    }
?><a name="completed"></a><?php
if ($show_past[1] - $show_past[0] > 0)
  $r = array(0,0);
 else $r = array(0,50);
echo  "<br><h2>" . out_link($experiment_id, $device_name, $r, $show_future) . "Completed Scans</a></h2>";
if ($show_past[1] - $show_past[0] > 0){
  echo out_limits(0,$completed_events,$show_past,$completed_events_count,$show_cancelled);
  display_events($completed_events, $experiment_names,$sample_names,$show_past);
    }

display_worm_page_footer();
?>
