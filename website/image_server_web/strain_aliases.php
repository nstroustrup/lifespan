<?php
require_once ('worm_environment.php');
require_once('ns_experiment.php');
$experiment_id = @$query_string['experiment_id'];
$strain_name = @$query_string['strain'];
$update_experiment = $_POST['update_experiment']=='1';


if ($experiment_id != ''){
	$query = "SELECT name FROM experiments WHERE id = $experiment_id";
	$sql->get_value($query,$experiment_name);
}
//load all strain aliases
$query = "SELECT DISTINCT s.id, s.strain,s.genotype,s.conditions FROM strain_aliases as s";
if ($strain_name != '')
	$query .=" WHERE s.strain = '$strain_name'";
else if ($experiment_id != ''){
	    $query .=", sample_region_image_info as r, capture_samples as m WHERE s.strain=r.strain AND r.sample_id=m.id AND m.experiment_id = $experiment_id ";
}
$query .= " ORDER BY s.strain ASC";
$sql->get_row($query, $current_strain_aliases );

//build a hash table for strain aliases
$j = 0;
foreach($current_strain_aliases as $i => $v){
  $known_strains[$j]=array($v[0],$v[1],$v[2]);
  $known_strains_by_name[$v[1]] = 1;
  $known_strains_by_id[$v[0]] = $v[1];
}

//identify any strains that are specified for the current experiment but do not have aliases in the database
//This makes it easier to enter in the metadata after the experiment plates have been labeled
$new_strains_from_experiment = array();
if ($experiment_id != ''){
	$query = "SELECT DISTINCT r.strain FROM sample_region_image_info as r, capture_samples as m WHERE  r.sample_id=m.id AND m.experiment_id = $experiment_id ";
	$sql->get_row($query,$strains_in_current_experiment);
	$j = 0;
	$ss = sizeof($known_strains_by_id)+1;
	foreach($strains_in_current_experiment as $i => $v){
		if (!isset($known_strains_by_name[$v[0]])){
			$known_strains_by_id[$j+$ss] = $v[0];
			array_push($new_strains_from_experiment,$v[0]);
			$j++;
		}
	
	}
}

$reload = FALSE;
if ($_POST['edit'] =='1'){
  foreach($_POST as $key => $value){
    //     echo $key . "=" . $value . "<BR>";
    $is_strain = $key[0] == 's';
    $is_genotype = $key[0] == 'g';
    $is_condition = $key[0] == 'c';
    $is_delete_request = $key[0] == 'd';
    if (!$is_condition && !$is_strain && !$is_genotype && !$is_delete_request)
      continue;
    if ($key[1] != '_')
      continue;
    $id = substr($key,2);
    if ($is_delete_request){
      $query = "DELETE FROM strain_aliases WHERE id = " . $id;
      //	echo $query;
      $sql->send_query($query);
    }
    else if ($is_strain)
      $strain_info[$id][0] = $value;
    else if ($is_genotype)
      $strain_info[$id][1] = $value;
    else if ($is_condition)
      $strain_info[$id][2] = $value;
    else throw new ns_exception("Unknown option");
  }
  
  foreach ($strain_info as $id => $d){
    if ($d[0] == '')
      continue;
       $insert = FALSE;
    if ($id === 'a' || (int)$id < 0){
      $query = "INSERT INTO strain_aliases ";
      $insert = TRUE;
    }
    else 
      $query = "UPDATE strain_aliases ";
    
    $query .= "SET strain='" . mysql_real_escape_string($d[0]) . "', genotype='".mysql_real_escape_string($d[1])."', conditions='".mysql_real_escape_string($d[2])."'";
    
    if (!$insert) $query .= " WHERE id = $id ";
    //       echo $query . "<BR>";
    //      continue;
    $sql->send_query($query);
    $reload = TRUE;
  }
  
   if (FALSE && $update_experiment){
     foreach($strain_info as $id => $d){
       if ($d[0] != $known_strains_by_id[$id]){
	 //		var_dump($known_strains_by_id);
	 //		echo "<BR>$id<br>";
	 $query = "UPDATE sample_region_image_info as r, capture_samples as s "
	   . "SET r.strain = '" . mysql_real_escape_string($d[0]) . 
	   "' WHERE r.strain='" . mysql_real_escape_string($known_strains_by_id[$id]) 
	   . "' AND r.sample_id = s.id AND s.experiment_id = $experiment_id";
	 $sql->send_query($query);
       }
     }
   }
 }
