<?php
require_once("worm_environment.php");


function ns_clean_up_for_tooltip($str){
  $out = "";
  for ($i = 0; $i < strlen($str); $i++){
    if ($str[$i] != '"')
      $out .= $str[$i];
    else $out .= "'";
  }
  return $out;
}
$current_device_cutoff = 60*3;

$host_id = @$query_string['host_id'];
if (ns_param_spec($_POST,'host_id'))
	$host_id = $_POST['host_id'];
else $host_id = "";
$show_host_nodes = @$query_string['show_host_nodes']=='1';
$device_name = @$query_string['device_name'];
$highlight_host_id = @$query_string['highlight_host_id'];
$selected_devices = @$_POST['selected_devices'];

$refresh = FALSE;
$host_id_name = '';
if ($host_id != 0){
	$query = "SELECT name FROM hosts WHERE id=$host_id";
	$sql->get_row($query,$r);
	if (sizeof($r) == 0)
		die("Could not find specified host id in db:  $host_id");
	$host_id_name = $r[0][0];

}
if (ns_param_spec($_POST,'save_host')){
	$comments = ($_POST['comments']);

	$update_database_name = $_POST['update_database_name']=='1';
	$dname = '';
	$requested_database_name = $_POST['requested_database_name'];
	if ($update_database_name){
		$dname = ",database_used='$requested_database_name'";
		//die("WHEE");
	}
	$query = "UPDATE hosts SET comments='$comments' $dname WHERE id='$host_id'";
//	die ($query);
	$sql->send_query($query);
	if (!$show_host_nodes){
		$query = "UPDATE hosts SET comments='$comments' $dname WHERE base_host_name = '$host_id_name'  OR name='$host_id_name'";
		$sql->send_query($query);
	}
	$refresh = true;
}
if (ns_param_spec($_POST,'save_device')){
	$comments = ($_POST['comments']);
	$query = "UPDATE devices SET comments='$comments' WHERE name='$device_name'";
	$sql->send_query($query);
	$refresh = true;
}

if (ns_param_spec($_POST,'request_preview_capture')){
	$reflective = $_POST['reflective_preview'];

	if ($reflective != '')
	  $val = 2;
	else $val = 1;

	foreach ($selected_devices as $name){
		$query = "UPDATE devices SET preview_requested=$val WHERE name='$name'";
	//	echo $query . "<BR>";
		$sql->send_query($query);
	}
	$refresh = TRUE;
 }

if (ns_param_spec($_POST,'delete_host')){
	$query = "DELETE FROM hosts WHERE id='$host_id'";
	$sql->send_query($query);
	$refresh = TRUE;
}
if (ns_param_spec($_POST,'pause')){
//die($show_host_nodes?"YES":"NO");

  if ($show_host_nodes){
  $query = "UPDATE hosts SET pause_requested= !pause_requested WHERE id='$host_id'";
  $sql->send_query($query);
  }
  else{
	$query = "UPDATE hosts SET pause_requested = !pause_requested WHERE base_host_name = '$host_id_name' OR name='$host_id_name'";

	$sql->send_query($query);
  }
	$refresh = TRUE;
 }
if (ns_param_spec($_POST,'hotplug')){
  $id = $_POST['host_id'];
  $query = "UPDATE hosts SET hotplug_requested = 1 WHERE id='$id'";
  $sql->send_query($query);
	$refresh = TRUE;
 }
if (ns_param_spec($_POST,'buffer_reload')){
  $id = $_POST['host_id'];
  $query = "UPDATE hosts SET hotplug_requested = 2 WHERE id = '$id'";
  //die($query);
  $sql->send_query($query);
  $refresh = TRUE;
 }
if (ns_param_spec($_POST,'pause_all')){
  $id = $_POST['host_id'];
  $query = "UPDATE hosts SET pause_requested= 1";
  $sql->send_query($query);
	$refresh = TRUE;
 }
if (ns_param_spec($_POST,'pause_none')){
  $id = $_POST['host_id'];
  $query = "UPDATE hosts SET pause_requested= 0";
  $sql->send_query($query);
	$refresh = TRUE;
 }
