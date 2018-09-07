#include "ns_usb.h"
#include "ns_thread.h"
#if NS_USB_1 == 1
#define USE_NEW_USB
#endif

#ifndef _WIN32
#ifdef USE_NEW_USB
#include <libusb-1.0/libusb.h>
#else
#include <usb.h>
#endif
#endif

ns_usb_context ns_usb_control::usb_context;
void * ns_usb_context::get_context(){
	#ifndef _WIN32
#ifdef USE_NEW_USB
  if (context == 0){
    libusb_context * c;
    int r = libusb_init(&c);
    context = c;
    if (r < 0)
      throw ns_ex("ns_usb_control::Could not initialize libusb");
  }
#else
	if (context == 0){
  		usb_init();
		context = (void *)this;
	}
#endif
	#endif
  return context;
};
void ns_usb_context::release_context(){
	#ifndef _WIN32
  if (context == 0)
    return;
#ifdef USE_NEW_USB
  libusb_exit((libusb_context *)context);
#else
  //don't need to do anything
#endif
	#endif
  context = 0;
}
ns_usb_context::~ns_usb_context(){
  release_context();
}

bool ns_usb_control::reset_device(int bus,int device_id){
	#ifdef _WIN32
	return false;
	#else

	#ifdef USE_NEW_USB
	libusb_device **devices;
	int r;
	ssize_t cnt;
	ns_acquire_lock_for_scope lock(usb_context.libusb_lock,__FILE__,__LINE__);

	libusb_context * context = (libusb_context *)usb_context.get_context();
	try{
	  cnt = libusb_get_device_list(context, &devices);
	  if (cnt < 0)
	    throw ns_ex("ns_usb_control::Could not enumerate devices!");
	  try{
	    libusb_device *d;
	    libusb_device *requested_device(0);
	    int i = 0;

	    while ((d= devices[i++]) != NULL) {
	      struct libusb_device_descriptor desc;
	      unsigned long bus_number(libusb_get_bus_number(d));
	      unsigned long device_address(libusb_get_device_address(d));
	      if (bus_number == bus && device_address == device_id){
		requested_device = d;
	      }
	      else
		libusb_unref_device(d);//don't hold onto devices that aren't used
	    }


	    if(requested_device==0){
	      libusb_free_device_list(devices,0);
	      lock.release();
	      return 0;
	    }
	    struct libusb_device_handle *device_handle;
	    int err = libusb_open(requested_device,&device_handle);
	    if (err != 0)
	      throw ns_ex("ns_usb_control::Could not open device: ") << err;

	    err = libusb_reset_device(device_handle);
	    if (err){
	      libusb_close(device_handle);
	      libusb_unref_device(requested_device);
	      throw ns_ex("ns_usb::reset_device::Could not reset device: ") << err;
	    }
	    libusb_close(device_handle);
	    libusb_unref_device(requested_device);
	  }
	  catch(...){
	    //don't leak device name lists
	    libusb_free_device_list(devices, 0);
	    throw;
	  }
	}
	catch(...){
	  //release the context so that it can be reset
	  usb_context.release_context();
	  throw;
	}

	libusb_free_device_list(devices, 0);
	lock.release();
	return true;
#else

	ns_acquire_lock_for_scope lock(usb_context.libusb_lock,__FILE__,__LINE__);
	usb_find_busses();
	usb_find_devices();

	struct usb_device *device(0);
	ssize_t i = 0;
	int err = 0;

	for (struct usb_bus * cur_bus = usb_busses; cur_bus; cur_bus= cur_bus->next){
		int cur_bus_number(atoi(cur_bus->dirname));
		for (struct usb_device *dev = cur_bus->devices; dev; dev = dev->next){
			int cur_device_id(dev->devnum);
			//char * filename(dev->filename);

			if (cur_bus_number == bus && device_id == cur_device_id){
				device = dev;
				break;
			}
		}
	}


	if(!device){
		lock.release();
		return 0;
	}
	struct usb_dev_handle *handle;
	handle = usb_open(device);
	try{
		if (err)
			throw ns_ex("ns_usb::reset_device::Could not acquire handle for device");

		err = usb_reset(handle);
		//err2 = usb_reset(handle);
		if (err)
			throw ns_ex("ns_usb::reset_device::Could not reset device");
		usb_close(handle);
	}
	catch(...){
		usb_close(handle);
		throw;
	}
	lock.release();
	return true;
#endif
#endif
}
