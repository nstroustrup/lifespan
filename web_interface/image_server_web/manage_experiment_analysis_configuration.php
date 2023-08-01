<?php
require_once('worm_environment.php');
require_once('ns_dir.php');
//require_once('ns_external_interface.php');
require_once('ns_experiment.php');
require_once('ns_processing_job.php');

function output_model_choice($field_name,$model_selected,$selected_model_analysis_method,$analysis_step,$disabled,&$sql){
	global $db_name, $ns_posture_analysis_model_type,$ns_worm_detection_model_type;
	$check_analysis_type = false;
	if ($analysis_step == 'posture'){
		$model_type_names = $ns_posture_analysis_model_type;
		$check_analysis_type = true;
	}
	else $model_type_names = $ns_worm_detection_model_type;
	$query = "SELECT * FROM information_schema.tables WHERE table_name = 'analysis_model_registry' AND table_schema = '$db_name'";
	$sql->get_row($query,$res);
	if (sizeof($res)==0){
		echo "No model registry exists. Update your sql db.";
		return;
	}
	$query = "SELECT name, version, analysis_method FROM analysis_model_registry WHERE analysis_step='$analysis_step'";
	$sql->get_row($query,$res);
	$model_tree = array();
	for ($i = 0; $i < sizeof($res); $i++){
		$model_tree[$model_type_names[$res[$i][2]]] = array();
	}
	$model_tree = array_reverse($model_tree);
	for ($i = 0; $i < sizeof($res); $i++)
                array_push($model_tree[$model_type_names[$res[$i][2]]],$res[$i]);
        //echo "zzz".$model_analysis_method."zzz";
	echo "<select name=\"$field_name\"";
	if ($disabled) echo "disabled";
	echo ">\n";
	foreach($model_tree as $model_type=>&$models){
		echo "<optgroup label=\"$model_type\">\n";
		foreach($models as $model){
		#echo '"'.$model[2].'"' . $model_analysis_method;
			echo '<option value="' . $model[0] ;
			if ($check_analysis_type)echo "=".$model[2];
			echo '"';
			if ($model_selected == $model[0] && (!$check_analysis_type ||$selected_model_analysis_method == $model[2]))
				echo " selected";
			echo '>' . $model[0] ." (" . $model[1] . ")</option>\n";
		}
		echo "</optgroup>\n";
	}
	echo "</select>";
}

