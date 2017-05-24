<?php
require_once('worm_environment.php');
require_once('ns_dir.php');
//require_once('ns_external_interface.php');
require_once('ns_experiment.php');
require_once('ns_processing_job.php');

try{

if (!ns_param_spec($query_string,'experiment_id')))
	throw new ns_exception('No experiment id specified!');
$experiment_id = @(int)$query_string['experiment_id'];


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
 if (ns_param_spec($query_string,'regression_setup') && $query_string['regression_setup'] == 1){

   if (ns_param_spec($_POST,'set_control_strain')){
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
     header("Location: manage_experiment_controls.php?$query_parameters\n\n");
   }
 }

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

 $strain_models = array();
 $region_strains = array();
 //die("Control Regions" . implode(".",$control_regions));
 $query = "SELECT r.id,r.strain, r.strain_condition_1,r.strain_condition_2,r.strain_condition_3, r.posture_analysis_model, r.posture_analysis_method"
   ." FROM sample_region_image_info as r, capture_samples as s "
   ."WHERE r.sample_id = s.id AND s.experiment_id = " . $experiment_id;
 $sql->get_row($query,$exps);
 $posture_analysis_method = '';
 for ($i = 0; $i < sizeof($exps); $i++){
   $strain = $exps[$i][1];
   //echo $detail_level;
   if ($detail_level != 's'){
     if ($exps[$i][2] != '')
       $strain .= "::" . $exps[$i][2];
		if ($detail_level != 's1'){
			if ($exps[$i][3] != '')
				$strain .= "::" . $exps[$i][3];
		}
	}
   //echo $exps[$i][0] . "<BR>";

   $region_strains[$exps[$i][0]] = array($exps[$i][1],$exps[$i][2],$exps[$i][3],$exps[$i][4]);
   if ($strain_models[$strain] == '')
     $strain_models[$strain] = $exps[$i][5];
   $experiment_strains[$strain] = $exps[$i][0];
   if ($posture_analysis_method == ''){
     $posture_analysis_method = $exps[$i][6];
   }
   if (isset($control_region_hash[$exps[$i][0]]))
     $control_strain_hash[$strain]= '1';
 }

 if (ns_param_spec($_POST,'is_single_posture_model') && $_POST['is_single_posture_model']=="0"){
   $is_single_posture_model = fals;
 }
 else{
   $is_single_posture_model = true;
   $single_posture_model_name = 0;
   foreach ($strain_models as $s => $m){
     if ($single_posture_model_name == 0)
       $single_posture_model_name = $m;
     else if ($single_posture_model_name != $m){
       $is_single_posture_model = FALSE;
       break;
     }
   }
 }

 if (ns_param_spec($_POST,'set_posture_models')){
   if ($is_single_posture_model){
     $model_name = $_POST['single_posture_model_name'];
     $posture_analysis_method = $_POST['posture_analysis_method'];
     $query = "UPDATE sample_region_image_info as i, capture_samples as s SET posture_analysis_model ='$model_name',posture_analysis_method='$posture_analysis_method' WHERE i.sample_id = s.id AND s.experiment_id = $experiment_id";

   $sql->send_query($query);
   }
   else{
     foreach($_POST as $k => $v){
       if (substr($k,0,16) == "posture_modelZ"){
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
	$query =  " UPDATE sample_region_image_info as i, capture_samples as s SET posture_analysis_model ='$model',posture_analysis_method='$posture_analysis_method' WHERE $strain_condition AND i.sample_id = s.id AND s.experiment_id = $experiment_id";
	$sql->send_query($query);
       }

     }
   }
     header("Location: manage_experiment_controls.php?$query_parameters\n\n");

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
<form action="manage_experiment_controls.php?<?php echo $query_parameters . "&regression_setup=1"?>" method="post">
<?php //var_dump($jobs[0]);
?>
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
    </select></td></tr>
    <tr><TD bgcolor="<?php echo $table_colors[1][0] ?>">Device Calibration Strain <br>Lifespan Mean</td><td bgcolor="<?php echo $table_colors[1][1] ?>">
<input type="text" size=3 name="control_lifespan" value="<?php echo $control_lifespan?>"> days<font size="-2"><br>*leave blank to use grand mean</font></td></tr>
<tr><TD colspan=2 bgcolor="<?php echo $table_colors[0][0] ?>">
	<div align="right"><input name="set_control_strain" type="submit" value="Set Control Information">
	</div>
</td></tr>
	</table>
	</td></tr>
	</table>
</form>
	<br>

</td><td valign="top">
<form action="manage_experiment_controls.php?<?php echo $query_parameters . "&machine_parameters=1"?>" method="post">

<table align="center" border="0" cellpadding="0" cellspacing="1" bgcolor="#000000"><tr><td><table border="0" cellpadding="4" cellspacing="0" width="100%"><tr <?php echo $table_header_color?> ><td colspan=2><b>Posture Analysis Parameter Sets</b></td></tr>

<tr><td bgcolor="<?php echo $table_colors[1][0] ?>">Strain Grouping</td><td bgcolor="<?php echo $table_colors[1][1] ?>">

<select name="detail_level" onchange='this.form.submit()'>
<option value="s" <?php if ($detail_level == 's') echo "selected"?> >Just Strain</option>
<option value="s1"<?php if ($detail_level == 's1') echo "selected"?> >Strain And Condition 1</option>
<option value="s12"<?php if ($detail_level == 's12') echo "selected"?> >Strain and Conditions 1 and 2</option>
<option value = "s123"<?php if ($detail_level == 's123' || $detail_level == '') echo "selected"?> >Strain And Conditions 1,2,and 3</option></select></td></tr>
<tr><td bgcolor="<?php echo $table_colors[1][0] ?>">Posture Analysis Method</td><td bgcolor="<?php echo $table_colors[1][1] ?>">

<select name="posture_analysis_method">
<option value=""<?php if ($posture_analysis_method == '') echo "selected"?> >None Specified</option>
<option value="hm" <?php if ($posture_analysis_method == 'hm') echo "selected"?> >Hidden Markov Model</option>
<option value="thresh"<?php if ($posture_analysis_method == 'thresh') echo "selected"?> >Thresholding</option>
</select></td></tr>

<tr><td bgcolor="<?php echo $table_colors[0][0] ?>">Single Model for All Plates</td><TD bgcolor="<?php echo $table_colors[0][1] ?>">
						      <select name="is_single_posture_model" onchange='this.form.submit()'><option value="1" <?php if ($is_single_posture_model)echo "selected";?>>All plates use the same model</option><option value="0" <?php if (!$is_single_posture_model)echo "selected";?>>Each strain has its own model</option></select>
							     </td></tr>
							     <?php if ($is_single_posture_model){?>
      <tr><td bgcolor="<?php echo $table_colors[0][0] ?>">All Plate Model:</td><TD bgcolor="<?php echo $table_colors[0][1] ?>">
	<?php output_editable_field("single_posture_model_name",$single_posture_model_name,true,30);?>
							     </td></tr>
																							   <?} else{?>

<?php
	$c = 0;
    foreach($experiment_strains as $strain => $region_id){
	echo
	"<tr><td bgcolor=\"".$table_colors[$c][0] . "\">$strain</td>" .
	"<td bgcolor=\"" . $table_colors[$c][1] . "\">";
	output_editable_field("posture_modelZ" . $region_id ,$strain_models[$strain],true,30);
	echo "</td></tr>\n";
	$c =!$c;
    }

  ?>
	<?php }?>

<tr><td bgcolor="<?php echo $table_colors[0][0] ?>" colspan=2>
	<div align="right"><input name="set_posture_models" type="submit" value="Set Posture Analysis Models">
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