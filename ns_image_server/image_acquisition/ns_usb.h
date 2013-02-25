#ifndef NS_USB_H
#define NS_USB_H

#include "ns_ex.h"
#include "ns_thread.h"
struct ns_usb_context{
ns_usb_context():context(0),libusb_lock("libusb_lock"){}
  void * context;
  void * get_context();
  void  release_context();
  ~ns_usb_context();
   ns_lock libusb_lock;
};
class ns_usb_control{
public:
	static bool reset_device(int bus,int device_id);
private:
	static ns_usb_context usb_context;
};

#endif
