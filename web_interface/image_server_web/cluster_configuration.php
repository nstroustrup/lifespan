<?php
require_once ('worm_environment.php');
require_once('ns_experiment.php');
require_once('ns_processing_job.php');
$query = "SELECT id, k,v FROM constants ORDER BY k ASC";
$sql->get_row($query,$all_constants);
$pos = 0;
$constants = array();
$show_verbose_constants = false;

if (array_key_exists("show_verbose_constants",$query_string))
   $show_verbose_constants = true;
for ($i = 0; $i < sizeof($all_constants); $i++){

    if (!$show_verbose_constants){   
    if(substr($all_constants[$i][1],0,20)=="duration_until_next_")
       continue;
    if (substr($all_constants[$i][1],0,5)=="last_" &&
       substr($all_constants[$i][1],strlen($all_constants[$i][1])-17) =="_alert_submission")
       continue;
    if ($all_constants[$i][1]=="last_missed_scan_check_time")
       continue;
       }
    $constants[$pos] = $all_constants[$i];
    $pos++;
}

$reload = FALSE;
if (ns_param_spec($_POST,'save')){
  $id = $_POST['id'];
  $k = $_POST['k'];
  $v = $_POST['v'];
  if ($id == "new")
    $query = "INSERT INTO constants SET v='$v',k='$k'";
  else
    $query = "UPDATE constants SET k='$k', v='$v' WHERE id = $id";
  $sql->send_query($query);
  $reload = TRUE;
}
if (ns_param_spec($_POST,'delete')){
  $id = $_POST['id'];
  $query = "DELETE FROM constants WHERE id = $id";
  $sql->send_query($query);
  $reload = TRUE;
}
if ($reload){
  header("Location: cluster_configuration.php\n\n");
  die("");
 }

$query = "SELECT label_short,label,exclude,next_flag_name_in_order, hidden, color, id, next_flag_id_in_order FROM annotation_flags ORDER BY id ASC";
$sql->get_row($query,$flags);

#set up column labels for clearer code
$NEXT_NAME = 3;
$NAME = 0;
$NEXT_ID = 7;
$ID = 6;

$root_flag_index = 0;
$tail = "";
$number_of_tails = 0;
$first_not_root_id = -1;
$sorted_flags = array();
for ($i = 0; $i < sizeof($flags); $i++){
  $references_to_flags[$flags[$i][$NEXT_NAME]] = array();
  $sorted_flags[$flags[$i][$NAME]] = $flags[$i];
  if ($flags[$i][$NAME] == "MULTI_ERR")
    $root_flag_index = $i;
  else if ($first_not_root_id == -1)
    $first_not_root_id = $i;
  if (strlen($flags[$i][3]) == 0){
    $tail = $flags[$i][0];
    //echo "$i tail: " . $tail . "<BR>";
    $number_of_tails++;
  }
}
//var_dump($flags);
#die($number_of_tails);
if($number_of_tails != 1 || ns_param_spec($query_string,'fix_flags') && $query_string["fix_flags"] == "1"){
  for ($i = 0; $i < sizeof($flags); $i++){ 
   if ($flags[$i][0] == "MULTI_ERR"){
      if ($i == 0)
	$next_index = 1;
	else 
	$next_index = 0;
      $next = $flags[$next_index][$NAME];
      $next_id = $flags[$next_index][$ID];

    }
    else if ($i+1 ==sizeof($flags) || $i+2 == sizeof($flags) && $flags[sizeof($flags)-1][$NAME]=="MULTI_ERR"){
      $next='';
      $next_id = $flags[sizeof($flags)-1][$ID]+1;
    }
    else{
      $next_index = $i+1;
      if ($flags[$next_index][0] == "MULTI_ERR")
	$next_index++;
      $next = $flags[$next_index][$NAME];
      $next_id = $flags[$next_index][$ID];
    }
    $query = "UPDATE annotation_flags SET next_flag_name_in_order='" . $next ."', next_flag_id_in_order ='".$next_id ."' WHERE id=".
      $flags[$i][$ID];
    // echo $query . "<BR>";
     //die($query);
    $sql->send_query($query);
    //echo $query . "<BR>";
  }
  $q = "commit";
  $sql->send_query($q);
 //die("");
  header("Location: cluster_configuration.php?already_set_tails=1\n\n");
 }

for ($i = 0; $i < sizeof($flags); $i++){
  array_push($references_to_flags[$flags[$i][$NEXT_NAME]],$flags[$i][$NAME]);
 }
//var_dump($sorted_flags);
//echo "<BR><BR>";

