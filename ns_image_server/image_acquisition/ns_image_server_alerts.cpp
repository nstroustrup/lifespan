#include "ns_image_server_alerts.h"
#include "ns_image_server.h"
//#include "ns_image_server_dispatcher.h"
#include "ns_asynchronous_read.h"

using namespace std;
class ns_email_sender{
public:
	ns_email_sender():delayed_ex_thrown(false){}

	void send_email(const string & sendmail_text){	
		unsigned long capture_buffer_size(100);
		ns_external_execute exec;

		ns_asynchronous_read_info<ns_email_sender> read_info;
		ns_external_execute_options opt;
		opt.take_stdin_handle = true;
		exec.run(image_server.mail_path().c_str(),"",opt);
		exec.start_timeout(15);
		exec.write_stdin(const_cast<char *>(sendmail_text.c_str()),(unsigned long)sendmail_text.length());
		exec.finished_writing_to_stdin();
		//read from stdio asynchronously.
		
		//cerr << "Running exec";
		ns_asynchronous_read asynchronous_read(&exec);
		read_info.asynch = &asynchronous_read;
		read_info.reciever = this;
	
		ns_thread stderr_thread(asynchronous_read_start<ns_email_sender>,&read_info);
	
		ns_thread::sleep(1);
		exec.finished_reading_from_stdout();
	//	exec.wait_for_termination();
		exec.release_io();
		if (exec.timed_out())
			throw ns_ex("ns_email_sender()::Timed out.");

		//check to see if any errors occurred during asynchronous read.
		if (delayed_ex_thrown)
			throw delayed_ex;

		if (asynchronous_read.result().size() != 0){
			string res = asynchronous_read.result();
			if (res.size() != 0)
				throw ns_ex("Error sending email:") << res;
		}		
	}

	void throw_delayed_exception(const ns_ex & ex){
		delayed_ex_thrown = true;
		delayed_ex = ex;
	}
private:
	ns_ex delayed_ex;
	bool delayed_ex_thrown;
};


void ns_alert_handler::submit_not_rate_limited_alert(const ns_alert & alert, ns_sql & sql){
	if (buffer_all_alerts_locally_){
		submit_locally_buffered_alert(alert);
		return;
	}

	string summary_recipients,
		   detailed_recipients;
	ns_acquire_lock_for_scope recip_lock(recipient_lock,__FILE__,__LINE__);
	for (unsigned int i = 0; i < recipients.size(); i++){
		if (recipients[i].matches(alert.notification_type)) {
			if (recipients[i].type==ns_alert::ns_medium_urgency)
				summary_recipients+=recipients[i].email + ";";
			else detailed_recipients+=recipients[i].email + ";";
		}
	}
	recip_lock.release();
	
	ns_sql_table_lock lock(image_server.sql_table_lock_manager.obtain_table_lock("alerts", &sql, true, __FILE__, __LINE__));

	sql << "INSERT INTO alerts SET time=" << ns_current_time() << ",host_id=" << image_server.host_id() << 
		",text='"<< image_server.host_name_out() << ":" << sql.escape_string(alert.summary_text)<<"',detailed_text='"<<sql.escape_string(alert.detailed_text)<<"'"
		<< ",recipients='" << summary_recipients << "',detailed_recipients='"<<detailed_recipients<<"'";

	sql.send_query();
	
	lock.release(__FILE__, __LINE__);

	image_server.register_server_event(ns_image_server_event("Submitting Alert: ") << alert.detailed_text,&sql);
}

