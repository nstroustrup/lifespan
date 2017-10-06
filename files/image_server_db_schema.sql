-- --------------------------------------------------------
-- Host:                         mysqlsrv02.linux.crg.es
-- Server version:               5.1.73 - Source distribution
-- Server OS:                    redhat-linux-gnu
-- HeidiSQL Version:             9.4.0.5125
-- --------------------------------------------------------

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET NAMES utf8 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;

-- Dumping structure for table image_server_archive_2017.alerts
CREATE TABLE IF NOT EXISTS `alerts` (
  `host_id` int(10) unsigned NOT NULL,
  `time` int(10) unsigned NOT NULL,
  `text` text NOT NULL,
  `recipients` text NOT NULL,
  `acknowledged` tinyint(1) NOT NULL DEFAULT '0',
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `email_sender_host_id` int(10) unsigned NOT NULL DEFAULT '0',
  `critical_alert` tinyint(1) NOT NULL DEFAULT '0',
  `detailed_text` text NOT NULL,
  `detailed_recipients` text,
  PRIMARY KEY (`id`)
) ENGINE=MyISAM AUTO_INCREMENT=5929 DEFAULT CHARSET=latin1;

-- Data exporting was unselected.
-- Dumping structure for table image_server_archive_2017.animal_storyboard
CREATE TABLE IF NOT EXISTS `animal_storyboard` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `region_id` int(10) unsigned NOT NULL DEFAULT '0',
  `sample_id` int(10) unsigned NOT NULL DEFAULT '0',
  `experiment_id` int(10) unsigned NOT NULL DEFAULT '0',
  `image_id` int(10) unsigned NOT NULL DEFAULT '0',
  `metadata_id` int(10) unsigned NOT NULL DEFAULT '0',
  `using_by_hand_annotations` int(10) unsigned NOT NULL DEFAULT '0',
  `strain` text NOT NULL,
  `movement_event_used` int(10) unsigned NOT NULL DEFAULT '0',
  `aligned_by_absolute_time` int(10) unsigned NOT NULL DEFAULT '0',
  `storyboard_sub_image_number` int(10) unsigned NOT NULL DEFAULT '0',
  `number_of_sub_images` int(10) unsigned NOT NULL DEFAULT '0',
  `images_chosen_from_time_of_last_death` int(10) unsigned NOT NULL DEFAULT '0',
  `image_delay_time_after_event` int(10) unsigned NOT NULL DEFAULT '0',
  `minimum_distance_to_juxtipose_neighbors` int(11) NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  KEY `subject` (`region_id`,`sample_id`,`experiment_id`)
) ENGINE=InnoDB AUTO_INCREMENT=21136 DEFAULT CHARSET=latin1;

-- Data exporting was unselected.
-- Dumping structure for table image_server_archive_2017.annotation_flags
CREATE TABLE IF NOT EXISTS `annotation_flags` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `label_short` text NOT NULL,
  `label` text NOT NULL,
  `exclude` tinyint(1) NOT NULL,
  `next_flag_id_in_order` int(10) unsigned NOT NULL DEFAULT '0',
  `hidden` tinyint(1) unsigned NOT NULL DEFAULT '0',
  `color` varchar(6) NOT NULL DEFAULT '000000',
  `next_flag_name_in_order` text NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=MyISAM AUTO_INCREMENT=15 DEFAULT CHARSET=latin1;

