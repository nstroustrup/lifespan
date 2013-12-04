<?php
	require_once("ns_exception.php");
	class ns_sql{
		var $link,
			$server,$name,$pwd,$db;
		function connect($sql_server, $sql_name, $sql_pwd, $sql_db){
			$this->server = $sql_server;
			$this->name = $sql_name;
			$this->pwd = $sql_pwd;
			$this->db = $sql_db;
			$this->link = @mysql_connect($this->server, $this->name, $this->pwd);
    			if ($this->link === FALSE) 
				throw new ns_exception("Could not connect : " . mysql_error());
			if($sql_db != "")
			  mysql_select_db($this->db) or die("Could not select database");
		
		}

		function disconnect(){
    			mysql_close($this->link);
		}
		function restart(){
			$this->disconnect();
			$this->connect($this->server,$this->name,$this->pwd,$this->db);
		}
		
		//place sql results in a 2x2 matrix $result.
		function get_row(&$query,&$result, $debug = FALSE){
			$result = array();
			if ($debug)
				echo "Sending query ". $query . "<BR>\n";
	    	$res1 = mysql_query($query) or die("Query failed : $query<br>\n" . mysql_error());
			$i = (int)0;
			while($row = mysql_fetch_row($res1)){
				if ($debug)
					echo ("Getting row.<BR>");
				$result[$i] = $row;																																																																			
				$i = $i+1;
			}
			if ($debug)
				echo "Returning " . sizeof($result) . " results.<BR>";
			mysql_free_result($res1);
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
			$res1 = mysql_query($query) or die("Query failed : $query<br>\n" . mysql_error());
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
