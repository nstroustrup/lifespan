<?php
require_once("ns_exception.php");
require_once("ns_image_server_website.ini");
$website_version = "1.11";

define("NS_SPATIAL_NORM", 1);
define("NS_TEMPORAL_NORM", 2);
define("NS_BROWSE", 3);
define("NS_JPG", 3);


function ns_load_experiment_groups(&$experiment_groups,&$group_order,&$sql){
	$query = "SELECT group_id, group_name, hidden, group_order FROM experiment_groups ORDER BY group_order ASC";
	$sql->get_row($query,$res);
	$i = 0;
	foreach ($res as &$r){
		$experiment_groups[$r[0]] = array($r[0],$r[1],$r[2],$r[3]);
		$group_order[$i] = $r[0];
		$i++;
	}
	$experiment_groups[0] = array('0','No Group',0,999);
	$group_order[$i] = 0;
}

function ns_attempt_to_retry_transfer_to_long_term_storage($sample_id,$device,$experiment_id,&$sql){
  if ($experiment_id == 0 && $sample_id == 0)
    die("ns_attempt_to_retry_transfer_to_long_term_storage()::No experiment id or sample id specified");
  $query = "UPDATE ";
  //  $query = "SELECT s.name, c.* FROM ";
  $query .= "capture_samples as s, capture_schedule as c ";
  $query .= "SET c.problem = 0 ";
  $query .= "WHERE c.sample_id = s.id AND c.transferred_to_long_term_storage = 1 ";
  if ($device != "")
    $query .= "AND s.device_name = '$device' ";
  if ($sample_id != 0)
    $query .= "AND s.id = $sample_id";
  if ($experiment_id != 0)
    $query .= "AND s.experiment_id = $experiment_id";
  // $sql->get_row($query,$res);
   // var_dump($res);
  $sql->send_query($query);
  die($query);

}
function ns_check_db_name($name){
    global $database_choices;
    $db_found = 0;
    foreach($database_choices as $d){
      if ($d == $name){
	$db_found = 1;
	break;
      }
    }
    if (!$db_found)
      throw new ns_exception("Invalid image server database name:" .  $name);
  }

function ns_set_database_name($name){
  global $db_name;
  ns_check_db_name($name);
    setcookie('ns_image_server_db_name',$name);
  
    $db_name = $name;
}
    
  
try{

  require_once('ns_sql.php');
  $sql = new ns_sql;
  $sql->connect($central_sql_hostname,$central_sql_username,$central_sql_password,"");
  $query = "SHOW DATABASES";
  $sql->get_row($query,$dbs);
  $database_choices = array();
  if(!isset($database_blacklist)){
    $database_blacklist = array();
  }
  $database_blacklist["mysql"] = 1;
  $database_blacklist["test"] = 1;
  $database_blacklist["information_schema"] = 1;
  $database_blacklist["image_server_buffer"] = 1;
  for ($i=0; $i < sizeof($dbs);$i++){
    if (isset($database_blacklist[$dbs[$i][0]]))
	continue;
	array_push($database_choices,$dbs[$i][0]);
  }

  //var_dump($dbs);
 // var_dump($database_choices);
  //die();
  if (!isset($_COOKIE['ns_image_server_db_name'])){
	$db_name = $database_choices[0];
	foreach ($database_choices as $c){
		//echo $c;
		if ($default_database == $c){
		  $db_name = $c;
		 
		 // die("Found $db_name");
		  break;
		}
	}
   //die("$db_name");
    ns_set_database_name($db_name);
}
  else
    $db_name = $_COOKIE['ns_image_server_db_name'];
  
  $query = "USE " . $db_name;
  $sql->send_query($query);

  parse_str($_SERVER['QUERY_STRING'], $query_string );
 
  if (isset($query_string['db_name']))
      $database_choices = $query_string['db_name'];
 


}
catch(ns_exception $e){
	die($e->text);

}
function ns_slash($str){
	$j = 0;
	$slashed = false;
	$ret = $str;
	for ($i = 0; $i < strlen($str); $i++){
		if ($slashed){
			if ($str[$i] != '\\'){
				$slashed = false;
				$ret[$j] = '\\';
				$ret[$j+1] = $str[$i];
				$j+=2;
			}
		}
		else{
			if ($str[$i] == '\\') $slashed = true;
			else {
				if ($str[$i] == '"' || $str[$i] == '\''){
					$ret[$j] = '\\';
					$ret[$j+1] = $str[$i];
					$j+=2;
				}
				else{
					$ret[$j] = $str[$i];
					$j++;
				}
			}
		}
	}
	return substr($ret,0,$j);
}


