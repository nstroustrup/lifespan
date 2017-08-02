<?php
require_once('ns_exception.php');


function get_filenames_in_directory($path, &$filenames){
	$filenames = array();
	if (!($dh = opendir ($path)))
		throw new ns_exception("Attempting to get directory conents of an invalid directory: $path");
	
	global $encrypted_file_suffix,
		   $encrypted_file_suffix_length;
	
	while (($file = readdir($dh)) !== false) {
			if (is_dir ($path . '/' . $file))
				continue;
			array_push($filenames,$file);
	}
	closedir($dh);
}



function delete_file($path){
	@unlink($path);
}

function remove_directory($path){
	$dir = @opendir($path);
	if ($dir === FALSE)
		return false;
	//remove all files and subdirectories in directory
	while($file = readdir($dir)){    
		if ($file == '.' || $file == '..')
			continue;
		if (is_dir($path .'/'.$file))
			remove_directory($path .'/'.$file);
		else 
			@unlink($path .'/'.$file);
	}
	//remove directory
	closedir($dir);
	rmdir($path);
	return true;
}

function make_directory($path){
	$r = @mkdir($path,0755);
	
	if ($r === FALSE)
		throw new ns_exception("File Error: Could not create directory $path");
}

function rename_directory($old_path,$new_path){
	$res = @rename($old_path,$new_path);
	if ($r === FALSE)
		throw new ns_exception('File Error: Could not rename the directory $old_path to $new_path');
}

function make_directory_if_missing($path){
	if (is_dir($path))
		return;
	make_directory($path);

}

function correct_slashes($path){
  return str_replace("\\","/",$path);
}
?>