if (ns_param_spec($_POST,'shut_down')){
  $id = $_POST['host_id'];
  $query = "UPDATE hosts SET shutdown_requested=1 WHERE id='$id'";
  $sql->send_query($query);
	if (!$show_host_nodes){
		$query = "UPDATE hosts SET shutdown_requested=1 WHERE base_host_name = '$host_id_name' OR name='$host_id_name'";
		$sql->send_query($query);
	}
	$refresh = TRUE;
 }
if (ns_param_spec($_POST,'launch_from_screen_saver')){
  $id = $_POST['host_id'];
  $query = "UPDATE hosts SET launch_from_screen_saver=1 WHERE id='$id'";
  $sql->send_query($query);
	$refresh = TRUE;
 }

//var_dump($_POST);
if (ns_param_spec($_POST,'add_to_device_inventory')){
	//var_dump($selected_devices);
	//die('');
	//echo $selected_devices . "<BR>";
	foreach ($selected_devices as $name){
		$query = "SELECT device_name FROM device_inventory WHERE device_name = '$name'";
		$sql->get_row($query,$res);
		if (sizeof($res) > 0)
			echo "Device " . $name . " is already in the device inventory<br>";
		else{
			$query = "INSERT INTO device_inventory SET device_name='$name'";
			$sql->send_query($query);
		}
	}
	$refresh = TRUE;
}
if (ns_param_spec($_POST,'remove_device_from_inventory')){
	echo $selected_devices . "<BR>";
	foreach ($selected_devices as $name){
		$query = "SELECT device_name FROM device_inventory WHERE device_name = '$name'";
		$sql->get_row($query,$res);
		if (sizeof($res) > 0){
			$query = "DELETE FROM device_inventory WHERE device_name='$name'";
			$sql->send_query($query);
		}
		else{
			echo "Device " . $name . " is not in the device inventory<br>";
		}
	}
	$refresh = TRUE;
}
if (ns_param_spec($_POST,'pause_captures')){
	//echo $selected_devices . "<BR>";
	foreach ($selected_devices as $name){

		$query = "UPDATE devices SET pause_captures = !pause_captures WHERE name='$name'";
		$sql->send_query($query);
	}
	$refresh = TRUE;
}

if (ns_param_spec($_POST,'set_autoscan_interval')){
	$interval = 60*(int)$_POST['autoscan_interval'];
	foreach ($selected_devices as $name){

		$query = "UPDATE devices SET autoscan_interval = $interval WHERE name='$name'";
		$sql->send_query($query);
	}
	$refresh = TRUE;
}
if (ns_param_spec($_POST,'cancel_autoscan_interval')){
	$interval = $_POST['autoscan_interval'];
	foreach ($selected_devices as $name){

		$query = "UPDATE devices SET autoscan_interval = 0 WHERE name='$name'";
		$sql->send_query($query);
	}
	$refresh = TRUE;
}
if (ns_param_spec($_POST,'save_device_locations')){
	$device_locations = $_POST['device_locations'];
	foreach($device_locations as $name => $location){
		$query = "UPDATE device_inventory SET incubator_location='$location' WHERE device_name = '$name'";
		$sql->send_query($query);

	}
	$device_incubators = $_POST['device_incubator'];
	foreach($device_incubators as $name => $incubator){
		$query = "UPDATE device_inventory SET incubator_name='$incubator' WHERE device_name = '$name'";
		$sql->send_query($query);

	}
	//$refresh = TRUE;
}

if ($refresh){
  header("Location: view_hosts_and_devices.php\n\n");
die("");
 }


$query = "SELECT id, name, ip, last_ping, comments, long_term_storage_enabled, port,software_version_major,software_version_minor,software_version_compile, pause_requested, base_host_name, database_used,available_space_in_volatile_storage_in_mb,time_of_last_successful_long_term_storage_write FROM hosts ORDER BY pause_requested,name";
$sql->get_row($query,$hosts);
$base_hosts = array();
foreach ($hosts as $row){
	$base_hosts[$row[11]] = array();
	$nodes_running[$row[11]][0] = 0;
	$nodes_running[$row[11]][1] = 0;

}

$current_time = ns_current_time();

foreach ($hosts as $row){
	array_push($base_hosts[$row[11]],$row);
	$current = $current_time - $row[3] < $current_device_cutoff;
	if ($current)
		$nodes_running[$row[11]][0]++;
	$nodes_running[$row[11]][1]++;
}


