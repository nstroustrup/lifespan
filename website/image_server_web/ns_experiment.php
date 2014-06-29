<?php
  //require_once('request_scan.php');
require_once('ns_sql.php');
require_once('ns_dir.php');
//require_once('ns_external_interface.php');


class ns_device{
  function ns_device($name,&$sql,$allow_non_existant_devices=FALSE){
		$this->name_p = $name;
		if (!$allow_non_existant_devices){
		  $query = "SELECT name FROM devices WHERE name='" . $name . "'";
		  $sql->get_row($query,$dev);
		  if (sizeof($dev) == 0){
		    if (!$allow_non_existant_devices)
		      throw new ns_exception("There is no device named " . $name );
		  }
		}
	}
      
	function name(){
		return $this->name_p;
	}
	private $name_p;
    
}


class ns_capture_sample{
	public function id(){return $this->id_p;}
	
	function ns_capture_sample(ns_experiment &$experiment, ns_device &$device, $id = -1,&$sql = ""){
		$this->experiment_id_p = $experiment->id();
		$this->id_p = $id;
		$this->mask_id = 0;
		$this->device = $device;
		$this->turn_off_lamp_after_capture = FALSE;
		$this->censored = FALSE;
		if ($this->id_p != -1)
			$this->load($this->id_p,$sql);
	}
	
	function experiment_id(){
		return $this->experiment_id_p;
	}
	function name(){
		return $this->name_p;
	}
	function set_name($name,&$sql){
		$this->name_p = $name;
		$query = "SELECT id, parameters FROM capture_samples WHERE name='". addslashes($name) . "' AND experiment_id=" . $this->experiment_id_p;

		$sql->get_row($query,$res);
		if (sizeof($res) == 0){
			$query = "UPDATE capture_samples SET name='". addslashes($name) . "' WHERE experiment_id=" . $this->experiment_id_p;
			
			$this->id_p = $sql->send_query_get_id($query);
		}
		else {
			$this->id_p = $res[0][0];
			$this->capture_parameters = $res[0][1];
		}
	}
	function device_name(){
	  return $this->device->name();
	}
	
	function save(&$sql){
	  $this->model_filename = trim($this->model_filename);
		if ($this->id_p == -1)
			throw new ns_ex("Saving uninitialized ns_capture_sample object.");
		$turn_off_lamp = "0";
		if ($this->turn_off_lamp_after_capture===TRUE)
			$turn_off_lamp = "1";
		$censored = "0";
		if ($this->censored === TRUE)
		  $censored = "1";
		$query = "UPDATE capture_samples SET name='" .$this->name_p . "', parameters='" . $this->capture_parameters . "', description='" . $this->description . "', "
			   . " mask_id='" . $this->mask_id . "', device_name = '" . $this->device->name()."', turn_off_lamp_after_capture='$turn_off_lamp', censored='$censored', model_filename='" . $this->model_filename ."'  WHERE id='" . $this->id_p ."'";
		$sql->send_query($query);
	}
	function load($id,$sql){
		$this->id_p = $id;
		$query = "SELECT parameters, name, experiment_id, description, mask_id, device_name, turn_off_lamp_after_capture, censored,reason_censored, model_filename,
		size_unprocessed_captured_images,size_processed_captured_images,size_unprocessed_region_images,size_processed_region_images,size_metadata FROM capture_samples WHERE capture_samples.id=$id";
		$sql->get_row($query,$res);
		if (sizeof($res) == 0)
			throw new ns_exception("Could not find capture_sample id $id in db!");
		$this->capture_parameters = $res[0][0];
		$this->name_p = $res[0][1];
		$this->experiment_id = $res[0][2];
		$this->description = $res[0][3];
		$this->mask_id = $res[0][4];
		$this->device = new ns_device($res[0][5],$sql,TRUE);
		$this->turn_off_lamp_after_capture = $res[0][6] == '1';
		$this->censored = $res[0][7] == '1';
		$this->reason_censored = $res[0][8];
		$this->model_filename = $res[0][9];
		$this->size_unprocessed_captured_images = $res[0][10];
	 	$this->size_processed_captured_images= $res[0][11];
	 	$this->size_unprocessed_region_images= $res[0][12];
	 	$this->size_processed_region_images= $res[0][13];
		$this->size_metadata= $res[0][14];
		//echo "DEVICE " . $res[0][5] . "]]";

		if ($this->mask_id == '')
			$this->mask_id = 0;	
	}

	public	$capture_parameters,
	  $description,
	  $mask_id,
	  $turn_off_lamp_after_capture,
	  $censored,
	  $reason_censored,
	  $model_filename,
	  $size_unprocessed_captured_images,
	  $size_processed_captured_images,
	  $size_unprocessed_region_images,
	  $size_processed_region_images,
	  $size_metadata;
			
	private $id_p, 
			$name_p,
			$experiment_id_p,
			$device;
}