-- Data exporting was unselected.
-- Dumping structure for table image_server_archive_2017.automated_job_scheduling_data
CREATE TABLE IF NOT EXISTS `automated_job_scheduling_data` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `currently_running_host_id` int(11) NOT NULL DEFAULT '0',
  `acquisition_time` int(10) unsigned NOT NULL DEFAULT '0',
  `next_run_time` int(10) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`)
) ENGINE=MEMORY AUTO_INCREMENT=2 DEFAULT CHARSET=latin1;

-- Data exporting was unselected.
-- Dumping structure for table image_server_archive_2017.autoscan_schedule
CREATE TABLE IF NOT EXISTS `autoscan_schedule` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `device_name` varchar(256) NOT NULL DEFAULT '0',
  `autoscan_start_time` int(10) unsigned NOT NULL DEFAULT '0',
  `autoscan_completed_time` int(10) unsigned NOT NULL DEFAULT '0',
  `scan_interval` int(10) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  KEY `lookup` (`device_name`,`autoscan_completed_time`,`autoscan_start_time`)
) ENGINE=MyISAM AUTO_INCREMENT=4488 DEFAULT CHARSET=latin1;

-- Data exporting was unselected.
-- Dumping structure for table image_server_archive_2017.captured_images
CREATE TABLE IF NOT EXISTS `captured_images` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `image_id` int(10) unsigned NOT NULL DEFAULT '0',
  `processing_host_id` int(10) unsigned NOT NULL DEFAULT '0',
  `last_modified` int(10) unsigned NOT NULL DEFAULT '0',
  `experiment_id` int(10) unsigned NOT NULL DEFAULT '0',
  `sample_id` int(10) unsigned NOT NULL DEFAULT '0',
  `capture_time` bigint(20) unsigned NOT NULL DEFAULT '0',
  `currently_being_processed` int(10) unsigned NOT NULL DEFAULT '0' COMMENT 'Set nonzero if the current image is currently checked out for processing',
  `mask_applied` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `problem` int(10) unsigned NOT NULL DEFAULT '0',
  `registration_vertical_offset` int(11) NOT NULL DEFAULT '0',
  `registration_horizontal_offset` int(10) NOT NULL DEFAULT '0',
  `registration_offset_calculated` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `censored` tinyint(1) NOT NULL DEFAULT '0',
  `image_statistics_id` bigint(20) unsigned NOT NULL DEFAULT '0',
  `small_image_id` int(10) unsigned NOT NULL DEFAULT '0',
  `never_delete_image` tinyint(1) NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  KEY `image_id_reverse_lookup` (`image_id`),
  KEY `time` (`sample_id`,`capture_time`),
  KEY `job_search` (`sample_id`,`currently_being_processed`,`mask_applied`,`problem`,`small_image_id`,`censored`,`image_id`) USING BTREE
) ENGINE=InnoDB AUTO_INCREMENT=6596929 DEFAULT CHARSET=latin1;

-- Data exporting was unselected.
-- Dumping structure for table image_server_archive_2017.capture_samples
CREATE TABLE IF NOT EXISTS `capture_samples` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `parameters` text NOT NULL,
  `name` varchar(32) NOT NULL,
  `experiment_id` int(10) unsigned NOT NULL DEFAULT '0',
  `description` text NOT NULL,
  `mask_id` int(10) unsigned NOT NULL DEFAULT '0',
  `device_id` varchar(12) NOT NULL DEFAULT '',
  `problem` tinyint(1) NOT NULL DEFAULT '0',
  `device_name` varchar(23) NOT NULL DEFAULT '0',
  `model_filename` text NOT NULL,
  `long_capture_interval` int(10) unsigned NOT NULL DEFAULT '50',
  `short_capture_interval` int(10) unsigned NOT NULL DEFAULT '25',
  `apply_vertical_image_registration` tinyint(3) unsigned NOT NULL DEFAULT '1',
  `turn_off_lamp_after_capture` tinyint(1) NOT NULL DEFAULT '0',
  `censored` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `position_x` float NOT NULL DEFAULT '0',
  `position_y` float NOT NULL DEFAULT '0',
  `size_x` float NOT NULL DEFAULT '0',
  `size_y` float NOT NULL DEFAULT '0',
  `excluded_from_analysis` tinyint(1) unsigned NOT NULL DEFAULT '0',
  `first_frames_are_protected` tinyint(1) NOT NULL DEFAULT '0',
  `analysis_scheduling_state` int(10) unsigned NOT NULL DEFAULT '0',
  `size_unprocessed_captured_images` int(10) unsigned NOT NULL DEFAULT '0',
  `size_processed_captured_images` int(10) unsigned NOT NULL DEFAULT '0',
  `size_unprocessed_region_images` int(10) unsigned NOT NULL DEFAULT '0',
  `size_processed_region_images` int(10) unsigned NOT NULL DEFAULT '0',
  `size_metadata` int(10) unsigned NOT NULL DEFAULT '0',
  `size_calculation_time` int(10) unsigned NOT NULL DEFAULT '0',
  `op0_video_id` int(10) unsigned NOT NULL DEFAULT '0',
  `reason_censored` text NOT NULL,
  `incubator_name` text NOT NULL,
  `incubator_location` text NOT NULL,
  `time_stamp` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  `image_resolution_dpi` double NOT NULL DEFAULT '-1',
  `raw_image_size_in_bytes` int(10) unsigned NOT NULL DEFAULT '0',
  `desired_capture_duration_in_seconds` int(10) unsigned NOT NULL DEFAULT '0',
  `device_capture_period_in_seconds` int(10) unsigned NOT NULL DEFAULT '0',
  `number_of_consecutive_captures_per_sample` int(10) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  KEY `text_find` (`parameters`(400)),
  KEY `device_lookup` (`id`,`device_name`)
) ENGINE=InnoDB AUTO_INCREMENT=31547 DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC COMMENT='different samples imaged on the scanner with different param';

-- Data exporting was unselected.
-- Dumping structure for table image_server_archive_2017.capture_schedule
CREATE TABLE IF NOT EXISTS `capture_schedule` (
  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `experiment_id` int(10) unsigned NOT NULL DEFAULT '0',
  `scheduled_time` bigint(20) unsigned NOT NULL DEFAULT '0',
  `time_at_start` bigint(20) unsigned NOT NULL DEFAULT '0',
  `time_at_finish` bigint(20) unsigned NOT NULL DEFAULT '0',
  `problem` bigint(20) unsigned NOT NULL DEFAULT '0',
  `sample_id` int(10) unsigned NOT NULL DEFAULT '0',
  `captured_image_id` bigint(20) unsigned NOT NULL DEFAULT '0' COMMENT 'Points to the captured_image entry that resulted from the scan',
  `missed` tinyint(1) NOT NULL DEFAULT '0',
  `time_at_imaging_start` bigint(20) unsigned NOT NULL DEFAULT '0',
  `transferred_to_long_term_storage` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `time_spent_reading_from_device` bigint(20) unsigned NOT NULL DEFAULT '0',
  `time_spent_writing_to_disk` bigint(20) unsigned NOT NULL DEFAULT '0',
  `total_time_during_read` bigint(20) unsigned NOT NULL DEFAULT '0',
  `time_during_transfer_to_long_term_storage` bigint(20) unsigned NOT NULL DEFAULT '0',
  `time_during_deletion_from_local_storage` bigint(20) unsigned NOT NULL DEFAULT '0',
  `time_stamp` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  `censored` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `uploaded_to_central_db` int(10) unsigned NOT NULL DEFAULT '0',
  `total_time_spent_during_programmed_delay` int(10) unsigned NOT NULL DEFAULT '0',
  `scanning_time_for_decile_0` int(10) unsigned NOT NULL DEFAULT '0',
  `scanning_time_for_decile_1` int(10) unsigned NOT NULL DEFAULT '0',
  `scanning_time_for_decile_2` int(10) unsigned NOT NULL DEFAULT '0',
  `scanning_time_for_decile_3` int(10) unsigned NOT NULL DEFAULT '0',
  `scanning_time_for_decile_4` int(10) unsigned NOT NULL DEFAULT '0',
  `scanning_time_for_decile_5` int(10) unsigned NOT NULL DEFAULT '0',
  `scanning_time_for_decile_6` int(10) unsigned NOT NULL DEFAULT '0',
  `scanning_time_for_decile_7` int(10) unsigned NOT NULL DEFAULT '0',
  `scanning_time_for_decile_8` int(10) unsigned NOT NULL DEFAULT '0',
  `scanning_time_for_decile_9` int(10) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  KEY `image_reverse_lookup` (`captured_image_id`),
  KEY `shed` (`scheduled_time`,`sample_id`),
  KEY `device_lookup` (`scheduled_time`,`time_at_start`,`sample_id`,`experiment_id`,`missed`,`problem`,`time_at_finish`,`censored`) USING BTREE
) ENGINE=InnoDB AUTO_INCREMENT=12858257 DEFAULT CHARSET=latin1 COMMENT='InnoDB free: 121856 kB';

-- Data exporting was unselected.
-- Dumping structure for table image_server_archive_2017.constants
CREATE TABLE IF NOT EXISTS `constants` (
  `k` text NOT NULL COMMENT 'key',
  `v` text NOT NULL COMMENT 'value',
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `time_stamp` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=200 DEFAULT CHARSET=latin1;

-- Data exporting was unselected.
-- Dumping structure for table image_server_archive_2017.daily_quotes
CREATE TABLE IF NOT EXISTS `daily_quotes` (
  `quote` text,
  `stock` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `author` text NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

-- Data exporting was unselected.
-- Dumping structure for table image_server_archive_2017.delete_file_jobs
CREATE TABLE IF NOT EXISTS `delete_file_jobs` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `confirmed` int(10) unsigned NOT NULL DEFAULT '0',
  `parent_job_id` int(10) unsigned NOT NULL DEFAULT '0' COMMENT 'points to the file deletion specification job that produced this deletion job',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=46 DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;

-- Data exporting was unselected.
-- Dumping structure for table image_server_archive_2017.delete_file_specifications
CREATE TABLE IF NOT EXISTS `delete_file_specifications` (
  `delete_job_id` int(10) unsigned DEFAULT NULL,
  `relative_directory` text NOT NULL,
  `filename` text NOT NULL,
  `partition` text NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

-- Data exporting was unselected.
-- Dumping structure for table image_server_archive_2017.devices
CREATE TABLE IF NOT EXISTS `devices` (
  `name` varchar(256) NOT NULL,
  `comments` text NOT NULL,
  `host_id` int(10) unsigned NOT NULL DEFAULT '0',
  `preview_requested` int(10) unsigned NOT NULL DEFAULT '0',
  `barcode_image_id` int(10) unsigned NOT NULL DEFAULT '0',
  `in_recognized_error_state` int(10) unsigned NOT NULL DEFAULT '0',
  `error_text` text NOT NULL,
  `id` int(11) NOT NULL DEFAULT '0',
  `unknown_identity` tinyint(1) NOT NULL DEFAULT '0',
  `simulated_device` tinyint(1) NOT NULL DEFAULT '0',
  `pause_captures` tinyint(1) NOT NULL DEFAULT '0',
  `currently_scanning` int(10) unsigned NOT NULL DEFAULT '0',
  `last_capture_start_time` int(10) unsigned NOT NULL DEFAULT '0',
  `autoscan_interval` int(10) unsigned NOT NULL DEFAULT '0',
  `last_autoscan_time` int(10) unsigned NOT NULL DEFAULT '0',
  `next_autoscan_time` int(10) unsigned NOT NULL DEFAULT '0',
  KEY `name_lookup` (`name`(5),`in_recognized_error_state`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC COMMENT='InnoDB free: 6144 kB';

-- Data exporting was unselected.
-- Dumping structure for table image_server_archive_2017.device_inventory
CREATE TABLE IF NOT EXISTS `device_inventory` (
  `device_name` varchar(23) NOT NULL,
  `incubator_name` varchar(45) NOT NULL DEFAULT 'unknown',
  `incubator_location` varchar(10) NOT NULL DEFAULT '?'
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

-- Data exporting was unselected.
-- Dumping structure for table image_server_archive_2017.experiments
CREATE TABLE IF NOT EXISTS `experiments` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `name` text NOT NULL,
  `description` text NOT NULL,
  `first_time_point` bigint(20) unsigned NOT NULL DEFAULT '0',
  `num_time_points` int(10) unsigned NOT NULL DEFAULT '0',
  `hidden` tinyint(1) NOT NULL DEFAULT '0',
  `last_time_point` bigint(20) unsigned NOT NULL DEFAULT '0',
  `partition` text NOT NULL,
  `delete_captured_images_after_mask` tinyint(1) NOT NULL DEFAULT '1',
  `run_automated_job_scheduling` int(10) unsigned NOT NULL DEFAULT '1',
  `size_unprocessed_captured_images` int(10) unsigned NOT NULL DEFAULT '0',
  `size_processed_captured_images` int(10) unsigned NOT NULL DEFAULT '0',
  `size_unprocessed_region_images` int(10) unsigned NOT NULL DEFAULT '0',
  `size_processed_region_images` int(10) unsigned NOT NULL DEFAULT '0',
  `size_metadata` int(10) unsigned NOT NULL DEFAULT '0',
  `size_video` int(10) unsigned NOT NULL DEFAULT '0',
  `size_calculation_time` int(10) unsigned NOT NULL DEFAULT '0',
  `priority` int(10) unsigned NOT NULL DEFAULT '1000',
  `time_stamp` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  `group_id` int(10) unsigned NOT NULL DEFAULT '0',
  `control_strain_for_device_regression` text,
  `last_timepoint_in_latest_storyboard_build` bigint(20) unsigned NOT NULL DEFAULT '0',
  `latest_storyboard_build_timestamp` bigint(20) unsigned NOT NULL DEFAULT '0',
  `number_of_regions_in_latest_storyboard_build` bigint(20) unsigned NOT NULL DEFAULT '0',
  `mask_time` bigint(20) unsigned NOT NULL DEFAULT '0',
  `compression_type` char(50) NOT NULL DEFAULT 'jp2k',
  `compression_ratio` float NOT NULL DEFAULT '0.05',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=1100 DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;

-- Data exporting was unselected.
-- Dumping structure for table image_server_archive_2017.experiment_groups
CREATE TABLE IF NOT EXISTS `experiment_groups` (
  `group_id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `group_name` text NOT NULL,
  `hidden` tinyint(3) unsigned NOT NULL,
  `group_order` int(10) unsigned NOT NULL,
  PRIMARY KEY (`group_id`)
) ENGINE=InnoDB AUTO_INCREMENT=20 DEFAULT CHARSET=latin1;

