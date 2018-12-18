#pragma once
#include "ns_simple_cache.h"
#include "ns_death_time_annotation.h"
struct ns_annotation_region_data_id {
	ns_annotation_region_data_id() {};
	ns_annotation_region_data_id(const ns_death_time_annotation_set::ns_annotation_type_to_load & a, const ns_64_bit & id_) :annotations(a), id(id_) {}
	ns_death_time_annotation_set::ns_annotation_type_to_load annotations;
	ns_64_bit id;
	bool operator < (const ns_annotation_region_data_id & other) const {
		if (other.id < id)
			return true;
		return (int)other.annotations < (int)annotations;
	}
};

struct ns_lifespan_curve_cache_entry_data {
	unsigned long region_compilation_timestamp;
	ns_death_time_annotation_compiler compiler;
	unsigned long latest_movement_rebuild_timestamp;
	unsigned long latest_by_hand_annotation_timestamp;

	ns_survival_data_with_censoring risk_timeseries;
	std::vector<unsigned long> risk_timeseries_time;
	ns_region_metadata metadata;

	void load(const ns_annotation_region_data_id & id, ns_sql & sql);
	bool check_to_see_if_cached_is_most_recent(const ns_64_bit & id, ns_sql & sql);
};
class ns_lifespan_curve_cache_entry {
public:
	ns_survival_data_with_censoring cached_strain_risk_timeseries;
	std::vector<unsigned long> cached_strain_risk_timeseries_time;
	ns_region_metadata cached_strain_risk_timeseries_metadata;

	const ns_lifespan_curve_cache_entry_data &get_region_entry(const ns_64_bit & region_id) const;
	
	ns_death_time_annotation_compiler::ns_region_list::const_iterator get_annotations_for_region(const ns_64_bit & region_id) const;

	ns_death_time_annotation_compiler region_data;
	typedef std::map<ns_64_bit, ns_lifespan_curve_cache_entry_data> ns_region_raw_cache;
	ns_region_raw_cache region_raw_data_cache;

	typedef ns_annotation_region_data_id id_type;

	static const ns_annotation_region_data_id & to_id(const ns_annotation_region_data_id & id) { return id; }
	typedef ns_sql external_source_type;
	void load_from_external_source(const ns_annotation_region_data_id & id, ns_sql & sql);

	ns_64_bit size_in_memory_in_kbytes() {
		return 1;
	}
private:
	ns_64_bit experiment_id;
	//all region information ordered by their region_id
	const ns_64_bit id() const { return experiment_id; }
	void clean_up(ns_sql & sql) {}
};

typedef ns_simple_cache<ns_lifespan_curve_cache_entry, ns_annotation_region_data_id, true> ns_experiment_surival_data_cache;