void ns_alert_handler::email_alert(const string & text, vector<string> & recips, const bool report_to_db){
	
	recipient_lock.wait_to_acquire(__FILE__,__LINE__);
	if (recipients.size() == 0){
		recipient_lock.release();
		image_server.register_server_event(ns_image_server::ns_register_in_central_db,ns_image_server_event("No recipients specified for alert: ") << text);
		return;
	}
	recipient_lock.release();
	string subject("Fontanalab Image Server Alert!");
	#ifdef _WIN32 
	
	string out = "From: <stroustr@fas.harvard.edu>\n";

	for (unsigned int i = 0; i < recips.size(); i++)
		out+="To: " + recips[i] + "\n";

	out+="Subject: " + subject + "\n\n";
	out += "(via " + image_server.host_name_out() +")";
	out+=text;
	ns_email_sender email_sender;
	email_sender.send_email(out);
	#else
	string command("echo \"\n");
	for (unsigned int i = 0; i < text.size(); i++){
		if(text[i] == '"')
			command+= '\\';
		command+=text[i];
	}
	command += "\n\" | " + image_server.mail_path() + " -s \"" + subject + "\" ";
	for (unsigned int i = 0; i < recips.size(); i++)
		command += recips[i] + " ";
	system(command.c_str());
	#endif

	if (report_to_db)
		image_server.register_server_event(ns_image_server::ns_register_in_central_db,ns_image_server_event("Sent Alert: ") << text);
	
	return;
}

string ns_alert::alert_type_label(const ns_alert::ns_alert_type & a){
	switch(a){
		case ns_device_error: return "device_error";
		case ns_missed_capture: return "missed_capture";
		case ns_long_term_storage_error: return "long_term_storage_error";
		case ns_volatile_storage_error: return "volatile_storage_error";
		case ns_test_alert: return "test";
		case ns_autoscan_alert: return "autoscan_alert";
		case ns_server_crash: return "server_crash";
		case ns_capture_buffering_warning: return "capture_buffer_warning";
		case ns_low_disk_space_warning: return "low_disk_space_warning";
		default: throw ns_ex("ns_alert_handler::alert_type_label:: Unknown alert type: ") << (unsigned long)(a);
	}
}
void ns_alert_handler::reset_all_alert_time_limits(ns_sql & sql){
	for (unsigned int i = 0; i < (ns_alert::ns_alert_type)ns_alert::ns_number_of_alert_types; ++i)
		reset_alert_time_limit((ns_alert::ns_alert_type)i,sql);
}

void ns_alert_handler::reset_alert_time_limit(const ns_alert::ns_alert_type a,ns_sql & sql){
	ns_sql_table_lock lock = image_server.sql_table_lock_manager.obtain_table_lock("constants", &sql, true, __FILE__, __LINE__);
	try{

	
		string duration_key;
		duration_key_name(a,duration_key);
		string last_time_key;
		last_action_time_key_name(a,last_time_key);

		image_server.set_cluster_constant_value(duration_key,ns_to_string(initial_alert_delays[(unsigned long)a]),&sql);
		image_server.set_cluster_constant_value(last_time_key,"0",&sql);
		lock.release(__FILE__, __LINE__);
	}
	catch(...){
		sql.clear_query();
		lock.release(__FILE__, __LINE__);
		throw;
	}
}

void ns_alert_handler::duration_key_name(const ns_alert::ns_alert_type a,std::string & key){
	key = "duration_until_next_";
	key += ns_alert::alert_type_label(a);
	key += "_alert_submission";
}

void ns_alert_handler::last_action_time_key_name(const ns_alert::ns_alert_type a,std::string & key){
	key = "last_";
	key += ns_alert::alert_type_label(a);
	key += "_alert_submission";
}

ns_sql_table_lock ns_alert_handler::get_locked_submission_information_for_alert(const ns_alert::ns_alert_type a, unsigned long & duration_until_next_submission, unsigned long & time_of_last_submission, ns_sql & sql){
	
	string duration_key;
	duration_key_name(a,duration_key);
	string last_time_key;
	last_action_time_key_name(a,last_time_key);


	ns_sql_table_lock lock(image_server.sql_table_lock_manager.obtain_table_lock("constants", &sql, true, __FILE__, __LINE__));

	duration_until_next_submission = atol(image_server.get_cluster_constant_value(duration_key,ns_to_string(initial_alert_delays[(unsigned long)a]),&sql).c_str());
	try{
		time_of_last_submission = atol(image_server.get_cluster_constant_value(last_time_key,"0",&sql).c_str());

		//if somebody sets the duration less than the minimum, fix it.
		if (duration_until_next_submission < initial_alert_delays[(unsigned long)a]){
			duration_until_next_submission = initial_alert_delays[(unsigned long)a];
			image_server.set_cluster_constant_value(duration_key,ns_to_string(duration_until_next_submission),&sql);
		}
	}
	catch(...){
		sql.clear_query();
		lock.release(__FILE__, __LINE__);
		throw;
	}
	return lock;
}