try{
if (!ns_param_spec($query_string,'experiment_id'))
	throw new ns_exception('No experiment id specified!');
else $experiment_id = (int)$query_string['experiment_id'];


if ($experiment_id != 0){

	$query = "SELECT name FROM experiments WHERE id = $experiment_id";
	$sql->get_row($query,$enames);
	$experiment_name = $enames[0][0];
}
 $page_title = "Machine Analysis Configuration for " . $experiment_name;

   $query_parameters = "experiment_id=$experiment_id";
$external_detail_spec = FALSE;
if (ns_param_spec($_POST,'detail_level')){
     $detail_level = $_POST['detail_level'];
     $external_detail_spec = TRUE;
 }



if (ns_param_spec($query_string,'reset_model_registry')){
   $query = "DELETE from analysis_model_registry WHERE analysis_step='posture'";
   $sql->send_query($query);
   header("Location: manage_experiment_analysis_configuration.php?$query_parameters\n\n");
}
if (ns_param_spec($_POST,'set_as_default'))
   $set_as_default = $_POST['set_as_default']=="yes";
else $set_as_default = FALSE;

 if (ns_param_spec($query_string,'set_denoising_options') && $query_string['set_denoising_options']==1){
 $no_region_info = FALSE;
   if (!array_key_exists('time_series_median',$_POST) &&
       !array_key_exists('maximum_number_of_worms',$_POST)){
       $no_region_info = TRUE;
   }else{
   if (array_key_exists('time_series_median',$_POST))
   $denoising_flag = $_POST['time_series_median'];
   else $denoising_flag = 0;


   $number_of_stationary_images = $_POST['number_of_stationary_images'];
   }
    	$end_minute = (int)$_POST['end_minute'];
    	$end_hour = (int)$_POST['end_hour'];
    	$end_day = (int)$_POST['end_day'];
    	$end_month = (int)$_POST['end_month'];
    	$end_year = (int)$_POST['end_year'];
  $mask_date = mktime(intval($end_hour), intval($end_minute), 0, intval($end_month), intval($end_day), intval($end_year));
   if ($end_month == 0 || $end_day==0 || $end_year == 0){
      $mask_date = 0;
      }
   $image_compression = $_POST['image_compression'];
   $image_compression_ratio = $_POST['image_compression_ratio'];
   $conversion_16_bit_upper_bound = $_POST['conversion_16_bit_upper_bound'];

   $conversion_16_bit_lower_bound = $_POST['conversion_16_bit_lower_bound'];
   if ($conversion_16_bit_upper_bound > 255 || $conversion_16_bit_lower_bound > 256){
      throw new ns_exception("All 16 bit conversion range values must fall between 0 and 256");
   }
   if ($conversion_16_bit_upper_bound != 0 && ($conversion_16_bit_upper_bound - $conversion_16_bit_lower_bound < 128))
      throw new ns_exception("The conversion range maps 16 pixels between (256*lower,256*upper) to 8 bit pixels (0,255).  So it is never sensible to set the upper bound below 128, the lower bound above 128, or the upper and lower bounds closer together than 128");
   $apply_vertical_image_registration = $_POST['apply_vertical_image_registration'] == "apply";
   $maximum_number_of_worms = @$_POST['maximum_number_of_worms'];
   $delete_captured_images = $_POST['delete_captured_images'] == "delete";
   //  die($_POST['delete_captured_images']);
   if (!$no_region_info){
      $query = "UPDATE sample_region_image_info as r, capture_samples as s SET r.maximum_number_of_worms_per_plate=$maximum_number_of_worms, time_series_denoising_flag=$denoising_flag WHERE r.sample_id = s.id AND s.experiment_id = $experiment_id";
      $sql->send_query($query);
   }
      $query = "UPDATE experiments SET delete_captured_images_after_mask=" . ($delete_captured_images?"1":"0") . ", compression_type='$image_compression',mask_time=$mask_date WHERE id = $experiment_id";

   $sql->send_query($query);

   $query = "UPDATE capture_samples SET apply_vertical_image_registration=" . ($apply_vertical_image_registration?"1":"0"). ", conversion_16_bit_low_bound=".(string)((int)$conversion_16_bit_lower_bound).",conversion_16_bit_high_bound=".(string)((int)$conversion_16_bit_upper_bound) ." WHERE experiment_id = $experiment_id";
   // die($query);
   $sql->send_query($query);
   if ($image_compression != 'lzw' && $image_compression_ratio !== '' && $image_compression_ratio !== 0){
      $query = "UPDATE experiments SET compression_ratio=$image_compression_ratio WHERE id = $experiment_id";
      $sql->send_query($query);

   }

   header("Location: manage_experiment_analysis_configuration.php?$query_parameters\n\n");

 }
 if (ns_param_spec($query_string,'regression_setup') && $query_string['regression_setup'] == 1){

   if ($_POST['set_control_strain']){
     $region_id = $_POST['control_region_id'];
     $control_lifespan = $_POST['control_lifespan'];
     //	var_dump($region_id);
     //	die($region_id);
     if ($region_id == 'none')
       $region_id = '';
     $control_string = $detail_level . ";" . implode(";",$region_id) . ";" . $control_lifespan;
     //	die($control_string);
     $query = "UPDATE experiments SET control_strain_for_device_regression = '" . $control_string . "' WHERE id = " . $experiment_id;
     //echo $query

     $sql->send_query($query);
     header("Location: manage_experiment_analysis_configuration.php?$query_parameters\n\n");
   }
 }
 $query = "SELECT apply_vertical_image_registration, conversion_16_bit_low_bound,conversion_16_bit_high_bound FROM capture_samples WHERE experiment_id = " . $experiment_id;
 $sql->get_row($query,$vir);
 $apply_vertical_image_registration = $vir[0][0];
$conversion_16_bit_lower_bound = $vir[0][1];
$conversion_16_bit_upper_bound = $vir[0][2];
 //var_dump($vir);
 //die();
 $query = "SELECT mask_time, compression_type,compression_ratio FROM experiments WHERE id=$experiment_id";
 $sql->get_row($query,$res);
//var_dump($res);
 if(sizeof($res) != 0){
   $mask_date = $res[0][0];
   $image_compression = $res[0][1];

   $image_compression_ratio = $res[0][2];
}
 else{
 die("Could not find experiment info in db");
}
 //die("".$mask_date);
 $query = "SELECT delete_captured_images_after_mask FROM experiments WHERE id = " . $experiment_id;
 $sql->get_row($query,$dci);
 $delete_captured_images = $dci[0][0];
 $query = "SELECT control_strain_for_device_regression FROM experiments WHERE id = " . $experiment_id;
 $sql->get_row($query,$exps);

 $cs = $exps[0][0];
 //die ($cs);
 $i = strpos($cs,";");
 if ($i === FALSE)
   $d = 's123';
 else $d = substr($cs,0,$i);
 if (!$external_detail_spec)
   $detail_level = $d;
 $control_regions = explode(";",substr($cs,$i));
 for ($i = 0; $i < sizeof($control_regions)-1; $i++)
   $control_region_hash[$control_regions[$i]] = '1';
 if (sizeof($control_regions) >= 1)
   $control_lifespan = $control_regions[sizeof($control_regions)-1];

 $strain_posture_models = array();
 $strain_detection_models = array();
 $strain_position_models = array();

 $region_strains = array();
 //die("Control Regions" . implode(".",$control_regions));
 $query = "SELECT r.id,r.strain, r.strain_condition_1,r.strain_condition_2,r.strain_condition_3, r.posture_analysis_model, r.posture_analysis_method, r.worm_detection_model, r.position_analysis_model,r.time_series_denoising_flag, r.maximum_number_of_worms_per_plate,r.number_of_frames_used_to_mask_stationary_objects"
   ." FROM sample_region_image_info as r, capture_samples as s "
   ."WHERE r.sample_id = s.id AND s.experiment_id = " . $experiment_id . " AND r.censored = 0";
 $sql->get_row($query,$exps);

 $number_of_regions = sizeof($exps);
if ($number_of_regions>0){
 $time_series_denoising_flag = $exps[0][9];
 $maximum_number_of_worms = $exps[0][10];
 $number_of_stationary_images = $exps[0][11];
}else{
$time_series_denoising_flag = "";
$maximum_number_of_worms = "";
$number_of_stationary_images="";
}

$posture_analysis_method = '';
 $experiment_strains = array();
 for ($i = 0; $i < sizeof($exps); $i++){
   /*  for ($j=0;$j < sizeof($exps[$i]); $j++)
     echo $exps[$i][$j] . " ";
   echo "<BR>";*/
$strain = $exps[$i][1];
   //echo $detail_level;
   if ($detail_level != 's'){
     if ($exps[$i][2] != '')
       $strain .= "::" . $exps[$i][2];
     if ($detail_level != 's1'){
       if ($exps[$i][3] != '')
	 $strain .= "::" . $exps[$i][3];
     }
     if ($detail_level != 's12'){
       if ($exps[$i][4] != '')
	 $strain .= "::" . $exps[$i][4];
     }
   }
   // echo $strain;

   $region_strains[$exps[$i][0]] = array($exps[$i][1],$exps[$i][2],$exps[$i][3],$exps[$i][4]);

   if (!array_key_exists($strain,$strain_posture_models) || $strain_posture_models[$strain] == '')
     $strain_posture_models[$strain] = array($exps[$i][5],$exps[$i][6]);
   if (!array_key_exists($strain,$strain_detection_models) || $strain_detection_models[$strain] == '')
     $strain_detection_models[$strain] = $exps[$i][7];

   $experiment_strains[$strain] = $exps[$i][0];

   if ($posture_analysis_method == '')
     $posture_analysis_method = $exps[$i][6];

   if (!array_key_exists($strain,$strain_position_models) || $strain_position_models[$strain] =='')
     $strain_position_models[$strain] = $exps[$i][8];

   if (isset($control_region_hash[$exps[$i][0]]))
     $control_strain_hash[$strain]= '1';
 }

 if (ns_param_spec($_POST,'is_single_posture_model') && $_POST['is_single_posture_model']=="0"){
   $is_single_posture_model = false;
   // die("WHA");
 }
 else{
$is_single_posture_model = true;
   $single_posture_model_name = "";
   foreach ($strain_posture_models as $s => $m){
    //	var_dump($m);
	 if ($single_posture_model_name === ""){
       $single_posture_model_name = $m[0];
	$single_posture_model_method = $m[1];
}
     else if ($single_posture_model_name != $m[0]){
       $is_single_posture_model = FALSE;
       break;
     }
   }
   //die("" . $is_single_posture_model);
 }
 if (ns_param_spec($_POST,'is_single_position_model') && $_POST['is_single_position_model']=="0"){
   $is_single_position_model = false;
   $single_position_model_name="";
}
 else{

   $is_single_position_model = true;
   $single_position_model_name = "";
   //var_dump($strain_position_models);
   //die("");
   foreach ($strain_position_models as $s => $m){
     if ($single_position_model_name === ""){
       $single_position_model_name = $m;


     }
     else if ($single_position_model_name != $m){
       //   die("e" . $single_position_model_name . "e");
       $is_single_position_model = false;
       break;
     }
   }
 }
 if (ns_param_spec($_POST,'is_single_detection_model') && $_POST['is_single_detection_model']=="0"){
   $is_single_detection_model = false;
   $single_detection_model_name="";
   //die("WHA");
 }
 else{
   $is_single_detection_model = true;
   $single_detection_model_name = "";
   foreach ($strain_detection_models as $s => $m){
     //    echo $s . " " . $m . "<BR>";
     if      ($single_detection_model_name === "")
              $single_detection_model_name = $m;
     else if ( $single_detection_model_name != $m){

       $is_single_detection_model = FALSE;
       break;
     }
   }
 }


 if (ns_param_spec_true($_POST,'set_posture_models')){
   if ($is_single_posture_model){
     $model_name = $_POST['single_posture_model_name'];
     $pos = strrpos($model_name,"=");
     $posture_analysis_method = substr($model_name,$pos+1);
     $model_name = substr($model_name,0,$pos);

     //die($posture_analysis_method);
     $query = "UPDATE sample_region_image_info as i, capture_samples as s SET posture_analysis_model ='$model_name',posture_analysis_method='$posture_analysis_method' WHERE i.sample_id = s.id AND s.experiment_id = $experiment_id";
     // die($query);
     $sql->send_query($query);
     if ($set_as_default){
     	$query = "UPDATE constants SET v='$posture_analysis_method' WHERE k='default_posture_analysis_method'";
     	$sql->send_query($query);
     	$query = "UPDATE constants SET v='$model_name' WHERE k='default_posture_analysis_model'";
     	$sql->send_query($query);
     }
   }
   else{
     foreach($_POST as $k => $v){
       if (substr($k,0,14) == "posture_model_"){
	 $region_id = substr($k,14);
	 $st = $region_strains[$region_id];
	 $model = $v;
	 $strain_condition = "i.strain = '" .$st[0] . "'";
	 if ($detail_level != 's'){

	   $strain_condition .= " AND i.strain_condition_1 = '" . $st[1] . "'";
	   if ($detail_level != 's1'){
	     $strain_condition .=" AND i.strain_condition_2 = '" . $st[2] . "'";
	   }
	 }
	 //	 	 echo $region_id . ": " . $model . "<br>";

     $pos = strrpos($model,"=");
     $posture_analysis_method = substr($model,$pos+1);
     $model = substr($model,0,$pos);
	$query =  " UPDATE sample_region_image_info as i, capture_samples as s SET posture_analysis_model ='$model',posture_analysis_method='$posture_analysis_method' WHERE $strain_condition AND i.sample_id = s.id AND s.experiment_id = $experiment_id";
	//		echo $query . "<BR>";

	$sql->send_query($query);
       }

     }
   }
     header("Location: manage_experiment_analysis_configuration.php?$query_parameters\n\n");

 }

 if (ns_param_spec_true($_POST,'set_position_models')){
   if ($is_single_position_model){
    $model_name = $_POST['single_position_model_name'];
     $query = "UPDATE sample_region_image_info as i, capture_samples as s SET position_analysis_model ='$model_name' WHERE i.sample_id = s.id AND s.experiment_id = $experiment_id";
      $sql->send_query($query);

       if ($set_as_default){
        $query = "UPDATE constants SET v='$model_name' WHERE k='default_position_analysis_model'";
        $sql->send_query($query);
     }
   }
   else{
     var_dump($_POST);
     //    die("");
     foreach($_POST as $k => $v){
       if (substr($k,0,14) == "position_model"){
	 $region_id = substr($k,14);
	 $st = $region_strains[$region_id];
	 $model = $v;
	 $strain_condition = "i.strain = '" .$st[0] . "'";
	 if ($detail_level != 's'){

	   $strain_condition .= " AND i.strain_condition_1 = '" . $st[1] . "'";
	   if ($detail_level != 's1'){
	     $strain_condition .=" AND i.strain_condition_2 = '" . $st[2] . "'";
	   }
	 }
	 //	 	 echo $region_id . ": " . $model . "<br>";
	$query =  " UPDATE sample_region_image_info as i, capture_samples as s SET position_analysis_model ='$model'  WHERE $strain_condition AND i.sample_id = s.id AND s.experiment_id = $experiment_id";
	//		echo $query . "<BR>";

	$sql->send_query($query);
       }

     }
     //   die("");
   }
   header("Location: manage_experiment_analysis_configuration.php?$query_parameters\n\n");
 }


 if (ns_param_spec_true($_POST,'set_detection_models')){
   if ($is_single_detection_model){
     $model_name = $_POST['single_detection_model_name'];
     $query = "UPDATE sample_region_image_info as i, capture_samples as s SET worm_detection_model ='$model_name' WHERE i.sample_id = s.id AND s.experiment_id = $experiment_id";

   $sql->send_query($query);
      if ($set_as_default){
        $query = "UPDATE constants SET v='$model_name' WHERE k='default_worm_detection_model'";
        $sql->send_query($query);
     }
   }
   else{
     foreach($_POST as $k => $v){
       //  echo $k . "<br>";
       if (substr($k,0,16) == "detection_modelZ"){
	 $region_id = substr($k,16);
	 $st = $region_strains[$region_id];

	 $model = $v;

	 $strain_condition = "i.strain = '" .$st[0] . "'";
   //echo $detail_level;
	 if ($detail_level != 's'){

	   $strain_condition .= " AND i.strain_condition_1 = '" . $st[1] . "'";
	   if ($detail_level != 's1'){
	     $strain_condition .=" AND i.strain_condition_2 = '" . $st[2] . "'";
	   }
	 }
	 //echo $region_id . ": " . $model . "<br>";
	$query =  " UPDATE sample_region_image_info as i, capture_samples as s SET worm_detection_model ='$model'  WHERE $strain_condition AND i.sample_id = s.id AND s.experiment_id = $experiment_id";
	//echo $query . "<br>";//
	$sql->send_query($query);
       }
       // die("");

     }
   }
   //   die("");
     header("Location: manage_experiment_analysis_configuration.php?$query_parameters\n\n");

 }
 $back_url = "manage_samples.php?experiment_id=$experiment_id";
 display_worm_page_header($page_title, "<a href=\"$back_url\">[Back to {$experiment_name}]</a>");
//var_dump($control_strain_hash);
}
catch(ns_exception $ex){
	die("Error: " . $ex->text);
}
?>

