<?php
require_once ('worm_environment.php');
require_once('ns_experiment.php');
require_once('ns_processing_job.php');
$refresh=FALSE;
$job_id = $_POST["job_id"];
if ($_POST["confirm"] != ''){
  $query = "UPDATE delete_file_jobs SET confirmed=1 WHERE id=$job_id";
  $sql->send_query($query);
  $job = new ns_processing_job;
  $job->urgent = 1;
  $job->time_submitted = ns_current_time();
  $job->maintenance_task = $ns_maintenance_tasks["ns_maintenance_delete_files_from_disk_action"];
  $job->delete_file_job_id = $job_id;
  $job->save_to_db($sql);
  ns_update_job_queue($sql);
  $refresh=TRUE;
 }

if ($_POST["confirm_all"] != ''){
  $query = "SELECT id FROM delete_file_jobs WHERE confirmed = 0";
  $sql->get_row($query,$res);
  for ($i = 0; $i < sizeof($res); $i++){
	$job_id = (int)$res[$i][0];
	$query = "UPDATE delete_file_jobs SET confirmed=1 WHERE id=$job_id";
	//echo $query . "<br>";
	$sql->send_query($query);
	$job = new ns_processing_job;
	$job->urgent = 1;
	$job->time_submitted = ns_current_time();
	$job->maintenance_task = $ns_maintenance_tasks["ns_maintenance_delete_files_from_disk_action"];
	$job->delete_file_job_id = $job_id;
	$job->save_to_db($sql);
	}
	ns_update_job_queue($sql);
	$refresh=TRUE;
 }

if ($_POST["dismiss_all"] != ''){
  $query = "DELETE FROM delete_file_specifications";
  $sql->send_query($query);
  $query = "DELETE FROM delete_file_jobs";
  $sql->send_query($query);
  $query = "COMMIT";
  $sql->send_query($query);
  $refresh=TRUE;
}
if ($_POST["dismiss"] != ''){

  $query = "DELETE FROM delete_file_specifications WHERE delete_job_id = $job_id";
  $sql->send_query($query);
  $query = "DELETE FROM delete_file_jobs WHERE id = $job_id";
  $sql->send_query($query);
  $query = "COMMIT";
  $sql->send_query($query);
  $refresh=TRUE;
 }

$show_details_id = $query_string['show_details_id'];
if ($refresh){
  header("Location: manage_file_deletion_jobs.php\n\n");
  die("");
 }
$query = "SELECT id, confirmed, parent_job_id FROM delete_file_jobs ORDER BY confirmed ASC";
$sql->get_row($query,$jobs);
for ($i = 0; $i < sizeof($jobs); $i++){
  $query = "SELECT relative_directory,filename FROM delete_file_specifications WHERE delete_job_id=" . $jobs[$i][0];
  $sql->get_row($query,$job_specifications[$i]);
 }

display_worm_page_header("Manage File Deletion Jobs");
?>
<table border=0 bgcolor="#000000" cellspacing=1 cellpadding=0 align="center"><tr><td>
<table border=0 bgcolor="#FFFFFF" cellspacing=0 cellpadding=3>
<tr><td colspan = 2<?php echo $table_header_color?>>File Deletion Jobs Pending Confirmation<br>
<form method="post" action="manage_file_deletion_jobs.php">
<?php if (sizeof($jobs) > 0){?>
<input name="dismiss_all" type="submit" value="Dismiss All Deletion Requests" onClick="javascript:return confirm('Are you sure you wish to dismiss all requests?')"><?php }?>
</form>
</td></tr><?php
if (sizeof($jobs) == 0){
  echo "<tr><td  bgcolor=\"{$table_colors[0][0]}\">No Deletion Jobs are Ready for Processing.</td></tr>\n";
}
 else{
   for ($i = 0; $i < sizeof($jobs); $i++){
     echo "<tr ><td bgcolor=\"{$table_colors[($i+1)%2][1]}\">\n";
     $job_id = $jobs[$i][0];
     echo "<a name=\"id" . $job_id . "\">";
	if ($i == 0){
	  try{
		$job = new ns_processing_job;
		$query = $job->provide_query_stub();
		$query .= " FROM processing_jobs WHERE id = " . $jobs[$i][2];
		$sql->get_row($query,$res);
		$job->load_from_result($res[0]);
		$job->get_names($sql);
		echo $job->get_job_description($sql,TRUE);
	  }
	  catch (ns_exception $e){
	    echo $e->text . "<BR>";;
	  }
	}
	$s = sizeof($job_specifications[$i]);
	$max = $s;
	if ($job_id != $show_details_id && $s > 10)
		$max = 10;
	
     for ($j=0; $j < $max; $j++)
       echo $job_specifications[$i][$j][0] . "\\" . $job_specifications[$i][$j][1] . "<BR>\n";
	if ($s != $max)
		echo "<center>...<br><a href=\"manage_file_deletion_jobs.php?show_details_id={$job_id}#id{$job_id}\">[View More]</a><center>";
     echo "</td><td bgcolor=\"{$table_colors[($i+1)%2][1]}\" valign=\"top\">";
     if ($jobs[$i][1] == "0")
       echo "<form method=\"post\" action=\"manage_file_deletion_jobs.php\"><input type=\"submit\" name=\"confirm\" value=\"Confirm\"><br><input type=\"submit\" name=\"dismiss\" value=\"Dismiss\"><input type=\"hidden\" name=\"job_id\" value=\"" . $job_id . "\"></form>";
     else
       echo "Pending...";
     echo "</td></tr>\n";
   }
 }
 ?><tr><td colspan = 2<?php echo $table_header_color?>>
<form method="post" action="manage_file_deletion_jobs.php">
<?php if (sizeof($jobs) > 0){?>
<input name="confirm_all" type="submit" value="Confirm All Deletion Requests" onClick="javascript:return confirm('Are you sure you wish to confirm all deletion requests?  This is pretty dangerous.')"><?php }?>
</form>
</td></tr>
</table></td></tr></table><br>
<?php

display_worm_page_footer();
?>
