<?php
require_once('worm_environment.php');

function ns_get_filenames($filename,$path,$partition,&$url_filename,&$filesystem_filename){
	global $ns_image_server_storage_directory_absolute,
		$ns_image_server_storage_directory;
	global $DIR_CHAR,$NOT_DIR_CHAR;
	if ($partition != '')
    		$basename = $partition . $DIR_CHAR;
  	$basename .= $path . $DIR_CHAR . $filename;
 	 $basename = str_replace($NOT_DIR_CHAR,$DIR_CHAR,$basename);
  	$url_filename = $ns_image_server_storage_directory . $DIR_CHAR . $basename;
  	$filesystem_filename = $ns_image_server_storage_directory_absolute . $DIR_CHAR . $basename;
}

$agent = $_SERVER['HTTP_USER_AGENT'];
$msie = strpos($agent,'MSIE');
$firefox = strpos($agent,'Firefox');
$safari = strpos($agent,'Safari');
$chrome = strpos($agent,'Chrome');
$can_display_video = $msie || $safari || $chrome;

$image_id = @$query_string['image_id'];
$captured_images_id = @$query_string['captured_images_id'];
$redirect = @$query_string['redirect'];
$video_image_id = @$query_string['video_image_id'];
$fps = @$query_string['fps'];
$width = @$query_string['width'];
$height = @$query_string['height'];

if (!ns_param_spec($query_string,'width'))
	$width = 600;
if (!ns_param_spec($query_string,'height'))
	$height = 600;


if (!ns_param_spec($query_string,$query_string['fps')))
	$fps = 10;


$video_data_present = FALSE;
$image_data_present = FALSE;
$video_problem = "";
$image_problem = "";

if ($captured_images_id != 0){
	$query = "SELECT image_id, mask_applied FROM captured_images WHERE id = $captured_images_id";
	$sql->get_row($query,$res);
	if (sizeof($res)==0)
		echo "Specified captured image does not exist.";
	else $image_id = $res[0][0];
	if ($image_id == 0){
	  if ($res[0][1]=="1")
	    $image_problem = 'it was deleted after its mask was applied.';
	  else $image_problem = 'it was deleted for an unknown reason.';
	}
}


$fps_suffix = "={$fps}fps.mp4";




if ($video_image_id != 0){
	if (!$can_display_video){
		$video_problem = "this client does not support h.264 video";
	}
	else{
		$query = "SELECT `filename`,`path`,`partition` FROM images WHERE id=$video_image_id";
		$sql->get_row($query,$data);
		if (sizeof($data) ==0){
			$video_problem = "the video's database record could not be located";
		}
		else{
			$filename = $data[0][0] . $fps_suffix;
			ns_get_filenames($filename,$data[0][1],$data[0][2],$video_url,$video_filesystem_filename);
			$video_data_present = file_exists($video_filesystem_filename);

			if (!$video_data_present)
				$video_problem = " the video could not be located on disk ($video_filesystem_filename)";
		}
	}
}
//die($video_image_id);
if ($image_id != 0){

	$query = "SELECT `filename`,`path`,`partition` FROM images WHERE id=$image_id";
	$sql->get_row($query,$data);

	if (sizeof($data) ==0){
		$image_problem = "the image's database record could not be located";
	}
	else{

		ns_get_filenames($data[0][0],$data[0][1],$data[0][2],$image_url,$image_filesystem_filename);

		//var_dump($image_filesystem_filename);
//die('');
$image_data_present = file_exists($image_filesystem_filename);

		if (!$image_data_present)
			$image_problem = " the image could not be located on disk . (" . $image_filesystem_filename . ")";
	}
}

$info = '';
if (!$video_data_present){
	if (!$image_data_present){
		display_worm_page_header("View Image");
		if ($video_image_id  != 0)
			echo "The requested video could not be played because " . $video_problem . ".<BR>  Furthermore, the alternate";
		else echo "The requested ";
		echo " image could not be displayed because " . $image_problem . ".<br>";
		display_worm_page_footer();
		die("");
		}
	else{
		if ($redirect == "1"){
			header("Location: $image_url\n\n");
			die("");
		}

		display_worm_page_header("View Image");

		echo "<center>";
		if ($video_image_id){
			echo "<br>(Defaulting to still image because $video_problem)<br><br>";
		}
		echo "<iframe src=\"$image_url\"  width=\"$width\" height=\"$height\"></iframe>";
		echo "<br><a href=\"$image_url\">[View Full Size Image]</a>";
		echo "</center>";
		display_worm_page_footer();
	}
}
else{
	if ($redirect == "1")
		echo "<html><head></head><body>";
	else
	display_worm_page_header("View Image");

	echo "<video src=\"$video_url\" controls=\"controls\" width=\"$width\" height=\"$height\" onerror=\"report_video_failure(event)\">This web browser does not support the video tag.</video>";
	if ($redirect == "1")
		echo "</body></html>";
	else
	display_worm_page_footer();
}

?>
