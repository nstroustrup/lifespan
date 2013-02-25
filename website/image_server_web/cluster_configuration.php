<?php
require_once ('worm_environment.php');
require_once('ns_experiment.php');
$query = "SELECT id, k,v FROM constants ORDER BY k ASC";
$sql->get_row($query,$constants);
$reload = FALSE;
if ($_POST['save'] !=''){
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
if ($_POST['delete'] !=''){
  $id = $_POST['id'];
  $query = "DELETE FROM constants WHERE id = $id";
  $sql->send_query($query);
  $reload = TRUE;
}
if ($reload){
  header("Location: cluster_configuration.php\n\n");
  die("");
 }

$query = "SELECT label_short,label,exclude,next_flag_name_in_order, hidden, color FROM annotation_flags";
$sql->get_row($query,$flags);
$root_flag_index = 0;
$tail = "";
$number_of_tails = 0;
$first_not_root_id = -1;

for ($i = 0; $i < sizeof($flags); $i++){
  $references_to_flags[$flags[$i][3]] = array();
  $sorted_flags[$flags[$i][0]] = $flags[$i];
  if ($flags[$i][0] == "MULTI_ERR")
    $root_flag_index = $i;
  else if ($first_not_root_id == -1)
    $first_not_root_id = $i;
  if (strlen($flags[$i][3]) == 0){
    $tail = $flags[$i][0];
    $number_of_tails++;
  }
}
if($number_of_tails != 1){//|| TRUE){
  
  //  die("YIKES: Number of Tails" . $number_of_tails);
  //$query = "UPDATE annotation_flags
  for ($i = 0; $i < sizeof($flags); $i++){
    echo $flags[$i][0] . "<BR>";
    if ($flags[$i][0] == "MULTI_ERR"){
      $next = $flags[$first_not_root_id][0];
      //   die($next);
      }
    else if ($i+1 ==sizeof($flags) || $i+2 == sizeof($flags) && $flags[sizeof($flags)-1][0]="MULTI_ERR"){
      $next='';
    }
    else $next = $flags[$i+1][0];
    $query = "UPDATE annotation_flags SET next_flag_name_in_order='" . $next ."' WHERE label_short='".
      $flags[$i][0] . "'";
    //  echo $query . "<BR>";
    // die($query);
    $sql->send_query($query);
  }
  //die("SDF");
  header("Location: cluster_configuration.php\n\n");
 }

for ($i = 0; $i < sizeof($flags); $i++){
  array_push($references_to_flags[$flags[$i][3]],$flags[$i][0]);
 }
//var_dump($sorted_flags);
//echo "<BR><BR>";

$last_flag_id = $flags[0][0];
for (;;){
  // echo "CUR:";
  // var_dump($sorted_flags[$last_flag_id]);
  //   echo "<BR>";
  if (!isset($sorted_flags[$sorted_flags[$last_flag_id][4]]) || $sorted_flags[$last_flag_id][4] == 0  ){
    //  echo "STOPPING";
	break;
      }
      $last_flag_id = $sorted_flags[$last_flag_id][4];
 }

if ($_POST['save_label']){
  $label_short = $_POST['label_short'];
  $label = $_POST['label'];
  $exclude = $_POST['exclude'];
  $hide = $_POST['hide'];
  $color = $_POST['color'];
 $query = "UPDATE annotation_flags SET label ='$label',exclude=";
 if ($exclude=="1")
    $query .= "1";
  else $query .= "0";
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

 if ($_POST['new_label']){
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
      "next_flag_name_in_order=''";
    $query2 = "UPDATE annotation_flags SET next_flag_name_in_order = '$label_short' WHERE label_short='$tail'";
    //die($query . "<BR>" . $query2);
    $sql->send_query($query);
    $sql->send_query($query2);

  header("Location: cluster_configuration.php\n\n");
 }
if ($_POST['move_up']!='' || $_POST['move_down']!=''){
  $name = $_POST['label_short'];
  
  //handle error  state where this flag is floating.
 
   if ($name != $flags[$root_flag_index]){/*
    if (!isset($references_to_flags[$name]) || sizeof($references_to_flags[$name]) == 0){
      $query = "UPDATE annotation_flags SET next_flag_name_in_order = '$name' WHERE label_short = '" . $flags[$root_flag_index][0] . "'";
      echo $query . "<BR>";
      $query = "UPDATE annotation_flags SET next_flag_name_in_order = '" . 
$flags[$root_flag_index][3] . "' WHERE label_short = '$name'";
      echo $query . "<BR>";
      die("");
      }*/
      
    

    if ($_POST['move_up'] != ''){
      // die( "MOVING UP<BR>");
      //make grandparents point to current node
      $earliest_parent = $references_to_flags[$name][0];
      for ($i = 0; $i < sizeof($references_to_flags[$name]); $i++){
	if ($references_to_flags[$name][$i] < $earliest_parent)
	  $earliest_parent = $references_to_flags[$name][$i];
      }
      if ($earliest_parent != $flags[$root_flag_index][0]){
	
      //make parents point to moving node's child
      $query = "UPDATE annotation_flags SET next_flag_name_in_order = '" . $sorted_flags[$name][4] . "' WHERE next_flag_name_in_order = '$name'";
      //echo $query . "<BR>";
      $sql->send_query($query);
	for ($i = 0; $i < sizeof($references_to_flags[$name]); $i++){
	  $query = "UPDATE annotation_flags SET next_flag_name_in_order='$name' WHERE next_flag_name_in_order='" . $references_to_flags[$name][$i]."'";
	  //  echo $query . "<BR>";
	  $sql->send_query($query);
	  //make current node point to new child
	  $query = "UPDATE annotation_flags SET next_flag_name_in_order = '" . $earliest_parent . "' WHERE label_short = '$name'";
	  //echo $query . "<BR>";
	  $sql->send_query($query);
	}
      }
    }
    else{
      // die("MOVING DOWN<br>");
      //move down
      //echo $id . "<BR>";
      //make sure it's not already at the bottom
      if (isset($sorted_flags[$sorted_flags[$name][4]])){
	
	//make parents point to moving node's child
	$query = "UPDATE annotation_flags SET next_flag_name_in_order = '" . $sorted_flags[$name][4] . "' WHERE next_flag_name_in_order = '$name'";
	//		echo $query . "<BR>";	
	$sql->send_query($query);
	//mark children's next as current
	$query = "UPDATE annotation_flags SET next_flag_name_in_order = '$name WHERE label_short = '" . $sorted_flags[$name][4] ."'";
	//		echo $query . "<BR>";
	$sql->send_query($query);
	//mark current's net as children's next
	$query = "UPDATE annotation_flags SET next_flag_name_in_order = '" . $sorted_flags[$sorted_flags[$name][4]][4] . "' WHERE label_short = '$name'";
	//	echo $query . "<BR>";
	$sql->send_query($query);
      }
    }
  }
  //die("");
    header("Location:cluster_configuration.php\n\n");
 }

$ordered_flags = array();
for ($i = $flags[$root_flag_index][0];  isset($sorted_flags[$i]); $i = $sorted_flags[$i][3]){
  array_push($ordered_flags,$sorted_flags[$i]);
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
<?php
echo "<table align=\"center\" border=0 cellspacing=1 cellpadding=1>\n";
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
</table>
<BR><BR>
<a name="flags">
<H2>Animal Annotation Flags</H2>

<table align="center" border=0 cellspacing=0 cellpadding=1><tr><td bgcolor="#000000">
<table border=0 bgcolor="#FFFFFF" cellspacing=0 cellpadding=0><tr><td>
<table cellspacing=0 cellpadding=0 width="100%"><tr <?php echo $table_header_color ?>><td width=200>Short name</td><td width=400>Flag Label</td><td width=8>Color</td><td width=100><center>Exclude Animals<br>With Flag</center></td><td width=100><center>Hide Flag</center></td><td>&nbsp;</td></tr>
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
  echo "<center><input type=\"checkbox\" name=\"exclude\" value=\"1\"";
  if ($ordered_flags[$i][2]=="1")
    echo " checked";
  echo "></center></td><td width=100 bgcolor=\"{$table_colors[$i%2][0]}\"><center>";  
echo "<input type=\"checkbox\" name=\"hide\" value=\"1\"";
  if ($ordered_flags[$i][4]=="1")
    echo " checked";
  echo "></center>";
  echo "\n</td><td bgcolor=\"{$table_colors[$i%2][0]}\">\n";
  echo "<input type=\"hidden\" name=\"label_short\" value=\"{$ordered_flags[$i][0]}\">";
  echo "<input name=\"save_label\" type=\"submit\" value=\"save\">\n";
  echo "<input name=\"move_up\" type=\"submit\" value=\"move up\">\n";
  echo "<input name=\"move_down\" type=\"submit\" value=\"move down\">\n";
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
</table>
<?php
display_worm_page_footer();
?>