-- Data exporting was unselected.
-- Dumping structure for table image_server_archive_2017.hosts
CREATE TABLE IF NOT EXISTS `hosts` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `ip` varchar(15) NOT NULL DEFAULT '',
  `comments` text NOT NULL,
  `last_ping` int(10) unsigned NOT NULL DEFAULT '0',
  `name` varchar(18) NOT NULL DEFAULT '',
  `long_term_storage_enabled` tinyint(1) NOT NULL DEFAULT '0',
  `port` int(10) unsigned NOT NULL DEFAULT '0',
  `software_version_major` int(10) unsigned NOT NULL DEFAULT '0',
  `software_version_minor` int(10) unsigned NOT NULL DEFAULT '0',
  `software_version_compile` int(10) unsigned NOT NULL DEFAULT '0',
  `shutdown_requested` tinyint(1) NOT NULL DEFAULT '0',
  `launch_from_screen_saver` tinyint(1) unsigned NOT NULL DEFAULT '0',
  `pause_requested` tinyint(1) NOT NULL DEFAULT '0',
  `hotplug_requested` tinyint(1) unsigned NOT NULL DEFAULT '0',
  `base_host_name` varchar(18) NOT NULL DEFAULT '',
  `database_used` text NOT NULL,
  `available_space_in_volatile_storage_in_mb` bigint(20) unsigned NOT NULL DEFAULT '0',
  `time_of_last_successful_long_term_storage_write` bigint(20) unsigned NOT NULL,
  `system_hostname` char(255) NOT NULL DEFAULT '',
  `additional_host_description` char(255) NOT NULL DEFAULT '',
  `system_parallel_process_id` int(10) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=592 DEFAULT CHARSET=latin1;

