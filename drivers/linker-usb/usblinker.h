/* 
 * Based on f2a by Ulrich Hecht <uli@emulinks.de>
 * if2a by D. Gauchard <deyv@free.fr>
 * F2A Ultra support by Vincent Rubiolo <vincent.rubiolo@free.fr>
 * Licensed under the terms of the GNU Public License version 2
 */
 
// USB-related header

#ifndef __USBLINKER_H__
#define __USBLINKER_H__

#include <usb.h>

/* 
 * USB timeouts. cart_usb_timeout as globlal so that the routines can tweak it
 * depending on their needs
 * Default value is USB_TIMEOUT
 */
#define USB_TIMEOUT			8000
extern int cart_usb_timeout;

void		linker_usb_reinit		(void);
usb_dev_handle*	linker_usb_open			(struct usb_device* dev, int init_hack);
int		linker_usb_connect		(int first_stage_major, int first_stage_minor,
						 int second_stage_major, int second_stage_minor,
						 int usb_interface, int usb_configuration,
						 int usb_read_endpoint, int usb_write_endpoint);
void		linker_usb_disconnect 		(void);
void		linker_usb_release 		();
int		linker_usb_read 		(unsigned char* buffer, int buffer_size);
int		linker_usb_write		(const unsigned char* buffer, int buffer_size);
int		linker_usb_write_by_block 	(const unsigned char* buffer, int buffer_size, int block_size);

#endif // __USBLINKER_H__