//$last_flag_id = $flags[0][0];
//for (;;){
  // echo "CUR:";
  // var_dump($sorted_flags[$last_flag_id]);
  //   echo "<BR>";
 // if (!isset($sorted_flags[$sorted_flags[$last_flag_id][4]]) || $sorted_flags[$last_flag_id][4] == 0  ){
    //  echo "STOPPING";
//	break;
  //    }
    //  $last_flag_id = $sorted_flags[$last_flag_id][3];
 //}

if (ns_param_spec($_POST,'save_label')){
  $label_short = $_POST['label_short'];
  $label = $_POST['label'];
  $exclude = $_POST['exclude'];
  $hide = $_POST['hide'];
  $color = $_POST['color'];
 $query = "UPDATE annotation_flags SET label ='$label',exclude=";
 $query .= $exclude;
 $query .= ", hidden=";
 if ($hide=="1")
   $query .= "1";
 else $query .="0";
 $query .= ", color='$color'";

  $query .= " WHERE label_short= '" . $label_short . "'";
  //die($query);
  $sql->send_query($query);
  header("Location: cluster_configuration.php\n\n");
 }

 if (ns_param_spec($_POST,'new_label')){
    $label_short = $_POST['label_short'];

    $label = $_POST['label_long'];
    $query = "SELECT label_short FROM annotation_flags WHERE label_short='$label_short'";
    $sql->get_row($query,$res);
    if (sizeof($res) != 0)
      die("The specified flag (" . $label_short . ") already exists");
    if (strlen($label_short) < 5)
      die( "The specified flag (".$label_short.") is too short");
    if (strlen($label_short) > 15)
      die("The specified flag ($label_short) is too long.  Please limit flags to 15 characters or less");
    $query = "INSERT INTO annotation_flags SET label_short='$label_short',label='$label',".
      "next_flag_name_in_order='', exclude=0";
    $query2 = "UPDATE annotation_flags SET next_flag_name_in_order = '$label_short' WHERE label_short='$tail'";
    //die($query . "<BR>" . $query2);
    $sql->send_query($query);
    $sql->send_query($query2);

  header("Location: cluster_configuration.php\n\n");
 }
if (ns_param_spec($_POST,'move_up') || ns_param_spec($_POST,'move_down')){
  $name = $_POST['label_short'];
  $id = -1;
  for ($i =0; $i < sizeof($flags); $i++){
      if ($name == $flags[$i][$NAME]){
      	 $id = $flags[$i][$ID];
	 break;
	 }
  }
  if ($id == -1)
     die("Could not find flag $name in DB");
  //handle error  state where this flag is floating.

   if ($name != $flags[$root_flag_index]){

/*
    if (!isset($references_to_flags[$name]) || sizeof($references_to_flags[$name]) == 0){
      $query = "UPDATE annotation_flags SET next_flag_name_in_order = '$name' WHERE label_short = '" . $flags[$root_flag_index][$NAME] . "'";
      echo $query . "<BR>";
      $query = "UPDATE annotation_flags SET next_flag_name_in_order = '" .
$flags[$root_flag_index][$NEXT_NAME] . "' WHERE label_short = '$name'";
      echo $query . "<BR>";
      die("Problem!");
      }*/



    if (ns_param_spec($_POST,'move_up')){
       //die( "MOVING UP<BR>");
      //make grandparents point to current node
      //var_dump($references_to_flags);
      //die("");
      
      $earliest_parent = $references_to_flags[$name][$NAME];
      for ($i = 0; $i < sizeof($references_to_flags[$name]); $i++){
	if ($references_to_flags[$name][$i] < $earliest_parent)
	  $earliest_parent = $references_to_flags[$name][$i];
      }
      
      //die($earliest_parent);
      if ($earliest_parent != $flags[$root_flag_index][$NAME]){

      //make parents point to moving node's child
      $query = "UPDATE annotation_flags SET next_flag_name_in_order = '" . $name . "', next_flag_id_in_order = " . $id . " WHERE next_flag_name_in_order = '$earliest_parent'";
       
      // echo $query . "<BR>";
       //die($query);
      $sql->send_query($query);

      //update this flag's next labels
      $query = "UPDATE annotation_flags SET next_flag_name_in_order = '" . $earliest_parent . "', next_flag_id_in_order = " .$sorted_flags[$earliest_parent][$ID] . " WHERE label_short = '$name'";
      //die($query);      
     // echo $query . "<BR>";
      $sql->send_query($query);

      //update previous parent to be new child
   $query = "UPDATE annotation_flags SET next_flag_name_in_order='" . $sorted_flags[$name][$NEXT_NAME] ."', next_flag_id_in_order = " . $sorted_flags[$name][$NEXT_ID] ." WHERE label_short='" . $earliest_parent."'";
 //die($query);
   //   echo $query . "<BR>";
      $sql->send_query($query);

//	for ($i = 0; $i < sizeof($references_to_flags[$name]); $i++){
//	  $query = "UPDATE annotation_flags SET next_flag_name_in_order='$name', next_flag_id_in_order = $id WHERE next_flag_name_in_order='" . $references_to_flags[$name][$i]."'";
//	    echo $query . "<BR>";
	  //$sql->send_query($query);
	  //make current node point to new child
//	  $query = "UPDATE annotation_flags SET next_flag_name_in_order = '" . $earliest_parent . "' WHERE label_short = '$name'";
//	  echo $query . "<BR>";
	  //$sql->send_query($query);
//	}
      }
      //die("");
    }
    else{
      // die("MOVING DOWN<br>");
      //move down
      //echo $id . "<BR>";
      //make sure it's not already at the bottom
      if ($sorted_flags[$sorted_flags[$name][$NEXT_NAME]]!=""){

	//make parents point to moving node's child
	$query = "UPDATE annotation_flags SET next_flag_name_in_order = '" . $sorted_flags[$name][$NEXT_NAME] . "', next_flag_id_in_order= " . $sorted_flags[$name][$NEXT_ID] . " WHERE next_flag_name_in_order = '$name'";
	//		echo $query . "<BR>";
	$sql->send_query($query);
	//mark children's next as current
	$query = "UPDATE annotation_flags SET next_flag_name_in_order = '$name', next_flag_id_in_order=$id WHERE label_short = '" . $sorted_flags[$name][$NEXT_NAME] ."'";
	//		echo $query . "<BR>";
	$sql->send_query($query);
	//mark current's net as children's next
	$query = "UPDATE annotation_flags SET next_flag_name_in_order = '" . $sorted_flags[$sorted_flags[$name][$NEXT_NAME]][$NEXT_NAME] . "', next_flag_id_in_order=" . $sorted_flags[$sorted_flags[$name][$NEXT_NAME]][$NEXT_ID] ." WHERE label_short = '$name'";
	//	echo $query . "<BR>";
	$sql->send_query($query);
      }
    }

  }
  //die("");
    header("Location:cluster_configuration.php\n\n");
 }

