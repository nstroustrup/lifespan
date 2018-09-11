#include "ns_image_registration_cache.h"
#include "ns_gaussian_pyramid.h"


ns_64_bit ns_image_fast_registration_profile::size_in_memory_in_kbytes() const {
	return (pyramid->properties().width*
		pyramid->properties().height *
		sizeof(ns_8_bit)) / 1024;
}
ns_image_storage_source_handle<ns_8_bit> ns_image_fast_registration_profile::full_res_image(ns_image_fast_registration_profile_data_source & data_source) const{
	return data_source.image_storage->request_from_local_cache(local_cache_filename, false);
}
void ns_image_fast_registration_profile::delete_cached_file(ns_image_fast_registration_profile_data_source & data_source) const {
	if (!local_cache_filename.empty())
		data_source.image_storage->delete_from_local_cache(local_cache_filename);
	local_cache_filename.clear();
}

void ns_image_fast_registration_profile::load_from_external_source(const ns_image_server_image & im, ns_image_fast_registration_profile_data_source & data_source) {
	image_record = im;
	if (!image_record.load_from_db(im.id, data_source.sql))
		throw ns_ex("Invalid image spec");
	//compress everything in the cache as tiff
	local_cache_filename = data_source.image_storage->add_to_local_cache(image_record, ns_tiff_lzw,data_source.sql);
	ns_image_storage_source_handle<ns_8_bit> source(data_source.image_storage->request_from_local_cache(local_cache_filename, false));
	properties = source.input_stream().properties();
	pyramid = new ns_gaussian_pyramid();
	pyramid->recieve_and_calculate(source, (int)ns_registration_downsample_factor,histogram);
}
void ns_image_fast_registration_profile::clean_up(ns_image_fast_registration_profile_data_source & data_source) {
	pyramid->clear();
	delete_cached_file(data_source);
}


ns_image_fast_registration_profile::~ns_image_fast_registration_profile() {
	ns_safe_delete(pyramid);
	pyramid = 0;
}