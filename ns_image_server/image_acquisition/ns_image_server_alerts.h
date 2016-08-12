#ifndef NS_IMAGE_SERVER_ALERTS_H
#define NS_IMAGE_SERVER_ALERTS_H
#include "ns_sql.h"
#include "ns_thread.h"
#include "ns_image_server_sql.h"
#include "ns_sql_table_lock_manager.h"

struct ns_alert{	

	typedef enum{
		ns_low_urgency,
		ns_medium_urgency,
		ns_high_urgency
	} 
	ns_notification_type;
	
	typedef enum{
		ns_device_error,
		ns_missed_capture,
		ns_long_term_storage_error,
		ns_volatile_storage_error,
		ns_test_alert,
		ns_autoscan_alert,
		ns_server_crash,
		ns_capture_buffering_warning,
		ns_low_disk_space_warning,
		ns_number_of_alert_types
	} 
	ns_alert_type;

	typedef enum{
		ns_not_rate_limited,
		ns_rate_limited
	}
	ns_rate_limitation;


	ns_alert():notification_type(ns_low_urgency),alert_type(ns_number_of_alert_types),rate_limitation(ns_not_rate_limited){}
	ns_alert(const std::string & summary_text_,const std::string & detailed_text_,const ns_alert_type & a,const ns_notification_type n, const ns_rate_limitation rl_)
		:summary_text(summary_text_),detailed_text(detailed_text_),alert_type(a),notification_type(n),rate_limitation(rl_){}
	
	
	static ns_notification_type get_notification_type(const ns_alert_type, bool sent_by_a_image_capture_server);
	static std::string alert_type_label(const ns_alert_type & a);

	std::string summary_text,
				detailed_text;
	ns_rate_limitation rate_limitation;
	ns_alert_type alert_type;
	ns_notification_type notification_type;
};

struct ns_alert_recipient{
	ns_alert_recipient(){}
	ns_alert_recipient(const ns_alert::ns_notification_type & t, const std::string & e):type(t),email(e){}
	ns_alert::ns_notification_type type;
	std::string email;
	bool matches(const ns_alert::ns_notification_type & t){
		return (t==ns_alert::ns_high_urgency || 
			   (t==ns_alert::ns_medium_urgency && t!=ns_alert::ns_high_urgency) || 
				(t==ns_alert::ns_low_urgency && t==ns_alert::ns_low_urgency));
	}
};

class ns_alert_handler{
public:


	ns_alert_handler();

	void initialize(ns_sql & sql);

	void submit_alert(const ns_alert & alert,ns_sql & sql);
	void submit_locally_buffered_alert(const ns_alert & alert);

	void buffer_all_alerts_locally(const bool delayed){buffer_all_alerts_locally_ = delayed;}
	void submit_buffered_alerts(ns_sql & sql);

	void reset_alert_time_limit(const ns_alert::ns_alert_type a,ns_sql & sql);

	void submit_desperate_alert(const std::string & text);

	void reset_all_alert_time_limits(ns_sql & sql);
	void handle_alerts(ns_sql & sql);

	bool can_send_alerts(){
		//reset statistics every two hours
		unsigned long current_time = ns_current_time();
		if (current_time - last_alert_sending_time_interval_start >= 2*60*60){
			number_of_alerts_sent_successfully = 0;
			number_of_alert_sending_failures = 0;
			last_alert_sending_time_interval_start = current_time;
		}
		return number_of_alerts_sent_successfully + 2 > number_of_alert_sending_failures;
	}

	private:
		
	void submit_not_rate_limited_alert(const ns_alert & alert, ns_sql & sql);
	void submit_rate_limited_alert(const ns_alert & alert, ns_sql & sql);

	unsigned long number_of_alerts_sent_successfully;
	unsigned long number_of_alert_sending_failures;
	unsigned long last_alert_sending_time_interval_start;

	bool buffer_all_alerts_locally_;

	static void duration_key_name(const ns_alert::ns_alert_type a,std::string & key);

	static void last_action_time_key_name(const ns_alert::ns_alert_type a,std::string & key);
	
	void get_locked_submission_information_for_alert(const ns_alert::ns_alert_type a, unsigned long & duration_until_next_submission, unsigned long & time_of_last_submission, ns_sql & sql,ns_sql_table_lock & lock);

	void email_alert(const std::string & text, std::vector<std::string> & recipients, const bool report_to_db);
	std::vector<ns_alert_recipient> recipients;
	std::vector<unsigned long> initial_alert_delays;
	unsigned long initial_desperate_alert_delay(){return 10;}
	ns_lock recipient_lock;
	unsigned long time_of_last_desperate_alert_submission;
	unsigned long duration_until_next_desperate_alert_submission;
	std::vector<ns_alert> local_alert_buffer;
};


#endif
