
#ifndef __AN2131_H__
#define __AN2131_H__

#include <usb.h>

int	ezusb_load_firmware	(struct usb_device * dev, const unsigned char * hex_file, int usb_timeout);

#endif