void ns_alert_handler::submit_locally_buffered_alert(const ns_alert & alert){
	ns_acquire_lock_for_scope lock(recipient_lock,__FILE__,__LINE__);
	local_alert_buffer.push_back(alert);
	lock.release();
}

void ns_alert_handler::submit_buffered_alerts(ns_sql & sql){
	//get all buffered alerts
	ns_acquire_lock_for_scope lock(recipient_lock,__FILE__,__LINE__);
	vector<ns_alert> alerts;
	alerts.insert(alerts.begin(),local_alert_buffer.begin(),local_alert_buffer.end());
	local_alert_buffer.resize(0);
	lock.release();

	for (unsigned int i = 0; i < alerts.size(); ++i){
		if (alerts[i].rate_limitation == ns_alert::ns_rate_limited)
			submit_rate_limited_alert(alerts[i],sql);
		else
			submit_alert(alerts[i],sql);
	}
}

void ns_alert_handler::submit_alert(const ns_alert &alert, ns_sql & sql){
	if (alert.rate_limitation == ns_alert::ns_rate_limited)
		this->submit_rate_limited_alert(alert,sql);
	else submit_not_rate_limited_alert(alert,sql);
}

ns_alert::ns_notification_type ns_alert::get_notification_type(ns_alert::ns_alert_type t, bool submitted_by_image_capture_server){
	switch(t){
		case ns_device_error: return ns_low_urgency;
		case ns_missed_capture: return ns_high_urgency;
		case ns_long_term_storage_error: return (submitted_by_image_capture_server?ns_high_urgency:ns_low_urgency);
		case ns_volatile_storage_error: return (submitted_by_image_capture_server?ns_high_urgency:ns_low_urgency);
		case ns_test_alert: return ns_high_urgency;
		case ns_autoscan_alert: return ns_low_urgency;
		case ns_server_crash: return (submitted_by_image_capture_server?ns_high_urgency:ns_low_urgency);
		case ns_capture_buffering_warning: return ns_high_urgency;
		case ns_low_disk_space_warning: return (submitted_by_image_capture_server?ns_high_urgency:ns_low_urgency);
		case ns_number_of_alert_types: 
		default: 
			throw ns_ex("Asked for a nonsensical notification: ") << (int)t;
	}
}

