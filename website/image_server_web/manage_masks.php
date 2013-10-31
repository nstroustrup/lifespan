<?php
require_once('worm_environment.php');
require_once('ns_experiment.php');
require_once('ns_processing_job.php');

$mask_id = $query_string['mask_id'];
if ($mask_id == '')
	$mask_id = 0;

$mask_region_id = $query_string['mask_region_id'];
if ($mask_region_id == '')
	$mask_region_id = 0;

$edit_regions = $query_string['edit'];

$sample_id = $query_string['sample_id'];
if ($sample_id == '')
	$sample_id = 0;

$delete_id = $query_string['delete_id'];
try{
	if ($delete_id != 0)
		delete_mask($delete_id,$sql);
	$vis_message = '';

	if ($_POST['cancel'] != ''){
			$$edit_regions = 0;
	}


	
	if ($query_string['generate_sample_region_image_info'] == 2){

	  if ($sample_id == '0' || $sample_id == '')
	    throw ns_exception("No sample id specified for sample_region_image_generation.");
	  $query = "SELECT experiment_id FROM capture_samples WHERE id = " . $sample_id;
	  $sql->get_row($query,$sample_id);
	  if (sizeof($sample_id) == 0)
		throw new ns_exception("Could not find sample id in database!");

	  $query = "SELECT id FROM capture_samples WHERE experiment_id = ".$sample_id[0][0];
	  $sql->get_row($query,$samples);
	  var_dump($samples);
	// die("");
	  for ($i = 0; $i < sizeof($samples); $i++){
		$job = new ns_processing_job;
		$job->maintenance_task = $ns_maintenance_tasks['ns_maintenance_generate_sample_regions_from_mask'];
		$job->time_submitted = ns_current_time();
		$job->sample_id = $samples[$i][0];
		$job->urgent = TRUE;
		//	  die("foo" . $job->maintenance_task);
		
		$job->save_to_db($sql,TRUE);
	  }
	  ns_update_job_queue($sql);	   

	  header("Location: manage_masks.php?mask_id=$mask_id\n\n");
	  die("");
	}

	if ($query_string['generate_sample_region_image_info'] == 1){

	  if ($sample_id == '0' || $sample_id == '')
	    throw ns_exception("No sample id specified for sample_region_image_generation.");

	  $job = new ns_processing_job;
	  $job->maintenance_task = $ns_maintenance_tasks['ns_maintenance_generate_sample_regions_from_mask'];
	  $job->time_submitted = ns_current_time();
	  $job->sample_id = $sample_id;
	  $job->urgent = TRUE;
	  //	  die("foo" . $job->maintenance_task);
	 
	  $job->save_to_db($sql,TRUE);
	  ns_update_job_queue($sql);	   
if (0){
	  //  echo "Regenerating info";
		$query = "SELECT capture_samples.mask_id FROM capture_samples WHERE id = $sample_id";
		
		//$sql->get_row($query,$capture_sample_mask_id);
		//echo $capture_sample_mask_id[0][0];

		$query = "SELECT image_mask_regions.id, image_mask_regions.mask_value FROM image_mask_regions WHERE image_mask_regions.mask_id = $mask_id ORDER BY image_mask_regions.mask_value ASC";
		//	echo $query . "<BR>";

		$sql->get_row($query,$mask_regions);	   
		//echo "Found " . sizeof($mask_regions) . " regions.";
	       
		//for each region in the mask, see if a region exists for it.
		//Regions exist based on the mask value they are attached to (Ie regions pointing to colors in the mask will be maintained accross new maps
		for ($i = 0; $i < sizeof($mask_regions); $i++){
		  //echo "Searching for samples bound to mask region " . $mask_regions[$i][0] . "<BR>";
			$query = "SELECT info.id, mask_region.id FROM sample_region_image_info as info, image_mask_regions as mask_region"
				   . "WHERE info.mask_region_id = mask_region.id AND mask_region.mask_value = {$mask_regions[$i][1]} "
				   . "AND info.mask_id = $mask_id";
			$sql->get_row($query, $old_records);
			//if pre-existing regions are found, check to see if they point to outdated mask_region_info records
			/*for ($j = 0; $j < sizeof($old_records); $j++){
				if (!$old_records[$j][1]!= $mask_regions[$i][0]){  //if the sample_region_info points to incorrect records, update it to the correct record and delete the incorrect.
				  echo "Found pre-existing";
					$query = "UPDATE sample_region_image_info SET mask_region_id = {$mask_regions[$i][1]} WHERE id={$old_records[$j][0]}";
					$sql->send_query($query);
					$query = "DELETE FROM image_mask_regions WHERE id={$old_records[$j][1]}";
					$sql->send_query($query);
			      
				//else echo "Found Fine pre-existing record.";
				}*/
			//if no pre-existing regions are found, make new ones.
			if (sizeof($old_records) == 0){
			  $iid = $i+1;
				$query = "INSERT INTO sample_region_image_info SET mask_region_id = {$mask_regions[$i][0]}, mask_id = $mask_id, sample_id=$sample_id,name='$iid', details=''";
				//	echo $query;
				$sql->send_query($query);
			}
		}

		//check for extraneous regions
		$query = "SELECT id, mask_region_id FROM sample_region_image_info WHERE sample_id = $sample_id";
		$sql->get_row($query,$all_sample_regions);
		for ($a = 0; $a < sizeof($all_sample_regions); $a++){
		  $found = false;
		  for ($b = 0; $b < sizeof($mask_regions); $b++){
		    if ($all_sample_regions[$a][1] == $mask_regions[$b][0]){
		      $found = true;
		      break;
		    }
		  }
		  if ($found == false){
		    // echo "Could not find match for region " . $all_sample_regions[$a][0] . "(" . $all_sample_regions[$a][1] . ")<br>";
		    $query = "DELETE from sample_region_images WHERE region_info_id=" . $all_sample_regions[$a][0];
		    $sql->send_query($query);
		    $query = "DELETE FROM sample_region_image_info WHERE id = " . $all_sample_regions[$a][0];
		    $sql->send_query($query);
		  }
		}
		
			
	  }
	  
	  header("Location: manage_masks.php?mask_id=$mask_id\n\n");
	  die("");
	}

	
	$capture_sample_names = array();
	//get selected mask region information
	if ($mask_id != 0){
		//get id of mask image
		$query = "SELECT image_id, visualization_image_id FROM image_masks WHERE image_masks.id=$mask_id";
		$sql->get_row($query,$mask_image_ids);
		if (sizeof($mask_image_ids) == 0)
			throw new ns_exception("Could not find specified mask ($mask_id) in database.");		
		$mask_image_id = $mask_image_ids[0][0];
		$mask_visualization_image_id = $mask_image_ids[0][1];
		
		//if the mask has not been processed, submit a job to do it.
		if ($mask_visualization_image_id == 0){
			$vis_message = "The current mask has not yet been analyzed.  It has been submitted to the cluster for processing.";
			//don't resubmit a job.  Check to see if its already been submitted
			$query = "SELECT id FROM processing_jobs WHERE image_id = $mask_image_id";
			$sql->get_row($query,$res);
			if (sizeof($res) == 0){  
			  
				$job = new ns_processing_job();
				$job->image_id = $mask_image_id;
				$job->mask_id = $mask_id;
				$job->operations[$ns_processing_tasks["ns_process_analyze_mask"]] = '1';
				$job->urgent = true;
				$job->time_submitted = ns_current_time();
				$job->save_to_db($sql);
				ns_update_job_queue($sql);

			}
		}
		else{
			$query = "SELECT path, partition,filename, id FROM images WHERE images.id = {$mask_visualization_image_id}";
		       
			$sql->get_row($query,$v);
			//	var_dump($v);
			if (strlen($v[0][1]) != 0)
			  $mask_visualization_filename .=$v[0][1] . "/";
			$mask_visualization_filename .= $v[0][0] . "/";
			$mask_visualization_filename .= $v[0][2];
	
			$mask_visualization_filename = correct_slashes($mask_visualization_filename);
			//	die($mask_visualization_filename);

}

		
	       
		//now look for any sample_regions that use the mask
		$query = "SELECT capture_samples.id, capture_samples.name, capture_samples.experiment_id, experiments.name "
				. "FROM capture_samples, experiments WHERE experiments.id = capture_samples.experiment_id AND capture_samples.mask_id = $mask_id "
				. "AND experiments.id = capture_samples.experiment_id";
		$sql->get_row($query,$sample_ids);
		for ($i = 0; $i < sizeof($sample_ids); $i++){
			$capture_sample_names[$sample_ids[$i][0] ] = $sample_ids[$i][1];
			$sample_experiment_info[$sample_ids[$i][0]] = array($sample_ids[$i][2],$sample_ids[$i][3] );
		}
		$capture_sample_regions = array();
	      
		for ($i = 0; $i < sizeof($sample_ids); $i++){
				$query = "SELECT sample_region_image_info.id, sample_region_image_info.name, sample_region_image_info.details, image_mask_regions.id, "
				."image_mask_regions.mask_value, image_mask_regions.x_average, image_mask_regions.y_average  FROM image_mask_regions, sample_region_image_info "
				."WHERE sample_region_image_info.mask_id = $mask_id AND sample_region_image_info.sample_id = {$sample_ids[$i][0]} "
				."AND image_mask_regions.id = sample_region_image_info.mask_region_id";
				$sql->get_row($query, $capture_sample_regions[$sample_ids[$i][0]]);
		}
		//	var_dump($capture_sample_regions);	

	}
	
	//get information about unclaimed masks
	$query = "SELECT image_masks.id, images.id, images.filename FROM image_masks, images WHERE image_masks.processed = 0 AND image_masks.image_id = images.id";
	$sql->get_row($query, $unclaimed_masks);


	if ($_POST['save'] != ''){
	  foreach($capture_sample_names as $j => $k){
	    //echo $j;
	    for ($i = 0; $i < sizeof($capture_sample_regions[$j]); $i++){
	   
	      $region_id = $capture_sample_regions[$j][$i][0];
	      
	      $name = ns_slash($_POST["region_{$region_id}_name"]);
	      $details = ns_slash($_POST["region_{$region_id}_details"]);
	     
	      $query = "UPDATE sample_region_image_info SET name='$name', details='$details' WHERE id=$region_id";
	     
	      $sql->send_query($query);
	    }
	  }
	  header("Location: manage_masks.php?mask_id=$mask_id\n\n");
	  die("");
	}
	display_worm_page_header("Manage Masks");
}
catch(ns_exception $ex){
	die("Error: " . $ex->text);
}
?>