class ns_schedule_event{
	public $sample,
	    $scheduled_time,
		$device_id;
	function ns_schedule_event($sched_time,ns_capture_sample &$samp){
		$samp->id(); //confirm that a correct object has been passed.
		$this->sample = $samp;
		$this->device_name = $device_name;
		$this->scheduled_time = $sched_time;
	}
	
	
	function add_event_to_schedule(&$sql){
		$query = "INSERT INTO capture_schedule SET scheduled_time = '" . $this->scheduled_time . "', sample_id='" . $this->sample->id() .
			 "', experiment_id = '".$this->sample->experiment_id() ."'";
		echo "Adding Sample \"" . $this->sample->name() . "\" time point at " . date("m/d: H:i:s",$this->scheduled_time) .  "<br>";
		$sql->send_query($query);
	}
}

class ns_experiment{
	private $id_p;
	public $description, 
		$name, 
		$first_time_point,
		$last_time_point, 
		$num_time_points, 
		$hidden,
		$samples,
		$size_unprocessed_captured_images,
		$size_processed_captured_images,
		$size_unprocessed_region_images,
		$size_processed_region_images,
		$size_metadata,
		$size_video,
		$size_calculation_time;
	
	function clear_schedule(&$sql){
		$query = "DELETE from capture_schedule WHERE experiment_id='" . $this->id_p . "'";
		$sql->send_query($query);
		$query = "DELETE from capture_samples WHERE experiment_id='" . $this->id_p . "'";
		$sql->send_query($query);
	}
	
	function ns_experiment($id, $name,&$sql, $create_new = false){
		if (($id == '' || $id == 0) && $name=='')
			return;

		//try to load information from the db
		if (!$this->load($id,$name,$sql)){
			if (!$create_new)
				throw new ns_exception("ns_experiment::Experiment ($id,$name) does not exist.");
			$query = "INSERT into experiments SET name = '$name'";
			$this->id_p = $sql->send_query_get_id($query);
			$this->name = $name;
		}
	}

	//load information from the db.  If a name is specified, use it, otherwise
	//if an id is specified, use that instead.
	function load($id,$name,&$sql){
		$this->samples = array();

		$query = "SELECT id, name, description, first_time_point, last_time_point, num_time_points, hidden, 
		size_unprocessed_captured_images,size_processed_captured_images,size_unprocessed_region_images,size_processed_region_images,size_metadata, size_video, size_calculation_time FROM experiments ";
		if ($name != '')
			$query .="WHERE name = '$name'";
		else {
			if ($id != '' && $id >0)
				$query .= "WHERE id = '$id'";
			else throw new ns_ex("ns_experiment::Not enough information specified to load experiment info!");
		}
		$sql->get_row($query,$res);
		if (sizeof($res) == 0)
			return false;
		$this->id_p = $res[0][0];
		$this->name = $res[0][1];
		$this->description = $res[0][2];
		$this->first_time_point = $res[0][3];
		$this->last_time_point = $res[0][4];
		$this->num_time_points = $res[0][5];
		$this->hidden = $res[0][6];
		$this->size_unprocessed_captured_images = $res[0][7];
	 	$this->size_processed_captured_images= $res[0][8];
	 	$this->size_unprocessed_region_images= $res[0][9];
	 	$this->size_processed_region_images= $res[0][10];
		$this->size_metadata= $res[0][11];
		$this->size_video= $res[0][12];
		$this->size_calculation_time = $res[0][13];
		return true;
	}

	function & new_sample(ns_device &$device){
			return new ns_capture_sample($this,$device);			
	}

	function get_sample_information(&$sql,$load_censored=TRUE){
		$query = "SELECT id FROM capture_samples WHERE experiment_id='" . $this->id_p . "'";
		if (!$load_censored)
			$query .= " AND censored=0 ";
		$query .= " ORDER BY name ASC";
		//		die($query);
		$sql->get_row($query,$sample_ids);
		
		for ($i = 0; $i < sizeof($sample_ids); $i++){
		 // echo $sample_ids[$i][0] . ",";
		  $this->samples[$i] = new ns_capture_sample($this,new ns_device("",$sql,TRUE),$sample_ids[$i][0],$sql);
		}	
	}
	
	function id(){
		return $this->id_p;
	}
	
}


function ns_generate_sample_hash($experiment_id = 0, &$sample_hash, &$sql){
	//get sample hash
	$query = "SELECT experiment_id, id, name FROM capture_samples ";
	if ($experiment_id != 0 && $experiment_id != '')
		$query .= "WHERE experiment_id = '$experiment_id'";
	
	$sql->get_row($query,$samples);
	for ($i = 0; $i < sizeof($samples); $i++)
		$sample_hash[ $samples[$i][0] ][ $samples[$i][1] ] = $samples[$i][2] ;
}

function ns_generate_device_list(&$device_hash, &$sql){
	$query = "SELECT name FROM devices";
	$sql->get_row($query,$devices);
	for ($i = 0; $i < sizeof($devices); $i++){
		$device_hash[$i] = $devices[$i][0];
	}
}
?>