<table cellspacing = 5 cellpadding=0 border=0 align="center"><tr><td valign="top">


<form action="manage_experiment_analysis_configuration.php?<?php echo $query_parameters . "&set_denoising_options=1"?>" method="post">

<table align="center" border="0" cellpadding="0" cellspacing="1" bgcolor="#000000"><tr><td><table border="0" cellpadding="4" cellspacing="0" width="100%"><tr <?php echo $table_header_color?> ><td colspan=2><b>Image Analysis Options</b></td></tr>
<!--
<tr><td bgcolor="<?php echo $table_colors[1][0] ?>">Time Series Denoising</td><td bgcolor="<?php echo $table_colors[1][1] ?>">

<select name="time_series_median" style="width: 245px"
<?php
    if($number_of_regions==0) echo " disabled ";
    echo ">";
    foreach($ns_denoising_option_labels as $l => $v){
      echo "<option value=\"" . $v . "\" ";
      if($number_of_regions > 0 && $time_series_denoising_flag == $v) echo "selected ";
      if ($number_of_regions == 0)
      	 echo "disabled ";
      echo ">";
      echo  $l . "</option>\n";
    }
?>
</select></td></tr>
-->
<tr><td bgcolor="<?php echo $table_colors[0][0] ?>">Apply Vertical Image Registration</td><td bgcolor="<?php echo $table_colors[0][1] ?>">

