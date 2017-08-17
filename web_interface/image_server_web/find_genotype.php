<?php
require_once ('worm_environment.php');

$refresh=FALSE;

if ($refresh){
  header("Location: manage_file_deletion_jobs.php\n\n");
  die("");
 }
$query = "SELECT distinct r.strain,s.experiment_id,e.name " .
  "FROM sample_region_image_info as r, capture_samples as s,".
  "experiments as e WHERE r.sample_id = s.id AND r.strain != '' ".
  " AND s.experiment_id = e.id ORDER BY strain";

$sql->get_row($query,$loc);
$strain_experiment = array();
$strain_experiment_id = array();
$genotypes = array();
$show_experiment = @$query_string['show_experiment']!="0";
$show_strain = @$query_string['show_strain'] != "0";
for ($i = 0; $i < sizeof($loc); $i++){
  $n = $loc[$i][0];
  if (!isset($strain_experiment[$n])){
    $strain_experiment[$n] = array();
    $strain_experiment_id[$n] = array();
  }
  array_push($strain_experiment[$n],$loc[$i][2]);
  array_push($strain_experiment_id[$n], $loc[$i][1]);
  $query = "SELECT genotype FROM strain_aliases WHERE strain = \"" . $n . "\"";
  $sql->get_row($query,$l);
  if (sizeof($l) == 0){
    $genotypes[$n] = "";
  }
  else $genotypes[$n] = $l[0][0];
 }


display_worm_page_header("Locate Strains and Genotypes in Database");
?>
<table border=0 bgcolor="#000000" cellspacing=1 cellpadding=0 align="center"><tr><td>
<table border=0 bgcolor="#FFFFFF" cellspacing=0 cellpadding=3>
<tr><td colspan = 2 bgcolor="#FFFFFF">
<?php
echo "<table border=0>";
foreach ($strain_experiment as $strain=>$experiment){
  echo "<tr><td colspan = 2, bgcolor = \"#CCCCCC\" >";
  if ($show_strain)
    echo $strain . " ";
  echo "<i>" . $genotypes[$strain] . "</i></td></tr>";
  if($show_experiment){
  for ($i = 0; $i < sizeof($experiment); $i++){
    echo "<tr><td width = 50>&nbsp;</td><td><a href=\"manage_samples.php?experiment_id=" . $strain_experiment_id[$strain][$i] . "\">" . $experiment[$i] . "</a></td></tr>";
  }
  }
}
  echo "</table>";



?>
</td></tr>
</table></td></tr></table><br>
<?php

display_worm_page_footer();
?>