$query = "SELECT devices.host_id, devices.name,
		devices.comments, devices.error_text,
		devices.in_recognized_error_state, devices.simulated_device,
		devices.unknown_identity, devices.pause_captures,
		devices.currently_scanning, devices.last_capture_start_time,
		devices.autoscan_interval, devices.preview_requested, devices.next_autoscan_time
		FROM devices, hosts WHERE devices.host_id = hosts.id ORDER BY name";
$sql->get_row($query, $devices);
$devices_identified = array();
$connected_devices = array();
for ($i = 0; $i < sizeof($devices); $i++){
	$devices_identified[$devices[$i][1]] = 1;
	$connected_devices[$devices[$i][1]] = $devices[$i];
}

$query = "SELECT device_name, incubator_location, incubator_name FROM device_inventory";
$sql->get_row($query,$device_inventory_q);
$device_inventory = array();
$device_inventory[''] = array();
$device_in_inventory = array();
$device_incubators = array();
for ($i = 0; $i < sizeof($device_inventory_q); $i++)
	$device_inventory[$device_inventory_q[$i][2]] = array();

for ($i = 0; $i < sizeof($device_inventory_q); $i++){
	$device_identified = ($devices_identified[$device_inventory_q[$i][0]]===1)?1:0;
	if ($device_identified == 1)
		$devices_identified[$device_inventory_q[$i][0]]=2;
	$incubator_name = $device_inventory_q[$i][2];
	array_push($device_inventory[$incubator_name],
		array($device_inventory_q[$i][0],$device_inventory_q[$i][1],$device_identified));
	$device_in_inventory[$device_inventory_q[$i][0]] = 1;
	$device_incubators[$device_inventory_q[$i][0]] = $device_inventory_q[$i][2];
}
for ($i = 0; $i < sizeof($hosts); $i++)
	$device_map[$hosts[$i][0]] = array();

for ($i = 0; $i < sizeof($devices); $i++){
	array_push($device_map[$devices[$i][0]],$devices[$i]);

}
$unregistered_device_inventory = array();
foreach ($devices_identified as $name => $info){
	if ($info == 1){
		array_push($unregistered_device_inventory,
			array($name,"",1));
	}
}

display_worm_page_header("Hosts and Devices","<a href=\"view_experiments.php\">[Back to Experiment Index]</a>",TRUE,"");
?>
<table border="0" cellspacing="15"><TR><TD valign="top">
<form action="view_hosts_and_devices.php?<?php echo "show_host_nodes=" . ($show_host_nodes?"1":"0")?>" method="post">
<span class="style1">Image Capture and Processing Servers</span><br>
<table border = 0><TR><td width="100%">
<?php
if ($show_host_nodes)
		echo "<a href=\"view_hosts_and_devices.php?show_host_nodes=0\">[Hide Individual Nodes]</a>";
	else    echo "<a href=\"view_hosts_and_devices.php?show_host_nodes=1\">[Show Individual Nodes]</a>";