<select name="apply_vertical_image_registration">
<option value="do_not_apply"<?php if ($apply_vertical_image_registration == '0') echo "selected"?> >Do not apply vertical registration</option>
<option value="apply" <?php if ($apply_vertical_image_registration == '1') echo "selected"?> >Appy vertical registration</option>
</select></td></tr>

<tr><td bgcolor="<?php echo $table_colors[1][0] ?>">Delete Captured Images after Masking</td><td bgcolor="<?php echo $table_colors[1][1] ?>">

<select name="delete_captured_images">
<option value="do_not_delete"<?php if ($delete_captured_images == '0') echo "selected"?> >Do not delete captured images</option>
<option value="delete" <?php if ($delete_captured_images == '1') echo "selected"?> >Delete Captured Images</option>
</select></td></tr>

<tr><td bgcolor="<?php echo $table_colors[0][0] ?>">Maximum Number of Worms per Plate</td><td bgcolor="<?php echo $table_colors[0][1] ?>">
	<?php output_editable_field("maximum_number_of_worms",$maximum_number_of_worms,$number_of_regions>0,5);?></td></tr>
		<!--
	<tr><td bgcolor="<?php echo $table_colors[1][0] ?>">Number of images used to detect stationary plate features <br>(0 for automatic)</td><td bgcolor="<?php echo $table_colors[1][1] ?>"><?php output_editable_field("number_of_stationary_images",$number_of_stationary_images,$number_of_regions>0,5);?>