-- Data exporting was unselected.
-- Dumping structure for table image_server_archive_2017.host_event_log
CREATE TABLE IF NOT EXISTS `host_event_log` (
  `host_id` int(10) unsigned NOT NULL DEFAULT '0',
  `event` text NOT NULL,
  `time` int(10) unsigned NOT NULL DEFAULT '0',
  `error` tinyint(4) NOT NULL DEFAULT '0',
  `minor` tinyint(4) NOT NULL DEFAULT '0',
  `processing_job_op` int(10) unsigned NOT NULL DEFAULT '0',
  `parent_event_id` bigint(20) unsigned NOT NULL DEFAULT '0',
  `subject_experiment_id` int(10) unsigned NOT NULL DEFAULT '0',
  `subject_sample_id` int(10) unsigned NOT NULL DEFAULT '0',
  `subject_region_info_id` int(10) unsigned NOT NULL DEFAULT '0',
  `subject_region_image_id` int(10) unsigned NOT NULL DEFAULT '0',
  `subject_captured_image_id` int(10) unsigned NOT NULL DEFAULT '0',
  `subject_width` int(10) unsigned NOT NULL DEFAULT '0',
  `subject_height` int(10) unsigned NOT NULL DEFAULT '0',
  `subject_image_id` int(10) unsigned NOT NULL DEFAULT '0',
  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `processing_duration` bigint(20) unsigned NOT NULL DEFAULT '0',
  `sub_text` mediumtext NOT NULL,
  `node_id` int(11) NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  KEY `time_index` (`time`),
  KEY `host_index` (`host_id`,`time`),
  KEY `sub_events` (`parent_event_id`,`time`),
  KEY `duration` (`processing_duration`)
) ENGINE=MyISAM AUTO_INCREMENT=45836685 DEFAULT CHARSET=latin1 COMMENT='InnoDB free: 204800 kB';

-- Data exporting was unselected.
-- Dumping structure for table image_server_archive_2017.images
CREATE TABLE IF NOT EXISTS `images` (
  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `filename` text NOT NULL,
  `host_id` int(10) unsigned NOT NULL DEFAULT '0',
  `creation_time` bigint(20) unsigned NOT NULL DEFAULT '0',
  `path` text NOT NULL COMMENT 'directory of storage',
  `currently_under_processing` int(10) unsigned NOT NULL DEFAULT '0' COMMENT 'set nonzero if image is currently checked out by a processing node',
  `problem` bigint(20) unsigned NOT NULL DEFAULT '0',
  `partition` text NOT NULL,
  PRIMARY KEY (`id`),
  KEY `host_lookup` (`host_id`)
) ENGINE=InnoDB AUTO_INCREMENT=260671993 DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;

