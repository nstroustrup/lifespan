<?php 
class ns_exception extends Exception{
	function ns_exception($t = ""){
		$this->text = $t;
		$this->trace = var_export(debug_backtrace(), true);
	}
	var $text, $trace;
};
?>