//The number of times a command should be retried before giving up
$fail_limit = 15;

//local scan output directory
//require_once("long_term_storage_directories.txt");
//$ns_image_server_storage_directory= "/long_term_storage";
//$ns_image_server_storage_directory_absolute= "/mnt/sysbio/FONTANA/stroustrup/image_server_storage";

//scan url
//$scan_url = "../scan_output/";

function ns_current_time(){
	return time();
}
function format_time($t){
	if ($t == 0) return "(Never)";
	return date("H:i ",$t) . date("m-d-Y ",$t);
}

function ns_expand_unix_timestamp($time, &$minute, &$hour,&$day,&$month,&$year){
	//die ($time);
  if ($time == 0){
	$minute = "";
	$hour = "";
	$day = "";
	$month = "";
	$year = "";
	return;
  }
  $d = getdate($time);
  $minute = $d['minutes'];
  $hour = $d['hours'];
  $day = $d['mday'];
  $month = $d['mon'];
  $year = $d['year'];
}

function ns_view_image_link($image_id, $link_text = '(View)'){
	if ($image_id == 0 || $image_id == '')
		return "(none)";
	return "<a href=\"ns_view_image.php?image_id=$image_id\">$link_text</a>";
}

function ns_view_captured_image_link($captured_image_id, $link_text = '(View)'){
        if ($captured_image_id == 0 || $captured_image_id == '')
                return "(none)";
        return "<a href=\"ns_view_image.php?captured_images_id=$captured_image_id\">$link_text</a>";
}




//	output_editable_field('sd', 	   k,      TRUE,  30,       TRUE,            $height);
function output_editable_field($field_name,$value, $edit, $width='',$text_area=FALSE,$text_height=4){
	if ($edit==TRUE){
		if (!$text_area){
			echo "<input name=\"$field_name\" type=\"text\" value=\"" . str_replace('"','&quot;',$value)."\"";
			if ($width != '') echo "size=\"$width\"";
			echo ">";
		}
		else{
		  if ($width == '')
		    $width = 30;
		echo "<textarea name=\"$field_name\" cols=\"$width\" rows=\"$text_height\">".str_replace('"','&quot;',$value)."</textarea>";
		}
	}
	else  echo $value;
}


function delete_image($image_id,&$sql){
	global $ns_image_server_storage_directory_absolute;
	$query = "SELECT path, filename FROM images WHERE id='$image_id'";
	$sql->get_row($query,$res);
	if (sizeof($res) == 0)
		return;
	$filename = $ns_image_server_storage_directory_absolute . "/" . $res[0][0] . "/" . $res[0][1];
	
	delete_file($filename);
	$query =  "DELETE FROM images WHERE id='$image_id'";
	$sql->send_query($query);
}

function delete_mask($mask_id, &$sql){
	$query = "SELECT image_id FROM image_masks WHERE id = '$mask_id'";
	$sql->get_row($query,$res);
	if (sizeof($res) == 0)
		return;
	if ($res[0][0]!=0)
		delete_image($res[0][0],$sql);
	$query = "DELETE FROM image_masks WHERE id = '$mask_id'";
	$sql->send_query($query);
	$query = "UPDATE capture_samples SET mask_id='0' WHERE mask_id='$mask_id'";
	$sql->send_query($query);
}

function ns_display_megabytes($i,$display_mb=FALSE){
	if ($display_mb===TRUE) $mb = "Mb";
	if ($i === 0) return "";
	if ($i < 1024) return ("$i" . $mb);
	if ($i < 1024*1024) return round($i/1024,1) . "Gb";
	return round($i/(1024*1024),1) . "Tb";
}

//$table_colors = array(array("#F5F5F5","#D2E2EF"), array("#FFFFFF","#E5EFFF"));
//$table_header_color = " bgcolor=\"#BBBBBB\"";

function ns_output_database_selector($name,$db_choice,$submit_immediately=TRUE){
	echo "<SELECT name='$name' ";
	if ($submit_immediately)
	echo " onchange='this.form.submit()'";
	echo ">";
	
	global $database_choices;
	foreach($database_choices as $o ){
	echo "<option value=\"$o\"";
	if ($o == $db_choice) echo " selected=\"yes\"";
	echo ">$o</option>\n";
	}
	echo "</select>";
}

