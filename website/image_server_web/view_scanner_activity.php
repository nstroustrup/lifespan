<?php
require_once ('worm_environment.php');
require_once('ns_experiment.php');
$query = "SELECT d.name,i.incubator_name,i.incubator_location FROM devices AS d "
	 ."LEFT JOIN device_inventory AS i "
	 ."ON d.name = i.device_name WHERE d.simulated_device = 0 ORDER BY i.incubator_name DESC, d.name  ASC";
$sql->get_row($query,$devices);
$schedule_future = array();
$schedule_past = array();
$devices_per_incubator=array();
for ($i = 0; $i < sizeof($devices); $i++)
 	$devices_per_incubator[$devices[$i][1]] = 0;
for ($i = 0; $i < sizeof($devices); $i++)
  $devices_per_incubator[$devices[$i][1]] = $devices_per_incubator[$devices[$i][1]] + 1;

$schedule_future = array();
$schedule_past = array();
for ($i = 0; $i < sizeof($devices); $i++){
	$device_index[$devices[$i][0]] = $i;
	$schedule_future[$i] = array();
	$schedule_past[$i] = array();
}
$number_past = @(int)$query_string['number_past'];
if (!ns_param_spec_true($query_string,'number_past'))
	$number_past = 5;

$number_future = @(int)$query_string['number_future'];
if (!ns_param_spec_true($query_string,'number_future'))
	$number_future= 5;


  $query = "SELECT c.scheduled_time, c.time_at_start, c.time_at_finish, s.experiment_id, c.sample_id, s.name, c.problem, c.captured_image_id, s.device_name, c.missed,c.transferred_to_long_term_storage FROM capture_schedule as c, capture_samples as s WHERE c.sample_id = s.id";

$query .= " AND (c.time_at_start != 0 || c.censored=0)";

  $query_future = $query . " AND c.scheduled_time >= " . ns_current_time() . " ORDER BY c.scheduled_time ASC LIMIT " . sizeof($devices)*$number_future;
  $query_past = $query . " AND c.scheduled_time <= " . ns_current_time() . " ORDER BY c.scheduled_time DESC LIMIT " . sizeof($devices)*$number_past;
 // echo $query_past . "<br>";
 // flush();
//  ob_flush();

  $sql->get_row($query_future,$fut);
  $sql->get_row($query_past,$past);
 foreach ($fut as &$f)
   $schedule_future[$device_index[$f[8]]] = array();
 foreach ($past as &$f)

   $schedule_past[$device_index[$f[8]]] = array();


  foreach ($fut as &$f)
	array_push($schedule_future[$device_index[$f[8]]],$f);
  foreach ($past as &$f)
	array_push($schedule_past[$device_index[$f[8]]],$f);
//$schedule_future[$i]
//echo $query_future . "<br>";
//  flush();
//  ob_flush();
  //$sql->get_row($query_past,$schedule_past[$i]);


//var_dump($devices_per_incubator);
//die("");
$query = "SELECT id,name FROM experiments";
$sql->get_row($query,$exper);
for ($i = 0; $i < sizeof($exper); $i++)
  $exp[$exper[$i][0]] = $exper[$i][1];

display_worm_page_header("Imaging Cluster Activity");
?>
<table width="100%">
<TR><td valign="top" align="center" width="50%"><center>
	<table bgcolor="#555555" cellspacing='0' cellpadding='1' >
		<tr><td>
			<table cellspacing='0' cellpadding='3' >
				<tr <?php echo $table_header_color?>><td><b>Scanners</b></td></tr>
				<tr>
				<td bgcolor="<?php echo $table_colors[0][0]?>">
				<?php
					$i=0;
					foreach($devices as $d){
						if ($d== '')
							$d = "Unknown";
						echo "<a href=\"#d{$d[0]}\">[{$d[0]}]</a> ";
						if ($i == 10){
							echo "<BR>";
							$i = 0;
						}
						$i++;
					}
				?>
				</td></tr>
			</table>
		</td></tr>
	</table></center>
</td>
<td width="50%" valign="top"><center>
	<table bgcolor="#555555" cellspacing='0' cellpadding='1' >
		<tr><td>
			<table cellspacing='0' cellpadding='3' >
				<tr <?php echo $table_header_color?>><td><b>Incubators</b></td></tr>
				<tr> <td bgcolor="<?php echo $table_colors[0][0]?>">
				<?php
					$i = 0;
					foreach($devices_per_incubator as $incubator => $count){
						if ($incubator == '')
							$incubator = "Unknown";
						echo "<a href=\"#i$incubator\">[$incubator]</a>";
						if($i==2){
							$i = 0;
							echo "<BR>";
						}
						$i++;
					}
				?>
				</td></tr>
			</table>
		</td></tr>
	</table>
	</center>
</TD>
</TR>
</table><br>
<?php
	$show_more_future = $number_future+5;
	$show_more_past   = $number_past+5;
	$show_less_future = $number_future-5;
	$show_less_past = $number_past-5;
	if ($show_less_future < 0)
		$show_less_future = 0;
	if ($show_less_past < 0)
		$show_less_past = 0;
?>

<a href="view_scanner_activity.php?number_future=<?php echo $number_future?>&number_past=<?php echo $show_more_past?>">[Show More Past]</a>
<a href="view_scanner_activity.php?number_future=<?php echo $number_future?>&number_past=<?php echo $show_less_past?>">[Show Less Past]</a>
<a href="view_scanner_activity.php?number_future=<?php echo $show_more_future?>&number_past=<?php echo $number_past?>">[Show More Future]</a>
<a href="view_scanner_activity.php?number_future=<?php echo $show_less_future?>&number_past=<?php echo $number_past?>">[Show Less Future]</a>
<?php
$last_incubator_name = 'THIS IS NOT A VALID INCUBATOR NAME';

	$incubator_pos = 1;
