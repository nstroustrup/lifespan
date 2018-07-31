<?php
	require_once("ns_exception.php");
	class ns_sql{
		var $link,$server,$name,$pwd,$db;
		function connect($sql_server, $sql_name, $sql_pwd, $sql_db){
			$this->server = $sql_server;
			$this->name = $sql_name;
			$this->pwd = $sql_pwd;
			$this->db = $sql_db;
			//echo $sql_db;
			$this->link = @mysqli_connect($this->server, $this->name, $this->pwd, $this->db);
			if ($this->link === FALSE)
			  throw new ns_exception("Could not connect to " . $this->name . "@" . $this->server . " .  Error: " .  mysqli_connect_error());
		}

		function disconnect(){
    			mysqli_close($this->link);
		}
		function restart(){
			$this->disconnect();
			$this->connect($this->server,$this->name,$this->pwd,$this->db);
		}
		function escape_string($text){
			return mysqli_real_escape_string($this->link,$text);
		}

		//place sql results in a 2x2 matrix $result.
		function get_row(&$query,&$result, $debug = FALSE){
			$result = array();
			if ($debug)
				echo "Sending query ". $query . "<BR>\n";
	    	$res1 = mysqli_query($this->link, $query) or die("Query failed : $query<br>\n" . mysqli_error($this->link));
			$i = (int)0;
			while($row = mysqli_fetch_row($res1)){
				if ($debug)
					echo ("Getting row.<BR>");
				$result[$i] = $row;
				$i = $i+1;
			}
			if ($debug)
				echo "Returning " . sizeof($result) . " results.<BR>";
			mysqli_free_result($res1);
		}

		//place a single line of returned input in array $result.
		function get_single_row(&$query,&$result, $debug=FALSE){
			if ($debug) echo $query . "<BR>";
			$this->get_row($query,$res);
			$size = sizeof($res);
			if ($size == 1)
				$result = $res[0];
			 else throw new ns_exception("Query returned $size results: $query.  Only one expected.");
		}

		function get_value(&$query,&$result,$debug=FALSE){
			$this->get_single_row($query,$res,$debug);
			$result = $res[0];
		}

		function send_query(&$query, $debug = FALSE){
			if ($debug)
				echo "Sending query = $query <BR>";
			if ($query == '')
			  throw new ns_exception('Blank query recieved');
			$res1 = mysqli_query($this->link, $query) or die("Query failed : $query<br>\n" . mysqli_error($this->link));
			if ($res1 != TRUE && $res1 != FALSE)
				throw new ns_exception("Query returned results: $query");
		}

		function send_query_get_id(&$query, $debug = FALSE){
			$this->send_query($query,$debug);
			$query2 = "SELECT @@identity";
			$this->get_value($query2,$result);
			return $result;
		}
	}
?>