function display_worm_page_header($title, $link = "<a href=\"view_experiments.php\">[Back to Experiment Index]</a>",$display_db_choice=FALSE){
	global $db_name;
	?><!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html4/loose.dtd">
<html>
<head>
<title>The Lifespan Machine - <?php echo $title?></title>
<link rel="icon" type="image/vnd.microsoft.icon" href="../server_icon.ico">
<meta http-equiv="Content-Type" content="text/html; charset=iso-8859-1">
<META HTTP-EQUIV="EXPIRES" CONTENT="0">
<META HTTP-EQUIV="CACHE-CONTROL" CONTENT="NO-CACHE">
<META HTTP-EQUIV="PRAGMA" CONTENT="NO-CACHE">
<style type="text/css">
<!--
.style1 {
	font-family: Arial, Helvetica, sans-serif;
	font-size: 18px;
	font-weight: bold;
}
  a {
    text-decoration: none;
}
  a:visited {color: #000055;}
body {
	background-color: #555566;
}
.style2 {
	font-family: Arial, Helvetica, sans-serif;
	font-size: 12px
}
-->
</style>
<script>
		function report_video_failure(e){
			switch(e.target.error.code){
				case e.target.error.MEDIA_ERR_ABORTED:
					alert('You aborted the video playback.');
					break;
				case e.target.error.MEDIA_ERR_NETWORK:
					alert('A network error caused the video download tofail part-way');
					break;
				case e.target.error.MEDIA_ERRO_DECODE:
					alert('The video playback was aborted due to a corrpution problem or because the video used features your browser did not support');
					break;
				case e.target.error.MEDIA_ERR_SRC_NOT_SUPPORTED:
					alert('The video could not be loaded, either because the server or network failed or because the format is not supported');
					break;
				default:
					alert('An unkown error occurred.');
			}
		}


		</script>
<script language="JavaScript">
		    function ns_check_all(theForm,cName,status){
  for (i=0,n=theForm.elements.length; i<n; i++){
    if (theForm.elements[i].className.indexOf(cName)!=-1){
      theForm.elements[i].checked = status;
    }
  }

}
</script>
</head>
<body>
<table width="1200" border="0" cellspacing="2" cellpadding="2" align="center">
  <tr>
    <td bgcolor="#FFFFFF">
<table width="100%" border="0" cellpadding="6" cellspacing="0">
  <tr bgcolor="#dbdbdb">
    <td width="0%" bgcolor="#dbdbdb"><img src="w1.png" width=47 height=41></td>
    <td bgcolor="#dbdbdb"><span class="style1">The Lifespan Machine<br>
<table border="0" cellspacing="0" cellpadding="0">
  <tr><td><img src="../subsection.png"></td><td><?php echo $title?></td></tr></table></span></td>
    <td bgcolor="#dbdbdb"><div align="right">
		    <?php if ($display_db_choice){?>
<form action="view_experiments.php" method="POST">
		    Database: <?php ns_output_database_selector('requested_db_name',$db_name);?>
<input type="hidden" name="db_name_set" value="1">
<input type="hidden" name="db_ref" value="<?php echo 'http://'. $_SERVER['SERVER_NAME'].$_SERVER['REQUEST_URI']?>">
</form>
		    <?php }?>
<?php echo $link?></div></td>

    <td width="0%" bgcolor="#dbdbdb" width="50"></td>
  </tr>
  <tr>
    <td width="0%" bgcolor="#dbdbdb" valign="top" ><br><br><br><br><br><br><br><br><br><br><div align="right"><img src="w2.png" width=26 height=57></div></td>
    <td width="100%" colspan="2" bgcolor="#FFFFFF">
<!--Begin Content-->
<?php
}
function display_worm_page_footer(){
  global $website_version;
?> <!--End Content-->
</td>
    <td width="0%" bgcolor="#dbdbdb" valign="bottom"><img src="w3.png" width=26 height=57><br><br></td>
  </tr>
  <tr>
    <td width="0%" bgcolor="#dbdbdb">&nbsp;</td>
    <td bgcolor="#dbdbdb">&nbsp;</td>
    <td bgcolor="#dbdbdb"><div align="right"><span class="style2"><?php echo format_time(time())?>
<br>
<br>
				    Lifespan Machine web interface v.<?php echo $website_version?> <br> Nicholas Stroustrup (2013)<br>Harvard Systems Biology </span></div></td>
    <td bgcolor="#dbdbdb">&nbsp;</td>
  </tr>
</table>

</td>
  </tr>
</table>
</body><br>
<META HTTP-EQUIV="EXPIRES" CONTENT="0">
<META HTTP-EQUIV="CACHE-CONTROL" CONTENT="NO-CACHE">
<META HTTP-EQUIV="PRAGMA" CONTENT="NO-CACHE">
</html>

<?php
}
?>