void ns_alert_handler::submit_rate_limited_alert(const ns_alert & alert,ns_sql & sql){
	if (buffer_all_alerts_locally_){
		submit_locally_buffered_alert(alert);
		return;
	}
	const unsigned long current_time = ns_current_time();
	string duration_key;
	duration_key_name(alert.alert_type,duration_key);
	string last_time_key;
	last_action_time_key_name(alert.alert_type,last_time_key);

	unsigned long duration_until_next_submission,
				  time_of_last_submission;
	ns_sql_table_lock lock(get_locked_submission_information_for_alert(alert.alert_type,duration_until_next_submission,time_of_last_submission,sql));
	try{
		const unsigned long expiration_time(time_of_last_submission+60*duration_until_next_submission);

		const bool duration_expired (current_time > expiration_time);
		const bool duration_very_expired (current_time > expiration_time + 60*initial_alert_delays[(int)alert.alert_type]);

		if (!duration_expired){
			lock.release(__FILE__, __LINE__);
			return;
		}
		
		//if the duration is only barely expired, we keep increasing it
		unsigned long next_duration(2*duration_until_next_submission);

		//if the duration has been expired for a long amount of time, we assume
		//the reported event is a new event, and rest the delay.
		if (duration_very_expired || next_duration == 0)
			next_duration = initial_alert_delays[(int)alert.alert_type];

		image_server.set_cluster_constant_value(last_time_key,ns_to_string(current_time),&sql);
		image_server.set_cluster_constant_value(duration_key,ns_to_string(next_duration),&sql);
		lock.release(__FILE__, __LINE__);
		image_server.alert_handler.submit_not_rate_limited_alert(alert,sql);

	}
	catch(...){
		sql.clear_query();
		lock.release(__FILE__,__LINE__);
		throw;
	}
}
void ns_explode_list(const std::string & s, vector<std::string> & vals){
	string cur;
	for (unsigned int i = 0; i < s.size(); i++){
			if (s[i] == ';'){
				vals.push_back(cur);
				cur.resize(0);
			}
			else cur+=s[i];
		}
		if (cur.size() != 0)
			vals.push_back(cur);
};
void ns_alert_handler::handle_alerts(ns_sql & sql){
	if (image_server.mail_path().size() <= 1)
		return;
	//if there have been a lot of errors recently, don't try to send alerts.
	if (!can_send_alerts())
		return;
	bool autocommit_state(sql.autocommit_state());
	//grab a bunch of alerts
	sql.set_autocommit(false);	
	ns_sql_table_lock lock = image_server.sql_table_lock_manager.obtain_table_lock("alerts", &sql, true, __FILE__, __LINE__);

	ns_sql_result res;
	try{
		sql << "SELECT text,detailed_text, recipients,detailed_recipients, id, time FROM alerts WHERE email_sender_host_id = 0 AND acknowledged=0";
		sql.get_rows(res);
	}
	catch(...){
		lock.release(__FILE__,__LINE__);
		sql.set_autocommit(autocommit_state);
		throw;
	}
	ns_64_bit email_sender_host_id(image_server.host_id());
	if (email_sender_host_id ==0)
		email_sender_host_id = 666;

	for (unsigned int a = 0; a < res.size(); a++){
		sql << "UPDATE alerts SET email_sender_host_id=" << email_sender_host_id << " WHERE id = " << res[a][4];
		sql.send_query();
	}
	lock.release(__FILE__, __LINE__);
	sql.send_query("COMMIT");

	//process alerts
	for (unsigned int a = 0; a < res.size(); a++){
		vector<string> summary_recipients,
					   detailed_recipients;
		ns_explode_list(res[a][2],summary_recipients);
		ns_explode_list(res[a][3],detailed_recipients);
		const std::string time(ns_format_time_string_for_human(atol(res[a][5].c_str())) + " ");
		string summary_text =  time + res[a][0],
			   detailed_text = time + res[a][1];
		
		//try to send the alerts
		try{
			email_alert(summary_text,summary_recipients,true);
			email_alert(detailed_text,detailed_recipients,true);
			number_of_alerts_sent_successfully++;
		}
		catch(...){
			//if the alerts fail, return them to be processed elsewhere
			number_of_alert_sending_failures++;

			ns_sql_table_lock lock(image_server.sql_table_lock_manager.obtain_table_lock("alerts", &sql, true, __FILE__, __LINE__));
			for (unsigned int b = a; b < res.size(); b++){
				sql << "UPDATE alerts SET email_sender_host_id=0 WHERE id = " << res[b][4];
				sql.send_query();
			}
			lock.release(__FILE__, __LINE__);
			sql.set_autocommit(autocommit_state);
			throw;
		}
	}
	sql.set_autocommit(autocommit_state);
}

ns_alert::ns_notification_type ns_notification_type_from_string(const std::string & s){
	if (s == "alert_recipient")
		return ns_alert::ns_medium_urgency;
	if (s == "alert_recipient_low_urgency")
		return ns_alert::ns_low_urgency;
	if (s == "alert_recipient_medium_urgency")
		return ns_alert::ns_medium_urgency;
	if (s == "alert_recipient_high_urgency")
		return ns_alert::ns_high_urgency;
	throw ns_ex("ns_notification_type_from_string()::Unknown notification type: ") << s;
}

