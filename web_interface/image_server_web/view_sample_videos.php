<?php

require_once("worm_environment.php");
require_once("ns_processing_job.php");
try{

	display_worm_page_header("View Sample Videos");
	if (ns_param_spec($query_string,'samples')){
		$sample_str = @$query_string['samples'];
		$sample_ids = explode('o',$query_string['samples']);
	}
	else die("No samples specified");
	$sample_names = array();
	$sample_names_short = array();
	$sample_image_ids = array();
	$sample_video_ids = array();

	if (ns_param_spec($_POST,'censor')){
		$query = "UPDATE capture_samples SET censored=1 WHERE id = " .  (int)$_POST['sample_id'];
	//	die($query);
		$sql->send_query($query);

	}
	if (ns_param_spec($_POST,'uncensor')){
		$query = "UPDATE capture_samples SET censored=0 WHERE id = " .  (int)$_POST['sample_id'];
		$sql->send_query($query);
	}
	if (sizeof($sample_ids) == 0)
		throw new ns_exception("No samples specified");

	for ($i = 0; $i < sizeof($sample_ids); $i++){
		$query = "SELECT s.name,e.name, s.op0_video_id, s.censored FROM capture_samples as s, experiments as e WHERE s.id = " . $sample_ids[$i] . " AND s.experiment_id = e.id";
		$sql->get_row($query,$res);
		if (sizeof($res) == 0)
			throw new ns_exception("Could not load info for sample " . $sample_ids[$i]);
		$sample_names[$i] = $res[0][1] . "::" . $res[0][0];
		$sample_names_short[$i] = $res[0][0];
		$sample_video_ids[$i] = (int)$res[0][2];
		$sample_censored[$i] = (int)($res[0][3])!=0;

		$bq = "SELECT small_image_id FROM captured_images WHERE sample_id = " . $sample_ids[$i] . " AND small_image_id != 0 AND currently_being_processed=0 AND problem = 0 ORDER BY capture_time DESC LIMIT 1";
		$bq2 = "SELECT image_id FROM captured_images WHERE sample_id = " . $sample_ids[$i] . " AND image_id != 0 AND currently_being_processed=0 AND problem = 0 ORDER BY capture_time DESC LIMIT 1";

		$sql->get_row($bq,$res);
		if (sizeof($res) == 0){
			$sql->get_row($bq2,$res);
		}

		if (sizeof($res) == 0)
			$sample_image_ids[$i] = 0;
		else $sample_image_ids[$i] = (int)$res[0][0];
	}
	?>
	<span class="style1">Displaying Samples:</span><br>
<?php
	for ($i = 0; $i < sizeof($sample_ids); $i++){
		echo "$sample_names[$i], ";
	}
?>
	<br><br><table bgcolor="#555555" cellspacing='0' cellpadding='1' align="center"><tr><td>
	<table cellspacing='0' cellpadding='4' >
<?php
	echo "<tr $table_header_color >\n";
	for ($i = 0; $i < sizeof($sample_ids); $i++){
		echo "<td><center>{$sample_names_short[$i]}</center></td>";

	}
	echo "</tr>\n\n<tr>";

	$image_height = 700;
	$image_width = 150;
	$image_frame_x_buf = 10;
	$image_frame_y_buf=25;

	for ($i = 0; $i < sizeof($sample_ids); $i++){
		$clrs = $table_colors[0];
		echo "<td bgcolor=\"".$clrs[$i%2]."\" valign=\"top\">\n<center>";
		echo "\t<iframe src=\"ns_view_image.php?image_id={$sample_image_ids[$i]}&video_image_id={$sample_video_ids[$i]}&height=$image_height&width=$image_width&redirect=1\" width=\"".($image_width + $image_frame_x_buf)."\" height=\"".($image_height+$image_frame_y_buf)."\"></iframe>";
		echo "<form action=\"view_sample_videos.php?samples=$sample_str\" method=\"post\">";
		echo "<input type=\"hidden\" name=\"sample_id\" value=\"" . $sample_ids[$i] . "\">";
		if (!$sample_censored[$i])
			echo "<input name=\"censor\" type=\"submit\" value=\"Censor\">";
		else
			echo "<input name=\"uncensor\" type=\"submit\" value=\"UnCensor\">";
		echo "</form>";
		echo "</center>\n</td>\n";
	}

	?>
	</tr></table></td></tr>
	</table>
<?php
	display_worm_page_footer();
}
catch (ns_exception $ex){
  die ($ex->text);
}

?>