<?php if ($mask_id != 0){?>

<span class="style1">Mask Information</span>
<?php if ($vis_message != '') {echo  '<BR><br><b>' . $vis_message . '</b><br><BR>';}?>

<table cellspacing='0' cellpadding='1' width="100%"><tr><td>
<?php 
if (sizeof($capture_sample_names) == 0)
	echo "(This mask is not used by any samples.)";
foreach ($capture_sample_names as $capture_sample_id => $capture_sample_name) {
?><form action="manage_masks.php?<?php echo "mask_id=$mask_id&selected_capture_sample_region_id={$selected_capture_sample_region_id}"?>" method="post">
<span class="style1">
<?php echo $sample_experiment_info[$capture_sample_id][1] . " :: " . $capture_sample_name;?>
</span>

<table bgcolor="#555555" cellspacing='0' cellpadding='1' ><tr><td width="0%" valign="top">
<table cellspacing='0' cellpadding='3' ><tr <?php echo $table_header_color?>><br>
<td>Region Name</td>
<td>Region Color</td>
    <td>Region Center</td>
<td>Details</td>
<td>&nbsp;</td></tr>
<?php
	//mvtear_dump($capture_sample_regions);
	if (sizeof($capture_sample_regions[$capture_sample_id]) == 0)
		echo "<tr bgcolor={$table_colors[0][0]} ><td colspan = \"5\" valign=\"top\">(No Mask Regions Found)</td></tr>";
	for ($i = 0; $i < sizeof($capture_sample_regions[$capture_sample_id]); $i++){
		$current_sample_record = $capture_sample_regions[$capture_sample_id][$i];
		$clrs = $table_colors[$i%2];
		$edit = $edit_regions;
		echo "<tr><td bgcolor=\"$clrs[0]\">";
		echo output_editable_field("region_{$current_sample_record[0]}_name",$current_sample_record[1],$edit);
		echo "</td>";
		echo "<td bgcolor=\"$clrs[1]\">";
		echo $capture_sample_regions[$capture_sample_id][$i][4];
		echo "</td>";	
		echo "<td bgcolor=\"$clrs[0]\">";
		echo "(" . $capture_sample_regions[$capture_sample_id][$i][5] . "," . $capture_sample_regions[$capture_sample_id][$i][6] .")";
		echo "</td>";	
	echo "<td bgcolor=\"$clrs[1]\">";
	echo output_editable_field("region_{$current_sample_record[0]}_details",$current_sample_record [2],$edit,'');
		echo "</td>";
		echo "<td bgcolor=\"$clrs[0]\">";
		if (!$edit)
			echo "<a href=\"manage_masks.php?mask_id=$mask_id&edit=1\">[Edit]</a>";
		else{
			echo "<input name=\"save\" type=\"submit\" value=\"Save\"><br>";
			echo "<input name=\"cancel\" type=\"submit\" value=\"Cancel\">";
		}
		echo "</center></td></tr>";
	}
?>
</table></td></tr></table>
<a href="manage_masks.php?generate_sample_region_image_info=1&mask_id=<?php echo $mask_id?>&sample_id=<?php echo $capture_sample_id?>">[(Re)Generate Region Information]</a>
<a href="manage_masks.php?generate_sample_region_image_info=2&mask_id=<?php echo $mask_id?>&sample_id=<?php echo $capture_sample_id?>">[(Re)Generate Region Information for all Samples in Experiment]</a>
</form>
<?php }?>

</td><td valign="top" width = "100%" align="Center"><img width="100" src="<?php echo $ns_image_server_storage_directory . "/" . $mask_visualization_filename?>"> </td></td></tr></table><br>
<br>


<?php }else {?>
<span class="style1">Unclaimed Masks</span>
<table bgcolor="#555555" cellspacing='0' cellpadding='1' ><tr><td>
<table cellspacing='0' cellpadding='3' ><tr <?php echo $table_header_color?>><td>Name</td>
<td>Filename</td>
<td>Image</td>
<td>&nbsp;</td></tr><br>

<?php
if (sizeof($masks) == 0)
	echo "<tr bgcolor={$table_colors[0][0]} ><td colspan = \"4\">(No Unclaimed Masks Exist)</td></tr>";
for ($i = 0; $i < sizeof( $unclaimed_masks); $i++){
	$clrs = $table_colors[$i%2];
	
	echo "<tr><td bgcolor=\"$clrs[0]\">";
	echo "<a href=\"manage_masks.php?mask_id={$unclaimed_masks[$i][0]}\">{$unclaimed_masks[$i][2]}</a>";
	echo "</td>";
	echo "<td bgcolor=\"$clrs[1]\">";
	echo ns_view_image_link($unclaimed_masks[$i][1]);
	echo "</td>";
	echo "<td bgcolor=\"$clrs[0]\">";
	echo "<a href=\"manage_masks.php?delete_id={$unclaimed_masks[$i][0]}\">[Delete]</a>";
	echo "</center></td></tr>";
}
?>
</table></td></tr></table><br>

<?php }?>
<?php display_worm_page_footer()?>