for ($i = 0; $i < sizeof($devices); $i++){
	if ($devices[$i][1] != $last_incubator_name){
		if ($i != 0){
			$incubator_pos = 1;
			$need_end = false;
			?>
			</TD></TR></table>
			</td></tr>
			</table></td></tr></table>

<!--//Incubator <?php echo $last_incubator_name?>-->
<br>
<?php
		}
		$last_incubator_name = $devices[$i][1];
		$name = "Incubator " . $last_incubator_name;

		if($last_incubator_name == '')
		$name = "Unknown Incubator";

		$aname = $last_incubator_name;
		if ($last_incubator_name == '')
			$aname = "Unknown";

?>
<!--Incubator <?php echo $aname?>-->
<table width="100%" bgcolor="#555555" cellspacing='0' cellpadding='1' ><tr><td>
<table width="100%" cellspacing='0' cellpadding='0' >
  <tr <?php echo $table_header_color?>><td><b>
<?php
echo "<a name=\"i$aname\">";
echo "<font size=\"+2\">" . $name . "</font></a>";
?></b></td></tr>
<tr><td>
<!--Begin Internal Incubator Table-->
<table width="100%" cellspacing=0 cellpadding=6><TR><TD valign="top" bgcolor="<?php echo $table_colors[1][0]?>">
<?php


}
  $need_end = true;
  // echo "<!-- Begin " . $exp[$schedule_future[$i][$j][3]] ."::". $schedule_future[$i][$j][5]  . " Table -->\n";
  echo "<table align=\"center\" border=0 cellspacing=1 cellpadding=1 width=\"100%\">\n<tr><td bgcolor=\"#000000\" valign=\"top\">\n";
  echo "<table border=0 bgcolor=\"#FFFFFF\" width=\"100%\" cellspacing=0 cellpadding=3>\n";

  echo "<a name=\"d{$devices[$i][0]}\">";
  $s = sizeof($schedule_future[$i]);
  for ($j = $s-1; $j >= 0; $j--){
    echo "<tr bgcolor=\"{$table_colors[0][0]}\"><td><font size=\"-1\">";
    echo format_time($schedule_future[$i][$j][0]);
    echo "&nbsp;&nbsp;&nbsp;&nbsp;(" .$exp[$schedule_future[$i][$j][3]] ."::". $schedule_future[$i][$j][5] . ")";
       if ($schedule_future[$i][$j][6] >  0 )
      echo "<b>[Failed]</b>" . $schedule_future[$i][$j][6];
    echo "</font>\n</td></tr>\n";
  }
  if (sizeof($schedule_future[$i]) == 0) echo "<tr><td bgcolor=\"{$table_colors[0][0]}\"><center>(None)</center></td></tr>\n";
  echo "<tr><td><center><b><font size=\"+1\"><a href=\"view_scanner_schedule.php?device_name=" . $devices[$i][0] . "\">". $devices[$i][0] . "</a></font></b></center></td></tr>\n";

  for ($j = 0; $j < sizeof($schedule_past[$i]); $j++){
    echo "<tr><td bgcolor=\"{$table_colors[0][1]}\"><font size=\"-1\">";
    echo format_time($schedule_past[$i][$j][0]);

    $problem = ($schedule_past[$i][$j][6] != 0);
    $missed = ($schedule_past[$i][$j][9] != 0);
    $busy = (!$problem && !$missed &&  ($schedule_past[$i][$j][7] ==0));
    $pending_transfer = (!$problem && !$missed && !$busy && $schedule_past[$i][$j][10] != 3);

    if (!$problem && !$missed && !$busy && !$pending_transfer)
      echo '<a href="ns_view_image.php?captured_images_id=' . $schedule_past[$i][$j][7] . '">';
    echo "&nbsp;&nbsp;&nbsp;&nbsp;(" .$exp[$schedule_past[$i][$j][3]]. "::". $schedule_past[$i][$j][5] . ")";
    if(!$problem && !$missed && !$busy)
      echo '</a>';
    if ($problem)
	echo" <a href=\"view_hosts_log.php?event_id=" . $schedule_past[$i][$j][6] . "\"><b> [Failed]</b></a>";
    if ($missed)
	echo"<b> [Missed]</b>";
    if ($busy)
	echo" [Busy]";
    if ($pending_transfer)
      echo " [Pending Transfer]";
    echo "</font></td></tr>\n";
  }

  if (sizeof($schedule_past[$i]) == 0) echo "<tr bgcolor=\"{$table_colors[0][1]}\"><td><center>(None)</center></td></tr>\n";
  echo "</table></td></tr></table><BR>\n\n";
  //echo "<!-- End" . $exp[$schedule_future[$i][$j][3]] ."::". $schedule_future[$i][$j][5]  . " Table -->";

//echo $incubator_pos ."/" . $devices_per_incubator[$devices[$i][1]]/3;
//echo $incubator_pos . "/" . floor($devices_per_incubator[$devices[$i][1]]/3);
$first_column_size = ceil($devices_per_incubator[$devices[$i][1]]/3);
$second_column_size = ceil(($devices_per_incubator[$devices[$i][1]] - $first_column_size)/2);
  if ($incubator_pos != 1 && (
	$incubator_pos == $first_column_size ||
      	($incubator_pos == $first_column_size + $second_column_size))){
    echo "</td><TD valign=\"top\" bgcolor=\"{$table_colors[1][0]}\">";
    echo "<!--End Internal Incubator Table-->\n";
  }
$incubator_pos++;
 }

if (isset($need_end) && $need_end){
			?>
			</TD></TR></table>
			</td></tr>
			</table></td></tr></table>
<?php
}

display_worm_page_footer();
?>