?></td><td nowrap>
<input name="pause_all" type="submit" value="Pause All Nodes">
<input name="pause_none" type="submit" value="Un-Pause All Nodes"></td></TR></table>
<input name="device_name" type="hidden" value="all">
</form>
<form action="view_hosts_and_devices.php" method="post">
<table bgcolor="#555555" cellspacing='0' cellpadding='1'><tr><td>
<table cellspacing='0' cellpadding='3' >
<tr <?php echo $table_header_color?>><td>Host Name</td>
<td>Version</td>
<td>IP Address</td>
<td>Last Ping</td>
<td>Comments</td>
<td>&nbsp;</td></tr>
<?php
$cur_time = ns_current_time();
$k = 0;
foreach ($base_hosts as $base_host_name => $host){
	for ($i = 0; $i < sizeof($host); $i++){
		$clrs = $table_colors[$k%2];
		$edit_host = ($host_id == $host[$i][0]);
		echo "<tr><td bgcolor=\"$clrs[0]\">";
		$current = $cur_time - $host[$i][3] < $current_device_cutoff;
		if ($current) echo "<b>";
		if ($highlight_host_id == $host[$i][0]) echo "<i>";

		echo $host[$i][1];

		if ($host[$i][10] == "1")
		echo "(p)";

		if ($current) echo "</b>";
		if ($highlight_host_id == $host[$i][0]) echo "</i>";

		echo "<a href=\"view_hosts_log.php?host_id={$host[$i][0]}&limit=50\">[log]</a></td>";
		echo "<td bgcolor=\"$clrs[1]\">{$host[$i][7]}.{$host[$i][8]}.{$host[$i][9]}</td>";
		echo "<td bgcolor=\"$clrs[0]\">";
		echo $host[$i][2] . ":" . $host[$i][6];
		echo "</td><td bgcolor=\"$clrs[1]\">";
		echo format_time($host[$i][3]);
		echo "</td><td bgcolor=\"$clrs[0]\" nowrap>";
		if ($host[$i][5] != "1"){
		  echo "<b>Unable to Access Long Term Storage since ";
		  echo  format_time($host[$i][14]);
		  echo "<br></b>";
		}
		if ($base_host_name != '' && !$show_host_nodes && sizeof($host) > 1){
			//echo $base_host_name;
			echo $nodes_running[$base_host_name][0] . "/".  $nodes_running[$base_host_name][1] ." nodes running<br>";
		}
		$devs = $device_map[$host[$i][0]];
		//	var_dump($devs);
		$real_devs = FALSE;
		for ($j = 0; $j < sizeof($devs); $j++){
		  if ($devs[$j][5]!="1"){
		    $real_devs = TRUE;
		    break;
		  }
		}
		if (!$edit_host){

		  echo "Database: " . $host[$i][12] . "<BR>";
}
		if (sizeof($devs) > 0)
			echo sizeof($devs). " devices<br>";
		echo output_editable_field("comments",$host[$i][4],$edit_host,'',TRUE);
			echo (floor($host[$i][13]/1024)) . " gb free</br>";
		echo "</td><td bgcolor=\"$clrs[1]\">";
		if (!$edit_host)
		  echo "<a href=\"view_hosts_and_devices.php?show_host_nodes=" . ($show_host_nodes?"1":"0") . "&host_id={$host[$i][0]}\">[Edit]</a>";
		else{
			if (!$real_devs){
				echo "Database: "; ns_output_database_selector('requested_database_name',$host[$i][12],FALSE);
				echo "<input name=\"update_database_name\" type=\"hidden\" value=\"1\">";
			}
			echo "<input name=\"host_id\" type=\"hidden\" value=\"{$host[$i][0]}\">";
			echo "<table cellspacing=5 cellpadding=0 border = 0>";
			echo '<tr><td><input name="save_host" type="submit" value="Save"><BR>';
			echo '<input name="delete_host" type="submit" value="Delete"></td>';
			echo '<td><input name="launch_from_screen_saver" type="submit" value="Launch"><br>';
			echo '<input name="shut_down" type="submit" value="Shut Down"></td></tr>';
			echo '<tr><td><input name="pause" type="submit" value="Pause"><br>';
			echo '<input name="hotplug" type="submit" value="Hotplug Devices"</td>';
			echo '<input name="buffer_reload" type="submit" value="Reload Schedule Buffer" onClick="javascript:return confirm(\'This will force the image acquisition server to re-download all pending scans, causing it to be temporarily unresponsive.  Continue?\')">';
			echo "</td>";
			echo '<td valign="bottom" align="right"><input name="cancel" type="submit" value="Cancel"></td></tr>';
			echo '</table>';
		}
		echo "</td></tr>";

		$k++;
		$clrs = $table_colors[1];
		$l = 0;
		if (sizeof($devs) > 0){
			echo "<tr><td bgcolor=\"$clrs[0]\">&nbsp;</td><td bgcolor=\"$clrs[0]\" colspan=4>";
			echo "<table cellspacing = 0 cellpadding = 2 width=\"100%\"><tr><td>";
			$sorted_devs = array();
			for ($j = 0; $j < sizeof($devs); $j++){
			  $inc = $device_incubators[$devs[$j][1]];
			  if (!isset($sorted_devs[$inc]))
			    $sorted_devs[$inc] = array();
			  array_push($sorted_devs[$inc],$devs[$j]);
			}
			$j = 0;
			foreach ($sorted_devs as  $inc_name => $inc){
			  if ($inc_name == '')
			    echo "<b>Unknown Incubator</b>";
			  else
			    echo "<b>Incubator " . $inc_name . "</b>";
			  // $clrs = $table_colors[$l%2];
			  echo "</td><td bgcolor=\"$clrs[0]\">";
			  $j++;

			  foreach ($inc as $dev){

				$edit_device = ($device_name == $dev[1]);
				echo "<a href=\"view_scanner_schedule.php?device_name=".$dev[1]."\">" . $dev[1] . "</a>";
				//echo "</td><td bgcolor=\"$clrs[0]\">";
				if ($dev[3] != ""){

				  echo " <span title=\"" . ns_clean_up_for_tooltip($dev[3]) . "\">";

				echo "<font color=\"red\">(Error)";
				//	if ($dev[4] == "1")
				//echo "(Recognized)";
				//else echo "(Unrecognized)";
				//echo "</b>:";
				echo "</font>";
				//echo $dev[3];
				//if ($dev[8] != "0")
				//	echo "Scanning for " . number_format((-$devs[$j][9] + $current_time /60),1) . "min)";
				echo "</span>";
				}
				//echo $dev[3];
				//echo "<a href=\"view_hosts_and_devices.php?device_name=" . $devs[$j][1] . "\">[Edit]</a>";
				if ($j != 0 && ($j+1)%3 == 0){
					$clrs = $table_colors[$l%2];
					echo "</td></tr><tr><td bgcolor=\"$clrs[0]\">";
					$l++;
				}
				else echo "</td><td bgcolor=\"$clrs[0]\">";
				$j++;
			  }
			  if ($j%3 != 0){
			    $clrs = $table_colors[$l%2];
			    echo "</td></tr><tr><td bgcolor=\"$clrs[0]\">";
			    $l++;
			    $j+=$j%3;
			  }
			}
		        echo "</td></tr></table></td><td bgcolor=\"$clrs[1]\"></td></tr>";
		}
		if ($base_host_name != '' && !$show_host_nodes)
			break;

	}
}
?>
</table></td></tr></table>
</form>