void ns_alert_handler::initialize(ns_sql & sql){
	sql << "SELECT v,k FROM constants WHERE "
			"k='alert_recipient' OR "
			"k='alert_recipient_low_urgency' OR "
			"k='alert_recipient_medium_urgency' OR "
			"k='alert_recipient_high_urgency'";
	ns_sql_result res;
	sql.get_rows(res);
	if(res.size() == 0){
		image_server.register_server_event(ns_image_server::ns_register_in_central_db,ns_image_server_event("Note: No alert recipients are specified in the database."));
		image_server.set_cluster_constant_value("alert_recipient_low_urgency",".",&sql);
		image_server.set_cluster_constant_value("alert_recipient_medium_urgency",".",&sql);
		image_server.set_cluster_constant_value("alert_recipient_high_urgency",".",&sql);
		return;
	}

	recipient_lock.wait_to_acquire(__FILE__,__LINE__);

	recipients.resize(0);
	for (unsigned int i = 0; i < res.size(); i++){
		if (res[i][0] == ".")
			continue;
		recipients.push_back(ns_alert_recipient(ns_notification_type_from_string(res[i][1]),res[i][0]));
	}

	recipient_lock.release();
}

ns_alert_handler::ns_alert_handler():
	buffer_all_alerts_locally_(false),
	initial_alert_delays((unsigned long)ns_alert::ns_number_of_alert_types,15),
		time_of_last_desperate_alert_submission(0),recipient_lock("ns_ah::recipient"),
		number_of_alerts_sent_successfully(0), number_of_alert_sending_failures(0),last_alert_sending_time_interval_start(0){
	//IF ANY OF THESE ARE SET TO ZERO, THE ALERT SYSTEM BLOWS UP (sending a gazillion emails).
	initial_alert_delays[(unsigned long)ns_alert::ns_device_error] = 15;
	initial_alert_delays[(unsigned long)ns_alert::ns_missed_capture] = 15;
	initial_alert_delays[(unsigned long)ns_alert::ns_long_term_storage_error] = 15;
	initial_alert_delays[(unsigned long)ns_alert::ns_volatile_storage_error] = 15;
	initial_alert_delays[(unsigned long)ns_alert::ns_test_alert] = 1;
	initial_alert_delays[(unsigned long)ns_alert::ns_autoscan_alert] = 15;
	initial_alert_delays[(unsigned long)ns_alert::ns_server_crash] = 5;
	initial_alert_delays[(unsigned long)ns_alert::ns_capture_buffering_warning] = 15;
	initial_alert_delays[(unsigned long)ns_alert::ns_low_disk_space_warning] = 5;
}

void ns_alert_handler::submit_desperate_alert(const string & text){
	const unsigned long current_time(ns_current_time());
	if (current_time > 	time_of_last_desperate_alert_submission+
						60*duration_until_next_desperate_alert_submission + 
						60*initial_desperate_alert_delay()){
		time_of_last_desperate_alert_submission = 0;
		duration_until_next_desperate_alert_submission = 0;
	}
	
	if(current_time > time_of_last_desperate_alert_submission+
					  60* duration_until_next_desperate_alert_submission){
		time_of_last_desperate_alert_submission = current_time;
		duration_until_next_desperate_alert_submission*= 2;
		if (duration_until_next_desperate_alert_submission == 0)
			duration_until_next_desperate_alert_submission = initial_desperate_alert_delay();
		string txt = ns_format_time_string_for_human(ns_current_time()) + " " + image_server.host_name_out() + " " + text;
		vector<string> r(recipients.size());
		for (unsigned int i = 0; i < recipients.size(); i++)
			r[i] = recipients[i].email;
		image_server.alert_handler.email_alert(txt,r,false);
	}
}