</td></tr>-->
		<tr><td bgcolor="<?php echo $table_colors[0][0] ?>">Date and time of images to use <br> when generating plate region mask</td><td bgcolor="<?php echo $table_colors[0][1]?>">
<?php
	ns_expand_unix_timestamp($mask_date,$i,$h,$d,$m,$y);
					output_editable_field("end_hour",$h,TRUE,2);
					echo ":";
					output_editable_field("end_minute",$i,TRUE,2);
					echo "<br>";
					output_editable_field("end_month",$m,TRUE,2);
					echo "/";
					output_editable_field("end_day",$d,TRUE,2);
					echo "/";
					output_editable_field("end_year",$y,TRUE,4);
?>
<?php if ($image_compression == 'lzw'){?>
<input type="hidden" name="image_compression_ratio" value="<?php echo $image_compression_ratio?>">
<?php }
?>
</td></tr>
<tr><td bgcolor="<?php echo $table_colors[1][0] ?>">Image Compression</td><td bgcolor="<?php echo $table_colors[1][1] ?>">

<select name="image_compression">
<option value="lzw"<?php if ($image_compression == 'lzw') echo "selected"?> >Lossless</option>
<option value="jp2k" <?php if ($image_compression == 'jp2k') echo "selected"?> >jpeg2000 (recommended)</option>
</select></td></tr>
<?php if ($image_compression != 'lzw'){?>
<tr><td bgcolor="<?php echo $table_colors[0][0] ?>">Compression ratio</td><td bgcolor="<?php echo $table_colors[0][1] ?>">
<?php
output_editable_field("image_compression_ratio",$image_compression_ratio,TRUE,4);?></td></tr>
<?php } ?>
<tr><td bgcolor="<?php echo $table_colors[1][0] ?>">16 to 8 bit conversion range</td><td bgcolor="<?php echo $table_colors[1][1] ?>">
<?php
output_editable_field("conversion_16_bit_lower_bound",$conversion_16_bit_lower_bound,FALSE,4);
echo "-";
output_editable_field("conversion_16_bit_upper_bound",$conversion_16_bit_upper_bound,TRUE,4);
?>&nbsp;<font size="-2">(darkest - brightest) on an 8 bit scale</font> </td></tr>
<tr><td bgcolor="<?php echo $table_colors[0][0] ?>" colspan=2>
	<div align="right"><input name="set_denoising_options" type="submit" value="Set Analysis Options">
	</div>
	</td></tr>
	</table>
	</td></tr>
	</table>
