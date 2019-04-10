#ifndef NS_IMAGE_SIMPLE_CACHE
#define NS_IMAGE_SIMPLE_CACHE
#include "ns_image.h"
#include "ns_simple_cache.h"
#include "ns_sql.h"

class ns_image_storage_handler;

struct ns_image_cache_data_source {
	ns_image_cache_data_source() :sql(0), handler(0) {}
	ns_image_cache_data_source(ns_image_storage_handler * h, ns_sql * s):sql(s), handler(h) {}
	ns_sql * sql;
	const ns_image_storage_handler * handler;
};


ns_image_storage_source_handle<ns_8_bit> ns_storage_request_from_storage(const ns_image_storage_handler * image_storage, ns_image_server_image & im, ns_sql & sql);
ns_image_storage_source_handle<ns_8_bit> ns_request_cached_image(const ns_image_storage_handler * image_storage, ns_image_server_image & im, ns_sql & sql, std::string & unique_id);

void ns_delete_from_local_cache(const ns_image_storage_handler * image_storage, const std::string & id);


template<class ns_component>
class ns_image_cache_data : public ns_simple_cache_data<ns_image_server_image, ns_image_cache_data_source,ns_64_bit> {
public:
	ns_image_whole<ns_component> image;
	ns_image_server_image image_record;

	ns_64_bit size_in_memory_in_kbytes() const {
		return (image.properties().width*
			image.properties().height*
			image.properties().components *
			sizeof(ns_component)) / 1024;
	}
	void load_from_external_source(const ns_image_server_image & im, ns_image_cache_data_source & source) {
		image_record = im;
		ns_image_storage_source_handle<ns_component> s(ns_storage_request_from_storage(source.handler, image_record, *source.sql));
		s.input_stream().pump(image, 1024);
	}
	void clean_up(ns_image_cache_data_source & source) {}

	const ns_64_bit & id() const { return image_record.id; }
	static ns_64_bit to_id(const ns_image_server_image & im) { return im.id; }
};

template<class ns_component>
class ns_image_locally_cached_data : public ns_simple_cache_data<ns_image_server_image, ns_image_cache_data_source, ns_64_bit> {
	std::string local_cache_lookup_id;
public:
	ns_image_storage_source_handle<ns_component> source;
	ns_image_server_image image_record;

	ns_64_bit size_in_memory_in_kbytes() const {
		return (source.input_stream().properties().width*
			source.input_stream().properties().height*
			source.input_stream().properties().components *
			sizeof(ns_component)) / 1024;
	}
	void load_from_external_source(const ns_image_server_image & im, ns_image_cache_data_source & source_) {
		image_record = im;
		source = ns_request_cached_image(source_.handler, image_record, *source_.sql, local_cache_lookup_id);
	}
	void clean_up(ns_image_cache_data_source & source) { if (!local_cache_lookup_id.empty()) ns_delete_from_local_cache(source.handler,local_cache_lookup_id); }

	const ns_64_bit & id() const { return image_record.id; }
	static ns_64_bit to_id(const ns_image_server_image & im) { return im.id; }
};

typedef ns_simple_cache < ns_image_cache_data<ns_8_bit>, ns_64_bit, true> ns_simple_image_cache;
typedef ns_simple_cache < ns_image_locally_cached_data<ns_8_bit>, ns_64_bit, true> ns_simple_local_image_cache;


#endif