$next_qs = "experiment_id=$experiment_id&strain=$strain";

if ($reload){
  header("Location: strain_aliases.php?$next_qs\n\n");
  die('');
 }

display_worm_page_header("Strain Information");
echo "<span class=\"style1\">";
if ($experiment_id != '')
	echo "Viewing strains used in experiment  $experiment_name";

else if ($strain_name != '')
	echo "Viewing information for strain $strain_name";

else
	echo "Viewing all strain information";

echo "</span> <br><br>";
?>
<?php
 echo '<form action="strain_aliases.php?&'.$next_qs.'" method="post">';
echo "<input name=\"edit\" type=\"hidden\" value=\"1\">";
echo "<table align=\"center\" border=0 cellspacing=1 cellpadding=1>\n";
echo "<tr><td bgcolor=\"#000000\">\n";
echo "<table border=0 bgcolor=\"#FFFFFF\" cellspacing=0 cellpadding=3>\n";
echo "<table cellspacing=0 cellpadding=0 width=\"100%\">\n";
echo "<tr $table_header_color><td>Strain</td><td>Genotype</td><td>&nbsp</td></tr>";
if (sizeof($current_strain_aliases) +sizeof($new_strains_from_experiment)== 0)
	  echo "<tr><td colspan=3 bgcolor=\"{$table_colors[$i%2][0]}\">No Strains Found</td></tr>";


for ($i = 0; $i < sizeof($current_strain_aliases); $i++){
  echo "<tr><td>";
  
  echo "<tr><td bgcolor=\"{$table_colors[$i%2][0]}\">\n";
  output_editable_field('s_'.$current_strain_aliases[$i][0],$current_strain_aliases[$i][1],TRUE, 10);
  echo "\n</td><td bgcolor=\"{$table_colors[$i%2][1]}\">\n";
  output_editable_field('g_'.$current_strain_aliases[$i][0],$current_strain_aliases[$i][2],TRUE, 25);
  // echo "\n</td><td bgcolor=\"{$table_colors[$i%2][0]}\">\n";
  //output_editable_field('c_'.$current_strain_aliases[$i][0],$current_strain_aliases[$i][3],TRUE, 25);
  echo "\n</td><td bgcolor=\"{$table_colors[$i%2][1]}\">\n";
  echo "<input name=\"d_{$current_strain_aliases[$i][0]}\" type=\"submit\" value=\"delete\">\n";
  echo "\n</td></tr>";
  
 }
for ($i = 0; $i < sizeof($new_strains_from_experiment); $i++){
  echo "<tr><td>";
  echo "<tr><td bgcolor=\"{$table_colors[$i%2][0]}\">\n";
  output_editable_field('s_'.(-($i+1)),$new_strains_from_experiment[$i],TRUE, 10);
  echo "\n</td><td bgcolor=\"{$table_colors[$i%2][1]}\">\n";
  output_editable_field('g_'.(-($i+1)),'',TRUE, 25);
  //  echo "\n</td><td bgcolor=\"{$table_colors[$i%2][0]}\">\n";
  //  output_editable_field('c_'.(-($i+1)),'',TRUE, 25);
  echo "\n</td><td bgcolor=\"{$table_colors[$i%2][1]}\">\n";
  echo "&nbsp;";
  echo "\n</td></tr>";
  
 }
?>
<tr><td colspan=3  bgcolor="<?php echo $table_colors[0][0] ?>"><font size="-1">Add new strain description:</font></td></tr>
<tr><td>
<tr bgcolor="<?php echo $table_colors[0][0] ?>"><td>
<?php output_editable_field('s_a','',TRUE,10) ?>
</td><td>
<?php output_editable_field('g_a','',TRUE,25) ?>
</td>
<!--<td>
													    //<?php output_editable_field('c_0','',TRUE,25) ?>
</td> -->
<td>&nbsp;</td>
</tr><tr><td colspan=3  bgcolor="<?php echo $table_colors[0][0] ?>">&nbsp;</td></tr>
<tr <?php echo $table_header_color?>><td></td><td></td><td align="right"> <input name="save" type="submit" value="Save"></td></tr>

</td>
</tr>
</table>
</table>
</form>
<br>
<center><a href="strain_aliases.php">[View All Strains in DB]</a></center>
<?php
display_worm_page_footer();
?>