$ordered_flags = array();
$numflags = sizeof($flags);
$j = 0;
$str = "";
for ($i = $flags[$root_flag_index][0];  isset($sorted_flags[$i]); $i = $sorted_flags[$i][3]){
  if ($j >= $numflags){
    $str .= "<BR>";
    echo $str;
    var_dump($flags);
    die("<BR>A loop was detected in the flag ordering: $j.   <a href=\"cluster_configuration.php?fix_flags=1\">[Click Here to Reset Flag order]</a>");
  }
  $str .= $sorted_flags[$i][0] . "<BR>";
  array_push($ordered_flags,$sorted_flags[$i]);
  $j++;
 }
/*echo "FIRST: ". $flags[$root_flag_index][0] . "<BR>";
var_dump($sorted_flags);
echo "<BR><BR>";
var_dump($ordered_flags);
die("");*/

display_worm_page_header("Imaging Cluster Configuration");
?>
<a href="#constants">[Cluster Constants]</a> <a href="#flags">[Animal Annotation Flags]</a><br>
<a name="constants">
<H2>Cluster Constants</H2>
<table width="0%" border=0 align="center">
<tr><td>
<?php
echo "<table align=\"center\" border=0 cellspacing=1 cellpadding=1 b>\n";
echo "<tr><td bgcolor=\"#000000\">\n";
echo "<table border=0 bgcolor=\"#FFFFFF\" cellspacing=0 cellpadding=3>\n";
for ($i = 0; $i < sizeof($constants); $i++){
  echo "<tr><td>";
  echo '<form action="cluster_configuration.php" method="post">';
  echo "\n<table cellspacing=0 cellpadding=0 width=\"100%\">\n";
  echo "<tr><td bgcolor=\"{$table_colors[$i%2][0]}\" width=400>\n";
  output_editable_field('k',$constants[$i][1],TRUE, 55);
  echo "\n</td><td bgcolor=\"{$table_colors[$i%2][1]}\" width = 400>\n";

  output_editable_field('v',$constants[$i][2],TRUE, 55);
  echo "\n</td><td bgcolor=\"{$table_colors[$i%2][0]}\">\n";
  echo "<input type=\"hidden\" name=\"id\" value=\"{$constants[$i][0]}\">";
  echo "<input name=\"save\" type=\"submit\" value=\"save\">\n";
  echo "<input name=\"delete\" type=\"submit\" value=\"delete\">\n";
  echo "\n</td></tr>";
  echo "\n</table>\n</form>\n</td></tr>";
 }