</form><br>

<?php if (0){?>
<form action="manage_experiment_analysis_configuration.php?<?php echo $query_parameters . "&regression_setup=1"?>" method="post">

<table align="center" border="0" cellpadding="0" cellspacing="1" bgcolor="#000000"><tr><td><table border="0" cellpadding="4" cellspacing="0" width="100%">
<tr <?php echo $table_header_color?> ><td colspan=2><b>Device Effect Regression Configuration</b></td></tr>
<tr><td bgcolor="<?php echo $table_colors[1][0] ?>">Strain Grouping</td><td bgcolor="<?php echo $table_colors[1][1] ?>">

<select name="detail_level" onchange='this.form.submit()'>
<option value="s" <?php if ($detail_level == 's') echo "selected"?> >Just Strain</option>
<option value="s1"<?php if ($detail_level == 's1') echo "selected"?> >Strain And Condition 1</option>
<option value="s12"<?php if ($detail_level == 's12') echo "selected"?> >Strain and Conditions 1 and 2</option>
<option value = "s123"<?php if ($detail_level == 's123' || $detail_level == '') echo "selected"?> >Strain And Conditions 1,2,and 3</option></select></td></tr>

<tr><td bgcolor="<?php echo $table_colors[0][0] ?>">Device Calibration Strain</td><TD bgcolor="<?php echo $table_colors[0][1] ?>">
	 <select name = "control_region_id[]" multiple="multiple" size="<?php echo sizeof($experiment_strains)+1?>">
<?php
    echo "<option value=\"none\"";
  if (sizeof($control_strain_hash)==0)
    echo " selected";
  echo ">No Control</option>\n";
    foreach($experiment_strains as $strain => $region_id){
      echo "<option value =\"" . $region_id."\" ";
      if ($control_strain_hash[$strain] == '1')
	echo "selected";
      echo ">" ;
      echo $strain;
      echo "</option>\n";
    }

  ?>
    </select>
</td></tr>
    <tr><TD bgcolor="<?php echo $table_colors[1][0] ?>">Device Calibration Strain <br>Lifespan Mean</td><td bgcolor="<?php echo $table_colors[1][1] ?>">
<input type="text" size=3 name="control_lifespan" value="<?php echo $control_lifespan?>"> days<font size="-2"><br>*leave blank to use grand mean</font></td></tr>
<tr><TD colspan=2 bgcolor="<?php echo $table_colors[0][0] ?>">
	<div align="right"><input name="set_control_strain" type="submit" value="Set Control Information">
	</div>
</td></tr>
	</table>
	</td></tr>
	</table>
</form><?php } ?>
	<br>

</td><td valign="top">
<form action="manage_experiment_analysis_configuration.php?<?php echo $query_parameters . "&posture_parameters=1"?>" method="post">

<table align="center" border="0" cellpadding="0" cellspacing="1" bgcolor="#000000"><tr><td><table border="0" cellpadding="4" cellspacing="0" width="100%"><tr <?php echo $table_header_color?> ><td colspan=2><b>Posture Analysis Parameter Sets</b></td></tr>

<!-- <tr><td bgcolor="<?php echo $table_colors[1][0] ?>">Posture Analysis Method</td><td bgcolor="<?php echo $table_colors[1][1] ?>">

      <select name="posture_analysis_method" <?php if ($number_of_regions == 0) echo "disabled"?>>
      <option value=""<?php if ($posture_analysis_method == '') echo "selected"?> >None Specified</option>
<option value="thr_hm" <?php if ($posture_analysis_method == 'thr_hm') echo "selected"?> >Blended Hidden Markov Model</option>
<option value="hm" <?php if ($posture_analysis_method == 'hm') echo "selected"?> >Pure Hidden Markov Model</option>
<option value="thresh"<?php if ($posture_analysis_method == 'thresh') echo "selected"?> >Old Thresholding</option>
</select></td></tr>
-->
<tr><td bgcolor="<?php echo $table_colors[0][0] ?>">Single Model for All Plates</td><TD bgcolor="<?php echo $table_colors[0][1] ?>">
						      <select name="is_single_posture_model" onchange='this.form.submit()' <?php if ($number_of_regions == 0) echo "disabled"?>><option value="1" <?php if ($is_single_posture_model)echo "selected";?>>All plates use the same model</option><option value="0" <?php if (!$is_single_posture_model)echo "selected";?>>Each strain has its own model</option></select>
							     </td></tr>
							     <?php if ($is_single_posture_model){?>

      <tr><td bgcolor="<?php echo $table_colors[0][0] ?>">All Plate Model:</td><TD bgcolor="<?php echo $table_colors[0][1] ?>">
	<?php  output_model_choice("single_posture_model_name",$single_posture_model_name,$single_posture_model_method,'posture',$number_of_regions==0,$sql); ?>
	<?php //output_editable_field("single_posture_model_name",$single_posture_model_name,$number_of_regions>0,30);-?>
							     </td></tr>
																	   <?php } else{?>

<tr><td bgcolor="<?php echo $table_colors[1][0] ?>">Strain Grouping</td><td bgcolor="<?php echo $table_colors[1][1] ?>">

<select name="detail_level" onchange='this.form.submit()'>
<option value="s" <?php if ($detail_level == 's') echo "selected"?> >Just Strain</option>
<option value="s1"<?php if ($detail_level == 's1') echo "selected"?> >Strain And Condition 1</option>
<option value="s12"<?php if ($detail_level == 's12') echo "selected"?> >Strain and Conditions 1 and 2</option>
<option value = "s123"<?php if ($detail_level == 's123' || $detail_level == '') echo "selected"?> >Strain And Conditions 1,2,and 3</option></select></td></tr>
<?php
	$c = 0;
     # var_dump($experiment_strains);
    foreach($experiment_strains as $strain => $region_id){
	echo
	"<tr><td bgcolor=\"".$table_colors[$c][0] . "\">$strain</td>" .
	"<td bgcolor=\"" . $table_colors[$c][1] . "\">";
	 output_model_choice("posture_model_" . $region_id,$strain_posture_models[$strain][0],$strain_posture_models[$strain][1],'posture',$number_of_regions==0,$sql);
	//output_editable_field("posture_model_" . $region_id ,$strain_posture_models[$strain],$number_of_regions>0,30);
	echo "</td></tr>\n";
	$c =!$c;
    }

  ?>
	<?php }?>