<!--*Connected to long term storage.  These hosts can act as file servers.-->
</td><td valign="top">
<span class="style1">Image Capture Devices</span>
<table bgcolor="#555555" cellspacing='0' cellpadding='1'><tr><td><table cellspacing='0' cellpadding='3'>
<tr <?php echo $table_header_color?>>
<td>Device Name</td>
<td>Scanning Duration</td>
<td>Incubator Name</td>
<td>Location</td>
<td>Current Status</td>
<td>&nbsp;</td>
</tr>
<?php
$k = 0;

echo "<form id=\"incubator_form\" action=\"view_hosts_and_devices.php?show_host_nodes=" . ($show_host_nodes?"1":"0") . "\" method=\"post\">\n";


foreach ($device_inventory as $incubator => $devices){
	if(sizeof($devices) == 0) continue;
	$clrs = $table_colors[$k%2];
	if ($incubator == '')
		$incubator_display = "[No Incubator Specified]";
	else $incubator_display = "Incubator $incubator";
	echo "<tr><td bgcolor=\"$clrs[0]\" colspan = 4><b>$incubator_display</b></td><td colspan= 2 nowrap bgcolor = \"$clrs[0]\" align=\"right\"><input type=\"checkbox\" name=\"checkall\" onClick=\"ns_check_all(document.getElementById('incubator_form'),'incubator_$incubator',this.checked)\"></td></tr>\n";
	foreach ($devices as $device){
		//if ($device[2] == 0) continue;
		$dev =& $connected_devices[$device[0]];
		$k++;
		echo "\t<td bgcolor=\"$clrs[1]\">$device[0]</a></td>\n";
		echo "\t<td bgcolor=\"$clrs[1]\">";
		if ($device[2] == 1){
			if ($dev[8]==1)
				echo "<i>".number_format((-$dev[9] +$current_time)/60,1)  . " min</i>";
			if ($dev[7]==1)
				echo "(Paused!)";
			if ((int) $dev[11]>0)
				echo "(Preview Requested)";
		};
		echo "</td>";
		echo "\t<td bgcolor=\"$clrs[1]\">";
		echo output_editable_field("device_incubator[$device[0]]",$incubator,TRUE,'10',FALSE);
		echo "</td>\n";
		echo "\t<td bgcolor=\"$clrs[1]\">";
		echo output_editable_field("device_locations[$device[0]]",$device[1],TRUE,'4',FALSE);
		echo "</td>\n";
		echo "\t<td bgcolor=\"$clrs[1]\">";
	if ($dev[3] != ""){
			  echo "<span title=\"" . ns_clean_up_for_tooltip($dev[3]) . "\">";
			  echo "<font color=\"red\">(Error)</font>";
			  echo "</span>";
			  //echo "<b>Error ";
			  //if ($dev[4] == "1")
			  //	echo "(Recognized)";
			  //	else echo "(Unrecognized)";
			  //	echo "</b>:";
			  //b	echo $dev[3];
			}

		if ($device[2] != 1){
			echo "<font size=\"-1\">(Missing)</font>";
		}
		else{
			//if ($dev[8]!=1)
			//	echo $dev[9];
			if ((int) $dev[10]>0){
				if ((int) $dev[12]>0)
				echo number_format(($dev[12]-$current_time)/60,1) . "/";
				echo number_format(($dev[10])/60,1) . "min";
}
		};
		echo "</td>";
		echo "<td bgcolor=\"$clrs[1]\" align=\"right\">";
		echo "<input type=\"checkbox\" class=\"incubator_$incubator\" name=\"selected_devices[ ]\" value=\"$device[0]\">";
		echo "</td>\n";
		echo "</tr>\n";
		$clrs = $table_colors[$k%2];
	}
if (sizeof($devices)==0)
	$k++;
}
if (sizeof($unregistered_device_inventory) > 0){
	$k = 0;
	$clrs = $table_colors[$k%2];
	$incubator = "<b>[Undocumented Attached Devices]</b>";
	echo "<tr><td bgcolor=\"$clrs[0]\" colspan = 6>$incubator</td></tr>\n";
}
foreach ($unregistered_device_inventory as $device){
	$clrs = $table_colors[$k%2];
	$dev =& $unregistered_device_inventory[$device[0]];
	$k++;
	echo "\t<td bgcolor=\"$clrs[1]\">$device[0]</a></td>\n";

	echo "\t<td bgcolor=\"$clrs[1]\" colspan=3>&nbsp;</td>\n";
	echo "</td>\n";
	echo "\t<td bgcolor=\"$clrs[1]\">";

	if ((int) $dev[10]>0)
				echo "(Autoscan " . number_format(($dev[10])/60,1) . " min)";
	if ($dev[7]==1)
		echo "(Paused!)";
	if ($dev[8]==1)
		echo "(Scanning for " . number_format((-$dev[9] +$current_time)/60,1)  . "min)";
	if ($dev[3] != ""){
		echo "<b>Error ";
		if ($dev[4] == "1")
		echo "(Recognized)";
		else echo "(Unrecognized)";
		echo "</b>:";
		echo $dev[3];
	}
	if ((int) $dev[10]>0)
		echo "(Preview Requested)";


	echo "</td><td bgcolor=\"$clrs[1]\">";
	echo "<input type=\"checkbox\" name=\"selected_devices[ ]\" value=\"$device[0]\">";
	echo "</td>\n";
	echo "</tr>\n";
}
echo "<tr><td bgcolor=\"$clrs[0]\" colspan = 6 align=\"center\">";
echo "<input name=\"request_preview_capture\" type=\"submit\" value=\"Request Preview Capture\"> (";
echo " Reflective : <input type=\"checkbox\" name=\"reflective_preview\">)";

echo "<input name=\"pause_captures\" type=\"submit\" value=\"Pause Captures\">";
echo "<BR><BR>";
echo "Interval: <input name=\"autoscan_interval\" size=3 type=\"text\" value=\"15\">min ";
echo "<input name=\"set_autoscan_interval\" type=\"submit\" value=\"Start Autoscans\"> ";
echo "<input name=\"cancel_autoscan_interval\" type=\"submit\" value=\"Cancel Autoscans\">";
echo "<BR><BR>";
echo "<input name=\"save_device_locations\" type=\"submit\" value=\"Save Devices Locations\">";
echo "<br><br>";
echo "<input name=\"add_to_device_inventory\" type=\"submit\" value=\"Add devices to inventory\"> ";
echo "<input name=\"remove_device_from_inventory\" type=\"submit\" value=\"Remove devices from inventory\">";
echo "</td></tr>";
echo "</table></td></tr></table>";
echo "</form>";

?>
</td></tr></table>
<?php
display_worm_page_footer();
?>