?>
<tr><td colspan=3 bgcolor="#CCCCCC">New Constant</td></tr>
<tr><td>
<form action="cluster_configuration.php" method="post">
<table cellspacing=0 cellpadding=0 width="100%">
<tr bgcolor="#CCCCCC"><td>
<?php output_editable_field('k','',TRUE,55) ?>
</td><td>
<?php output_editable_field('v','',TRUE,55) ?>
</td>
<td>
<input type="hidden" name="id" value="new">
  <input name="save" type="submit" value="save">
</td>
</tr>
</table>
</form>
</td></tr>
</table>
</td></tr>
</table>
<div align="right">
<?php if (!$show_verbose_constants){?>
<a href="cluster_configuration.php?show_verbose_constants=1">[Show verbose constants]</a>
<?php } else { ?>
<a href="cluster_configuration.php">[Hide verbose constants]</a>
<?php } ?>
</div>
</td></tr></table>
</form>
<BR><BR>
<a name="flags">
<H2>Animal Annotation Flags</H2>
<table align="center" border=0><tr><td>
<table align="center" border=0 cellspacing=0 cellpadding=1><tr><td bgcolor="#000000">
<table border=0 bgcolor="#FFFFFF" cellspacing=0 cellpadding=0><tr><td>
<table cellspacing=0 cellpadding=0 width="100%"><tr <?php echo $table_header_color ?>><td width=200>Short name</td><td width=400>Flag Label</td><td width=8>Color</td><td width=100><center>Annotation Handling</center></td><td width=100><center>Hide Flag</center></td><td>&nbsp;</td></tr>
</table>
<?php

for ($i = 0; $i < sizeof($ordered_flags); $i++){
  echo "<tr><td>";

  echo '<form action="cluster_configuration.php" method="post">';
  echo "\n<table cellspacing=0 cellpadding=0 width=\"100%\">\n";
  echo "<tr><td bgcolor=\"{$table_colors[$i%2][0]}\" width=200>\n";
  output_editable_field('label_short',$ordered_flags[$i][0],FALSE, 55);
  echo "\n</td><td bgcolor=\"{$table_colors[$i%2][1]}\" width = 400>\n";
  output_editable_field('label',$ordered_flags[$i][1],TRUE, 55);
  echo "\n</td><td width=8 bgcolor=\"{$table_colors[$i%2][1]}\">\n";
  output_editable_field('color',$ordered_flags[$i][5],TRUE, 6);
  echo "\n</td><td width=100 bgcolor=\"{$table_colors[$i%2][1]}\">\n";
  echo "<select name=\"exclude\" id=\"exclude\">\n";
  foreach($ns_flag_handling_labels as $key => $val){
  	echo "<option value=\"$val\"";
	if ($ordered_flags[$i][2] == $val)
	   echo " selected";
	echo ">$key</option>\n";
  }
  echo "</select>";
//echo "<center><input type=\"checkbox\" name=\"exclude\" value=\"1\"";
  //if ($ordered_flags[$i][2]=="1")
    //echo " checked";
//  echo ">";
echo "</center></td><td width=100 bgcolor=\"{$table_colors[$i%2][0]}\"><center>";
echo "<input type=\"checkbox\" name=\"hide\" value=\"1\"";
  if ($ordered_flags[$i][4]=="1")
    echo " checked";
  echo "></center>";
  echo "\n</td><td bgcolor=\"{$table_colors[$i%2][0]}\">\n";
  echo "<input type=\"hidden\" name=\"label_short\" value=\"{$ordered_flags[$i][0]}\">";
  echo "<input name=\"save_label\" type=\"submit\" value=\"save\">\n";
  echo "<input name=\"move_up\" type=\"submit\" value=\"move up\">\n";
  //echo "<input name=\"move_down\" type=\"submit\" value=\"move down\">\n";
  echo "\n</td></tr>";
  echo "\n</table>\n</form>\n</td></tr>";
 }
?>
<tr><td colspan=3 bgcolor="#CCCCCC">New Flag</td></tr>
<tr><td>
<form action="cluster_configuration.php" method="post">
  <table cellspacing=0 cellpadding=0 width="100%">
  <tr bgcolor="#CCCCCC"><td>
   <?php output_editable_field('label_short','',TRUE,55) ?>
   </td><td>
   <?php output_editable_field('label_long','',TRUE,55) ?>
   </td>
   <td>
   <input type="hidden" name="id" value="new">
   <input name="new_label" type="submit" value="save">
   </td>
   </tr>
  </table>
   </form>
</td></tr>
</table>
</tr></td>
</table>
<div align="right">
  <a href="cluster_configuration.php?fix_flags=1">[Reset Flag order]</a>
</div></td></tr></table>
<?php
display_worm_page_footer();
?>