<tr><td valign="top" bgcolor=" <?php echo $table_colors[0][0]?>"><?php if ($is_single_posture_model){?><input type="checkbox" name="set_as_default" value="yes" <?php if ($number_of_regions==0) echo "disabled"?>><font size="-2">Set as default for
 all future experiments</font><?php } ?></td><td bgcolor="<?php echo $table_colors[0][0] ?>" colspan=1>
					  <div align="right"><input name="set_posture_models" type="submit" value="Set Posture Analysis Models" <?php if ($number_of_regions == 0) echo "disabled";?>>  <?php if ($number_of_regions == 0) echo "<br><font size=\"-2\">These options cannot be set before plate region mask is submitted.</font>"?>
	</div>
	</td></tr><tr><td colspan="2" bgcolor=" <?php echo $table_colors[0][0]?>"> <font size="-2">To add a new model, place the file the model directory and<br>re-run the worm browser or analysis server. To clear this list: click <a href="manage_experiment_analysis_configuration.php?experiment_id=<?php echo $experiment_id?>&reset_model_registry=1">[click here]</a></font></td><tr>
	</table>
	</td></tr>
	</table>
</form><br>
<form action="manage_experiment_analysis_configuration.php?<?php echo $query_parameters . "&detection_parameters=1"?>" method="post">

<table align="center" border="0" cellpadding="0" cellspacing="1" bgcolor="#000000"><tr><td><table border="0" cellpadding="4" cellspacing="0" width="100%"><tr <?php echo $table_header_color?> ><td colspan=2><b>Worm Detection Parameter Sets</b></td></tr>

<tr><td bgcolor="<?php echo $table_colors[0][0] ?>">Single Model for All Plates</td><TD bgcolor="<?php echo $table_colors[0][1] ?>">
						      <select name="is_single_detection_model" onchange='this.form.submit()' <?php if ($number_of_regions == 0) echo "disabled"?>><option value="1" <?php if ($is_single_detection_model)echo "selected";?>>All plates use the same model</option><option value="0" <?php if (!$is_single_detection_model)echo "selected";?>>Each strain has its own model</option></select>
							     </td></tr>
							     <?php if ($is_single_detection_model){?>
      <tr><td bgcolor="<?php echo $table_colors[0][0] ?>">All Plate Model:</td><TD bgcolor="<?php echo $table_colors[0][1] ?>">

	<?php
		output_model_choice("single_detection_model_name",$single_detection_model_name,'','detection',$number_of_regions==0,$sql);
	#output_editable_field("single_detection_model_name",$single_detection_model_name,$number_of_regions>0,30);?>
							     </td></tr>
																							   <?php } else{?>

<tr><td bgcolor="<?php echo $table_colors[1][0] ?>">Strain Grouping</td><td bgcolor="<?php echo $table_colors[1][1] ?>">

<select name="detail_level" onchange='this.form.submit()'>
<option value="s" <?php if ($detail_level == 's') echo "selected"?> >Just Strain</option>
<option value="s1"<?php if ($detail_level == 's1') echo "selected"?> >Strain And Condition 1</option>
<option value="s12"<?php if ($detail_level == 's12') echo "selected"?> >Strain and Conditions 1 and 2</option>
<option value = "s123"<?php if ($detail_level == 's123' || $detail_level == '') echo "selected"?> >Strain And Conditions 1,2,and 3</option></select></td></tr>

<?php
	$c = 0;
    foreach($experiment_strains as $strain => $region_id){
	echo
	"<tr><td bgcolor=\"".$table_colors[$c][0] . "\">$strain</td>" .
	"<td bgcolor=\"" . $table_colors[$c][1] . "\">";
	output_model_choice("detection_modelZ". $region_id ,$strain_detection_models[$strain],'','detection',$number_of_regions==0,$sql);
	//output_editable_field("detection_modelZ" . $region_id ,$strain_detection_models[$strain],true,30);
	echo "</td></tr>\n";
	$c =!$c;
    }

  ?>
	<?php }?>