-- Data exporting was unselected.
-- Dumping structure for table image_server_archive_2017.image_masks
CREATE TABLE IF NOT EXISTS `image_masks` (
  `image_id` int(10) unsigned NOT NULL DEFAULT '0',
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `processed` tinyint(1) NOT NULL DEFAULT '0',
  `visualization_image_id` int(10) unsigned NOT NULL DEFAULT '0',
  `resize_factor` int(10) unsigned NOT NULL DEFAULT '1',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=15520 DEFAULT CHARSET=latin1;

-- Data exporting was unselected.
-- Dumping structure for table image_server_archive_2017.image_mask_regions
CREATE TABLE IF NOT EXISTS `image_mask_regions` (
  `mask_id` int(10) unsigned NOT NULL DEFAULT '0',
  `x_min` int(10) unsigned NOT NULL DEFAULT '0',
  `y_min` int(10) unsigned NOT NULL DEFAULT '0',
  `x_max` int(10) unsigned NOT NULL DEFAULT '0',
  `y_max` int(10) unsigned NOT NULL DEFAULT '0',
  `pixel_count` int(10) unsigned NOT NULL DEFAULT '0',
  `y_average` int(10) unsigned NOT NULL DEFAULT '0',
  `x_average` int(10) unsigned NOT NULL DEFAULT '0',
  `mask_value` int(10) unsigned NOT NULL COMMENT 'the pixel value of this region in the mask image',
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  PRIMARY KEY (`id`),
  KEY `mask` (`mask_id`)
) ENGINE=InnoDB AUTO_INCREMENT=2405 DEFAULT CHARSET=latin1;

-- Data exporting was unselected.
-- Dumping structure for table image_server_archive_2017.image_statistics
CREATE TABLE IF NOT EXISTS `image_statistics` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `histogram` blob NOT NULL,
  `intensity_average` float NOT NULL DEFAULT '0',
  `intensity_entropy` float NOT NULL DEFAULT '0',
  `intensity_std` float NOT NULL DEFAULT '0',
  `intensity_top_percentile` float NOT NULL DEFAULT '0',
  `intensity_bottom_percentile` float NOT NULL DEFAULT '0',
  `size_x` int(10) unsigned NOT NULL DEFAULT '0',
  `size_y` int(10) unsigned NOT NULL DEFAULT '0',
  `worm_count` int(10) unsigned NOT NULL DEFAULT '0',
  `worm_area_mean` float NOT NULL DEFAULT '0',
  `worm_area_variance` float NOT NULL DEFAULT '0',
  `worm_length_mean` float NOT NULL DEFAULT '0',
  `worm_length_variance` float NOT NULL DEFAULT '0',
  `worm_intensity_mean` float NOT NULL DEFAULT '0',
  `worm_intensity_variance` float NOT NULL DEFAULT '0',
  `worm_width_mean` float NOT NULL DEFAULT '0',
  `worm_width_variance` float NOT NULL DEFAULT '0',
  `non_worm_object_count` int(10) unsigned NOT NULL DEFAULT '0',
  `non_worm_object_area_mean` float NOT NULL DEFAULT '0',
  `non_worm_object_area_variance` float NOT NULL DEFAULT '0',
  `non_worm_object_intensity_mean` float NOT NULL DEFAULT '0',
  `non_worm_object_intensity_variance` float NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`)
) ENGINE=MyISAM AUTO_INCREMENT=18295130 DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;

-- Data exporting was unselected.
-- Dumping structure for table image_server_archive_2017.path_data
CREATE TABLE IF NOT EXISTS `path_data` (
  `region_id` bigint(20) unsigned NOT NULL,
  `group_id` int(10) unsigned NOT NULL,
  `path_id` int(10) unsigned NOT NULL,
  `image_id` bigint(20) unsigned NOT NULL,
  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `flow_image_id` bigint(20) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  KEY `reg_id` (`region_id`,`group_id`,`path_id`) USING BTREE
) ENGINE=InnoDB AUTO_INCREMENT=1634845 DEFAULT CHARSET=latin1;

-- Data exporting was unselected.
-- Dumping structure for table image_server_archive_2017.performance_statistics
CREATE TABLE IF NOT EXISTS `performance_statistics` (
  `host_id` int(10) unsigned NOT NULL DEFAULT '0',
  `operation` int(10) unsigned NOT NULL,
  `mean` float NOT NULL,
  `variance` float NOT NULL,
  `count` int(10) unsigned NOT NULL
) ENGINE=MEMORY DEFAULT CHARSET=latin1;

-- Data exporting was unselected.
-- Dumping structure for table image_server_archive_2017.processing_jobs
CREATE TABLE IF NOT EXISTS `processing_jobs` (
  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `experiment_id` bigint(20) unsigned NOT NULL DEFAULT '0',
  `sample_id` bigint(20) unsigned NOT NULL DEFAULT '0',
  `region_id` bigint(20) unsigned NOT NULL DEFAULT '0',
  `image_id` bigint(20) unsigned NOT NULL DEFAULT '0',
  `urgent` tinyint(1) unsigned NOT NULL DEFAULT '0',
  `processor_id` bigint(20) unsigned NOT NULL DEFAULT '0' COMMENT 'requests can be submitted to specific hosts.  If this is nonzero, the specified host will perform it',
  `time_submitted` int(10) unsigned NOT NULL DEFAULT '0',
  `op1` tinyint(1) NOT NULL DEFAULT '0',
  `op2` tinyint(1) NOT NULL DEFAULT '0',
  `op3` tinyint(1) NOT NULL DEFAULT '0',
  `op4` tinyint(1) NOT NULL DEFAULT '0',
  `op5` tinyint(1) NOT NULL DEFAULT '0',
  `op6` tinyint(1) NOT NULL DEFAULT '0',
  `op7` tinyint(1) NOT NULL DEFAULT '0',
  `op8` tinyint(1) NOT NULL DEFAULT '0',
  `op9` tinyint(1) NOT NULL DEFAULT '0',
  `op10` tinyint(1) NOT NULL DEFAULT '0',
  `op11` tinyint(1) NOT NULL DEFAULT '0',
  `op12` tinyint(1) NOT NULL DEFAULT '0',
  `op13` tinyint(1) NOT NULL DEFAULT '0',
  `op14` tinyint(1) NOT NULL DEFAULT '0',
  `op15` tinyint(1) NOT NULL DEFAULT '0',
  `op16` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `op17` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `op18` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `op19` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `op20` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `op21` tinyint(1) unsigned NOT NULL DEFAULT '0',
  `op22` tinyint(1) unsigned NOT NULL DEFAULT '0',
  `op23` tinyint(1) unsigned NOT NULL DEFAULT '0',
  `op24` tinyint(1) unsigned NOT NULL DEFAULT '0',
  `op25` tinyint(1) unsigned NOT NULL DEFAULT '0',
  `op26` tinyint(1) unsigned NOT NULL DEFAULT '0',
  `op27` tinyint(1) NOT NULL DEFAULT '0',
  `op28` tinyint(1) NOT NULL DEFAULT '0',
  `op29` tinyint(1) NOT NULL DEFAULT '0',
  `op30` tinyint(1) NOT NULL DEFAULT '0',
  `mask_id` bigint(20) unsigned NOT NULL DEFAULT '0' COMMENT 'for mask analysis requests, the mask id must be provided.',
  `problem` bigint(20) unsigned NOT NULL DEFAULT '0',
  `currently_under_processing` int(10) unsigned NOT NULL DEFAULT '0',
  `locked` int(10) unsigned NOT NULL DEFAULT '0',
  `op0` tinyint(3) NOT NULL DEFAULT '0',
  `maintenance_task` int(10) unsigned NOT NULL DEFAULT '0',
  `job_name` varchar(128) NOT NULL DEFAULT '',
  `processed_by_push_scheduler` tinyint(1) unsigned NOT NULL DEFAULT '0',
  `subregion_position_x` int(10) unsigned NOT NULL DEFAULT '0',
  `subregion_position_y` int(10) unsigned NOT NULL DEFAULT '0',
  `subregion_width` int(10) unsigned NOT NULL DEFAULT '0',
  `subregion_height` int(10) unsigned NOT NULL DEFAULT '0',
  `subregion_start_time` int(10) unsigned NOT NULL DEFAULT '0',
  `subregion_stop_time` int(10) unsigned NOT NULL DEFAULT '0',
  `delete_file_job_id` int(10) unsigned NOT NULL DEFAULT '0',
  `video_add_timestamp` tinyint(1) NOT NULL DEFAULT '0',
  `maintenance_flag` int(10) unsigned NOT NULL DEFAULT '0',
  `paused` int(10) unsigned NOT NULL DEFAULT '0',
  `pending_another_jobs_completion` int(11) NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=283495 DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC COMMENT='InnoDB free: 92160 kB; InnoDB free: 309248 kB';

-- Data exporting was unselected.
-- Dumping structure for table image_server_archive_2017.processing_job_log
CREATE TABLE IF NOT EXISTS `processing_job_log` (
  `host_id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `job_id` int(10) unsigned NOT NULL DEFAULT '0',
  `time_start` bigint(20) unsigned NOT NULL DEFAULT '0',
  `time_stop` bigint(20) unsigned NOT NULL DEFAULT '0',
  `experiment_id` int(10) unsigned NOT NULL DEFAULT '0',
  `sample_id` int(10) unsigned NOT NULL DEFAULT '0',
  `region_id` int(10) unsigned NOT NULL DEFAULT '0',
  `image_id` int(10) unsigned NOT NULL DEFAULT '0',
  `processor_id` int(10) unsigned NOT NULL,
  `problem` int(10) unsigned NOT NULL,
  PRIMARY KEY (`host_id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

-- Data exporting was unselected.
-- Dumping structure for table image_server_archive_2017.processing_job_queue
CREATE TABLE IF NOT EXISTS `processing_job_queue` (
  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `priority` int(10) unsigned NOT NULL DEFAULT '0',
  `experiment_id` int(10) unsigned NOT NULL DEFAULT '0',
  `capture_sample_id` int(10) unsigned NOT NULL DEFAULT '0',
  `sample_region_info_id` int(10) unsigned NOT NULL DEFAULT '0',
  `sample_region_id` int(10) unsigned NOT NULL DEFAULT '0',
  `image_id` int(10) unsigned NOT NULL DEFAULT '0',
  `processor_id` int(10) unsigned NOT NULL DEFAULT '0',
  `problem` bigint(20) unsigned NOT NULL DEFAULT '0',
  `progress` int(10) unsigned NOT NULL DEFAULT '0',
  `job_id` int(10) unsigned NOT NULL DEFAULT '0',
  `movement_record_id` int(10) unsigned NOT NULL DEFAULT '0',
  `captured_images_id` int(10) unsigned NOT NULL DEFAULT '0',
  `job_name` varchar(128) NOT NULL,
  `job_submission_time` int(10) unsigned NOT NULL DEFAULT '0',
  `job_class` int(10) unsigned NOT NULL DEFAULT '0',
  `paused` int(10) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  KEY `priority_index` (`priority`,`problem`,`job_class`,`processor_id`) USING BTREE
) ENGINE=MyISAM AUTO_INCREMENT=9904590 DEFAULT CHARSET=latin1;

-- Data exporting was unselected.
-- Dumping structure for table image_server_archive_2017.processing_node_status
CREATE TABLE IF NOT EXISTS `processing_node_status` (
  `host_id` bigint(20) DEFAULT NULL,
  `node_id` bigint(20) DEFAULT NULL,
  `current_processing_job_queue_id` bigint(20) DEFAULT NULL,
  `current_processing_job_id` bigint(20) DEFAULT NULL,
  `state` varchar(20) NOT NULL DEFAULT '',
  `current_output_event_id` bigint(20) DEFAULT NULL,
  `ts` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

-- Data exporting was unselected.
-- Dumping structure for table image_server_archive_2017.sample_region_images
CREATE TABLE IF NOT EXISTS `sample_region_images` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `region_info_id` int(10) unsigned NOT NULL DEFAULT '0',
  `capture_time` int(10) unsigned NOT NULL DEFAULT '0',
  `image_id` int(10) unsigned NOT NULL DEFAULT '0',
  `op1_image_id` int(10) unsigned NOT NULL DEFAULT '0',
  `op2_image_id` int(10) unsigned NOT NULL DEFAULT '0',
  `op3_image_id` int(10) unsigned NOT NULL DEFAULT '0',
  `last_modified` int(10) unsigned DEFAULT NULL,
  `op4_image_id` int(11) NOT NULL DEFAULT '0',
  `op5_image_id` int(10) unsigned NOT NULL DEFAULT '0',
  `op6_image_id` int(10) unsigned NOT NULL DEFAULT '0',
  `op7_image_id` int(10) unsigned NOT NULL DEFAULT '0',
  `op8_image_id` int(10) unsigned NOT NULL DEFAULT '0',
  `op9_image_id` int(10) unsigned NOT NULL DEFAULT '0',
  `op10_image_id` int(10) unsigned NOT NULL DEFAULT '0',
  `op11_image_id` int(10) unsigned NOT NULL DEFAULT '0',
  `op12_image_id` int(10) unsigned NOT NULL DEFAULT '0',
  `op13_image_id` int(10) unsigned NOT NULL DEFAULT '0',
  `op14_image_id` int(10) unsigned NOT NULL DEFAULT '0',
  `op15_image_id` int(10) unsigned NOT NULL DEFAULT '0',
  `op16_image_id` int(10) unsigned NOT NULL DEFAULT '0',
  `op17_image_id` int(10) unsigned NOT NULL DEFAULT '0',
  `op18_image_id` int(10) unsigned NOT NULL DEFAULT '0',
  `op19_image_id` int(10) unsigned NOT NULL DEFAULT '0',
  `op20_image_id` int(10) unsigned NOT NULL DEFAULT '0',
  `op21_image_id` int(10) unsigned NOT NULL DEFAULT '0',
  `op22_image_id` int(10) unsigned NOT NULL DEFAULT '0',
  `op23_image_id` int(10) unsigned NOT NULL DEFAULT '0',
  `op24_image_id` int(10) unsigned NOT NULL DEFAULT '0',
  `op25_image_id` int(10) unsigned NOT NULL DEFAULT '0',
  `op26_image_id` int(10) unsigned NOT NULL DEFAULT '0',
  `op27_image_id` int(10) unsigned NOT NULL DEFAULT '0',
  `op28_image_id` int(10) unsigned NOT NULL DEFAULT '0',
  `op29_image_id` int(10) unsigned NOT NULL DEFAULT '0',
  `op30_image_id` int(10) unsigned NOT NULL DEFAULT '0',
  `capture_sample_image_id` int(10) unsigned NOT NULL DEFAULT '0' COMMENT 'a link back to the capture sample image from which this region was cut',
  `problem` bigint(20) unsigned NOT NULL DEFAULT '0',
  `worm_detection_results_id` int(10) unsigned NOT NULL DEFAULT '0',
  `worm_interpolation_results_id` int(10) unsigned NOT NULL DEFAULT '0',
  `currently_under_processing` bigint(20) NOT NULL DEFAULT '0',
  `worm_movement_id` int(10) unsigned NOT NULL DEFAULT '0',
  `vertical_image_registration_applied` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `censored` tinyint(1) NOT NULL DEFAULT '0',
  `image_statistics_id` bigint(20) unsigned NOT NULL DEFAULT '0',
  `make_training_set_image_from_frame` int(10) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  KEY `job_lookup` (`region_info_id`,`problem`,`currently_under_processing`),
  KEY `movement_index` (`region_info_id`,`worm_movement_id`)
) ENGINE=InnoDB AUTO_INCREMENT=19364157 DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;

-- Data exporting was unselected.
-- Dumping structure for table image_server_archive_2017.sample_region_image_aligned_path_images
CREATE TABLE IF NOT EXISTS `sample_region_image_aligned_path_images` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `region_info_id` int(10) unsigned NOT NULL DEFAULT '0',
  `frame_index` int(10) unsigned NOT NULL DEFAULT '0',
  `image_id` int(10) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  KEY `region_lookup` (`region_info_id`,`frame_index`)
) ENGINE=MyISAM AUTO_INCREMENT=571487 DEFAULT CHARSET=latin1;

-- Data exporting was unselected.
-- Dumping structure for table image_server_archive_2017.sample_region_image_info
CREATE TABLE IF NOT EXISTS `sample_region_image_info` (
  `mask_region_id` int(10) unsigned NOT NULL DEFAULT '0',
  `details` text NOT NULL,
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `name` text NOT NULL,
  `sample_id` int(10) unsigned NOT NULL DEFAULT '0',
  `mask_id` int(10) unsigned NOT NULL DEFAULT '0',
  `subregion_mask_id` bigint(20) NOT NULL DEFAULT '0',
  `temporal_interpolation_performed` int(10) unsigned NOT NULL DEFAULT '0',
  `op21_image_id` int(10) unsigned NOT NULL DEFAULT '0',
  `op22_image_id` int(10) unsigned NOT NULL DEFAULT '0',
  `censored` tinyint(1) NOT NULL DEFAULT '0',
  `reason_censored` text NOT NULL,
  `time_at_which_animals_had_zero_age` int(10) unsigned NOT NULL DEFAULT '0',
  `strain` text NOT NULL,
  `strain_condition_1` text NOT NULL,
  `strain_condition_2` text NOT NULL,
  `strain_condition_3` text NOT NULL,
  `culturing_temperature` text NOT NULL,
  `experiment_temperature` text NOT NULL,
  `food_source` text NOT NULL,
  `environmental_conditions` text NOT NULL,
  `excluded_from_analysis` tinyint(1) unsigned NOT NULL DEFAULT '0',
  `time_of_last_valid_sample` int(10) unsigned NOT NULL DEFAULT '0',
  `movement_file_triplet_id` int(10) unsigned NOT NULL DEFAULT '0',
  `movement_file_triplet_interpolated_id` int(10) unsigned NOT NULL DEFAULT '0',
  `movement_file_time_path_id` int(10) unsigned NOT NULL DEFAULT '0',
  `movement_file_time_path_image_id` int(10) unsigned NOT NULL DEFAULT '0',
  `path_movement_images_are_cached` int(11) NOT NULL DEFAULT '0',
  `time_path_solution_id` int(10) unsigned NOT NULL DEFAULT '0',
  `movement_image_analysis_quantification_id` int(10) unsigned NOT NULL DEFAULT '0',
  `analysis_scheduling_state` int(10) unsigned NOT NULL DEFAULT '0',
  `op0_video_id` int(10) unsigned NOT NULL DEFAULT '0',
  `op2_video_id` int(10) unsigned NOT NULL DEFAULT '0',
  `op3_video_id` int(10) unsigned NOT NULL DEFAULT '0',
  `op4_video_id` int(10) unsigned NOT NULL DEFAULT '0',
  `op5_video_id` int(10) unsigned NOT NULL DEFAULT '0',
  `op6_video_id` int(10) unsigned NOT NULL DEFAULT '0',
  `op7_video_id` int(10) unsigned NOT NULL DEFAULT '0',
  `op8_video_id` int(10) unsigned NOT NULL DEFAULT '0',
  `op17_video_id` int(10) unsigned NOT NULL DEFAULT '0',
  `op20_video_id` int(10) unsigned NOT NULL DEFAULT '0',
  `op24_video_id` int(10) unsigned NOT NULL DEFAULT '0',
  `op25_video_id` int(10) unsigned NOT NULL DEFAULT '0',
  `op26_video_id` int(10) unsigned NOT NULL DEFAULT '0',
  `op27_video_id` int(10) unsigned NOT NULL DEFAULT '0',
  `number_of_frames_used_to_mask_stationary_objects` int(10) unsigned NOT NULL DEFAULT '0',
  `maximum_number_of_worms_per_plate` int(10) unsigned NOT NULL DEFAULT '100',
  `latest_movement_rebuild_timestamp` int(10) unsigned NOT NULL DEFAULT '0',
  `last_timepoint_in_latest_movement_rebuild` int(10) unsigned NOT NULL DEFAULT '0',
  `latest_by_hand_annotation_timestamp` int(10) unsigned NOT NULL DEFAULT '0',
  `number_of_timepoints_in_latest_movement_rebuild` int(10) unsigned NOT NULL DEFAULT '0',
  `position_in_sample_x` double NOT NULL DEFAULT '0',
  `position_in_sample_y` double NOT NULL DEFAULT '0',
  `size_x` double NOT NULL DEFAULT '0',
  `size_y` double NOT NULL DEFAULT '0',
  `posture_analysis_model` text NOT NULL,
  `posture_analysis_method` varchar(10) NOT NULL,
  `worm_detection_model` text NOT NULL,
  `measured_temperature` double NOT NULL DEFAULT '0',
  `time_series_denoising_flag` int(10) unsigned NOT NULL DEFAULT '1',
  `position_analysis_model` text NOT NULL,
  `op30_image_id` bigint(20) NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  KEY `experiment` (`mask_region_id`)
) ENGINE=InnoDB AUTO_INCREMENT=56782 DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC COMMENT='InnoDB free: 6144 kB';

-- Data exporting was unselected.
-- Dumping structure for table image_server_archive_2017.sample_time_relationships
CREATE TABLE IF NOT EXISTS `sample_time_relationships` (
  `time` bigint(20) unsigned NOT NULL DEFAULT '0',
  `previous_short` bigint(20) unsigned NOT NULL DEFAULT '0',
  `previous_long` bigint(20) unsigned NOT NULL DEFAULT '0' COMMENT 'previous long-interval time point',
  `next_long` bigint(20) unsigned NOT NULL DEFAULT '0' COMMENT 'next long-interval time point',
  `next_short` bigint(20) unsigned NOT NULL DEFAULT '0' COMMENT 'next short-interval time point',
  `sample_id` bigint(20) unsigned NOT NULL DEFAULT '0',
  KEY `sample_time` (`sample_id`,`time`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

-- Data exporting was unselected.
-- Dumping structure for table image_server_archive_2017.strain_aliases
CREATE TABLE IF NOT EXISTS `strain_aliases` (
  `strain` text NOT NULL,
  `genotype` text NOT NULL,
  `conditions` text NOT NULL,
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `used_in_cluster` tinyint(1) NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  KEY `s` (`used_in_cluster`,`strain`(15)) USING BTREE
) ENGINE=MyISAM AUTO_INCREMENT=12194 DEFAULT CHARSET=latin1;

-- Data exporting was unselected.
-- Dumping structure for table image_server_archive_2017.worm_detection_results
CREATE TABLE IF NOT EXISTS `worm_detection_results` (
  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `source_image_id` bigint(20) unsigned NOT NULL DEFAULT '0',
  `capture_sample_id` bigint(20) unsigned NOT NULL DEFAULT '0',
  `worm_segment_node_counts` blob NOT NULL COMMENT 'int[i] = number of segments in worm i',
  `worm_segment_information` longblob NOT NULL COMMENT 'w_s_i[3*i + 0..3]  = {node[i].x, node[i].y, node[i].width}',
  `number_of_worms` int(10) unsigned NOT NULL DEFAULT '0',
  `worm_region_information` blob NOT NULL,
  `bitmap_tiles_per_row` int(10) unsigned NOT NULL DEFAULT '0',
  `bitmap_tile_width` int(10) unsigned NOT NULL DEFAULT '0',
  `bitmap_tile_height` int(10) unsigned NOT NULL DEFAULT '0',
  `worm_fast_movement_mapping` blob NOT NULL,
  `worm_slow_movement_mapping` blob NOT NULL,
  `worm_movement_state` blob NOT NULL,
  `worm_movement_fast_speed` blob NOT NULL,
  `worm_movement_slow_speed` blob NOT NULL,
  `worm_movement_tags` blob NOT NULL,
  `number_of_interpolated_worm_areas` int(10) unsigned NOT NULL DEFAULT '0',
  `interpolated_worm_areas` blob NOT NULL,
  `data_storage_on_disk_id` bigint(20) unsigned NOT NULL COMMENT 'Worm segment info can be stored in this db, or alternatively saved to disk with filename information provided by the image table row number worm_segment_information_id ',
  PRIMARY KEY (`id`),
  KEY `reverse_sample_lookup` (`capture_sample_id`)
) ENGINE=MyISAM AUTO_INCREMENT=22400523 DEFAULT CHARSET=latin1 CHECKSUM=1 COMMENT='myISAM';

-- Data exporting was unselected.
-- Dumping structure for table image_server_archive_2017.worm_movement
CREATE TABLE IF NOT EXISTS `worm_movement` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `sample_id` int(10) unsigned NOT NULL DEFAULT '0',
  `region_info_id` int(10) unsigned NOT NULL DEFAULT '0',
  `time` int(10) unsigned NOT NULL DEFAULT '0',
  `number_total` int(10) unsigned NOT NULL DEFAULT '0',
  `number_moving_fast` int(10) unsigned NOT NULL DEFAULT '0',
  `number_moving_slow` int(10) unsigned NOT NULL DEFAULT '0',
  `number_changing_posture` int(10) unsigned NOT NULL DEFAULT '0',
  `number_stationary` int(10) unsigned NOT NULL DEFAULT '0',
  `region_id_short_1` int(10) unsigned NOT NULL DEFAULT '0',
  `region_id_short_2` int(10) unsigned NOT NULL DEFAULT '0',
  `region_id_long` int(10) unsigned NOT NULL DEFAULT '0',
  `calculated` tinyint(1) NOT NULL DEFAULT '0',
  `currently_under_processing` tinyint(1) NOT NULL DEFAULT '0',
  `problem` bigint(20) unsigned NOT NULL DEFAULT '0',
  `region_id_previous_short` int(10) unsigned NOT NULL DEFAULT '0',
  `region_id_previous_long` int(10) unsigned NOT NULL DEFAULT '0',
  `interp_number_total` int(10) unsigned NOT NULL DEFAULT '0',
  `interp_number_moving_fast` int(10) unsigned NOT NULL DEFAULT '0',
  `interp_number_moving_slow` int(10) unsigned NOT NULL DEFAULT '0',
  `interp_number_changing_posture` int(10) unsigned NOT NULL DEFAULT '0',
  `interp_number_stationary` int(10) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  KEY `region_time` (`region_info_id`,`time`)
) ENGINE=MyISAM AUTO_INCREMENT=428056 DEFAULT CHARSET=latin1;

-- Data exporting was unselected.
/*!40101 SET SQL_MODE=IFNULL(@OLD_SQL_MODE, '') */;
/*!40014 SET FOREIGN_KEY_CHECKS=IF(@OLD_FOREIGN_KEY_CHECKS IS NULL, 1, @OLD_FOREIGN_KEY_CHECKS) */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
