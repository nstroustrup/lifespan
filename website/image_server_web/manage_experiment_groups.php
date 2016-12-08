<?php
require_once ('worm_environment.php');

 ns_load_experiment_groups($experiment_groups,$group_order,$sql);

if ($_POST['create'] !=''){
  $new_experiment_group_name = $_POST['new_group_name'];
  $query = "UPDATE experiment_groups SET group_order=group_order+1";
  $sql->send_query($query);
  $query = "INSERT INTO experiment_groups SET group_name='$new_experiment_group_name', group_order=0";
	//die($query);
  $sql->send_query($query);
  $reload = TRUE;
}
if ($_POST['delete'] !=''){

  	$id = $_POST['selected_group_id'];
	
	$query = "DELETE FROM experiment_groups WHERE id = $id";
	die($query);
	$sql->send_query($query);
  $reload = TRUE;
}
if ($_POST['modify'] !=''){
	
  	$id = $_POST['selected_group_id'];
  	$new_name = $_POST['selected_group_name'];

	$query = "UPDATE experiment_groups SET group_name='" . $new_name . "' WHERE group_id = $id";
	//die($query);
	$sql->send_query($query);

  $reload = TRUE;
}
if ($_POST['move_up'] !='' || $_POST['move_down']){
	
  	$id = $_POST['selected_group_id'];
	$query = "SELECT group_order FROM experiment_groups WHERE group_id = $id";
	$sql->get_row($query,$res);
	if (sizeof($res)==0)
		throw new ns_exception("Could not find group id $id");
	$cur_order = $res[0][0];
	$query = "SELECT group_id, group_order FROM experiment_groups ";
	if ($_POST['move_up'])
		$query .="WHERE group_order > $cur_order ORDER BY group_order ASC LIMIT 1";
	else $query .="WHERE group_order < $cur_order ORDER BY group_order DESC LIMIT 1";
	$sql->get_row($query,$res);
	if (sizeof($res) > 0){
		$query = "UPDATE experiment_groups SET group_order=" . $res[0][1] . " WHERE group_id = $id";
		//echo $query . "<br>";
		$sql->send_query($query);
		$query = "UPDATE experiment_groups SET group_order=" . $cur_order . " WHERE group_id = " . $res[0][0];
		//die($query);
		$sql->send_query($query);
	}

  $reload = TRUE;
}

if ($reload){
  header("Location: manage_experiment_groups.php\n\n");
  die("");
 }

display_worm_page_header("Experiment Group Management");
?>
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
<form action="manage_experiment_groups.php" method="post">
<table cellspacing=0 cellpadding=3 width="100%">

<tr bgcolor="<?php echo $table_colors[0][0]?>"><td>
Create New Group
</td>
<td>
<?php output_editable_field('new_group_name','',TRUE,35) ?>
</td>
<td>
  <input name="create" type="submit" value="Create">
</td>
</tr>


<tr bgcolor="<?php echo $table_colors[1][0]?>"><td>
Modify Existing Group
</td>
<td>
<select name="selected_group_id">
<?php
	foreach ($group_order as $g){
		if ($g == 0) continue;
		echo "<option value=\"". $experiment_groups[$g][0] . "\">" . $experiment_groups[$g][1] . "</option>\n"; 
}

?><BR>
<?php output_editable_field('selected_group_name','',TRUE,35) ?>
</select>
</td>
<td>
  <input name="modify" type="submit" value="Modify">
  <br>
  <input name="move_up" type="submit" value="Move up">
  <br>
  <input name="move_down" type="submit" value="Move Down">
  <br>
  <input name="delete" type="submit" value="Delete" onClick="javascript:return confirm('Are you sure you want to delete the specified group?')">
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
