<?php 
class ns_exception extends Exception{
	function __construct($t = ""){
		$this->text = $t;
		$this->trace = var_export(debug_backtrace(), true);
	}
	public $text, $trace;
};
?>