<tr><td valign="top" bgcolor=" <?php echo $table_colors[0][0]?>"><?php if ($is_single_detection_model){?><input type="checkbox" name="set_as_default" value="yes"  <?php if ($number_of_regions==0) echo "disabled"?>><font size="-2">Set as default for all future experiments</font><?php } ?></td><td bgcolor="<?php echo $table_colors[0][0] ?>">
					  <div align="right"><input name="set_detection_models" type="submit" value="Set Worm Detection Models" <?php if ($number_of_regions == 0) echo "disabled";?>> <?php if ($number_of_regions == 0) echo "<br><font size=\"-2\">These options cannot be set before plate region mask is submitted.</font>"?>
	</div>
	</td></tr>
	<tr><td colspan="2" bgcolor=" <?php echo $table_colors[0][0]?>"> <font size="-2">To add a new model, place the file the model directory and<br>re-run the worm browser or analysis server. To clear this list: click <a href="manage_experiment_analysis_configuration.php?experiment_id=<?php echo $experiment_id?>&reset_model_registry=1">[click here]</a></font></td></tr>
	</table>
	</td></tr>
	</table>
</form>
<br>
<form action="manage_experiment_analysis_configuration.php?<?php echo $query_parameters . "&position_parameters=1"?>" method="post">

<table align="center" border="0" cellpadding="0" cellspacing="1" bgcolor="#000000"><tr><td><table border="0" cellpadding="4" cellspacing="0" width="100%"><tr <?php echo $table_header_color?> ><td colspan=2><b>Object Position Analysis Parameter Sets</b></td></tr>
																									   <tr><td colspan = 2 bgcolor="<?php echo $table_colors[1][0] ?>"><i><font size="-1">Leave blank to use default parameters (recommended)</font></td></tr>
<tr><td bgcolor="<?php echo $table_colors[0][0] ?>">Model for All Plates</td><TD bgcolor="<?php echo $table_colors[0][1] ?>">
						      <select name="is_single_position_model" onchange='this.form.submit()' <?php if ($number_of_regions == 0) echo "disabled"?>><option value="1" <?php if ($is_single_position_model)echo "selected";?>>All plates use the same model</option><option value="0" <?php if (!$is_single_position_model)echo "selected";?>>Each strain has its own model</option></select>
							     </td></tr>
							     <?php if ($is_single_position_model){?>
      <tr><td bgcolor="<?php echo $table_colors[0][0] ?>">All Plate Model:</td><TD bgcolor="<?php echo $table_colors[0][1] ?>">
	<?php output_editable_field("single_position_model_name",$single_position_model_name,$number_of_regions>0,30);?>
							     </td></tr>
																							   <?php } else{?>

<tr><td bgcolor="<?php echo $table_colors[1][0] ?>">Strain Grouping</td><td bgcolor="<?php echo $table_colors[1][1] ?>">

<select name="detail_level" onchange='this.form.submit()'>
<option value="s" <?php if ($detail_level == 's') echo "selected"?> >Just Strain</option>
<option value="s1"<?php if ($detail_level == 's1') echo "selected"?> >Strain And Condition 1</option>
<option value="s12"<?php if ($detail_level == 's12') echo "selected"?> >Strain and Conditions 1 and 2</option>
<option value = "s123"<?php if ($detail_level == 's123' || $detail_level == '') echo "selected"?> >Strain And Conditions 1,2,and 3</option></select></td></tr>

<?php
	$c = 0;
    foreach($experiment_strains as $strain => $region_id){
	echo
	"<tr><td bgcolor=\"".$table_colors[$c][0] . "\">$strain</td>" .
	"<td bgcolor=\"" . $table_colors[$c][1] . "\">";
	output_editable_field("position_modelZ" . $region_id ,$strain_position_models[$strain],true,30);
	echo "</td></tr>\n";
	$c =!$c;
    }

  ?>
	<?php }?>

<tr><td valign="top" bgcolor=" <?php echo $table_colors[0][0]?>"><input type="checkbox" name="set_as_default" value="yes"  <?php if ($number_of_regions==0) echo "disabled"?>><font size="-2">Set as default for
 all future experiments</font></td><td bgcolor="<?php echo $table_colors[0][0] ?>" colspan=2>
					  <div align="right"><input name="set_position_models" type="submit" value="Set Position Models" <?php if ($number_of_regions == 0) echo "disabled";?>> <?php if ($number_of_regions == 0) echo "<br><font size=\"-2\">These options cannot be set before plate region mask is submitted.</font>"?>
	</div>
	</td></tr>
	</table>
	</td></tr>
	</table>
</form>
</td></tr></table>
<?php
display_worm_page_footer();
?>
