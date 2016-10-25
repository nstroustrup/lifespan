#include "ns_image_registration_cache.h"
#include "ns_gaussian_pyramid.h"


ns_64_bit ns_image_fast_registration_profile::size_in_memory_in_kbytes() const {
	return (pyramid->properties().width*
		pyramid->properties().height *
		sizeof(ns_8_bit)) / 1024;
}

void ns_image_fast_registration_profile::load_from_external_source(const ns_image_server_image & im, ns_image_fast_registration_profile_data_source & data_source) {
	image_record = im;
	ns_image_storage_source_handle<ns_8_bit> source(data_source.image_storage->request_from_storage(image_record, data_source.sql));
	pyramid->recieve_and_calculate(source);
}
void ns_image_fast_registration_profile::clean_up(ns_image_fast_registration_profile_data_source & data_source) {
	pyramid->clear();
}
