#ifndef NS_IMAGE_SIMPLE_CACHE
#define NS_IMAGE_SIMPLE_CACHE
#include "ns_image.h"
#include "ns_simple_cache.h"
#include "ns_sql.h"

class ns_image_storage_handler;

struct ns_image_cache_data_source {
	ns_sql * sql;
	ns_image_storage_handler * handler;
};


ns_image_storage_source_handle<ns_8_bit> ns_storage_request_from_storage(ns_image_storage_handler * image_storage, ns_image_server_image & im, ns_sql & sql);

template<class ns_component>
class ns_image_cache_data : public ns_simple_cache_data<ns_image_server_image, ns_image_cache_data_source> {
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

	ns_64_bit id() const { return image_record.id; }
	ns_64_bit to_id(const ns_image_server_image & im) const { return im.id; }
};

typedef  ns_simple_cache < ns_image_cache_data<ns_8_bit>, true> ns_simple_image_cache;

#endif
