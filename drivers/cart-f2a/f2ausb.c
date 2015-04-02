/*
 * Based in f2a by Ulrich Hecht <uli@emulinks.de>
 * Multiboot by Eli Curtz <eli@nuprometheus.com>
 * if2a by D. Gauchard <deyv@free.fr>
 * F2A Ultra support by Vincent Rubiolo <vincent.rubiolo@free.fr>
 * Licensed under the terms of the GNU Public License version 2
 */

#include <string.h>

#include "../../binware.h"
#include "../../libf2a.h"
#include "../../cartio.h"
#include "../../cartrom.h"
#include "../../cartutils.h"
#include "../linker-usb/usblinker.h"
#include "f2amisc.h"
#include "f2ausb.h"
#include "f2aio.h"

static int f2a_boot (binware_s* multiboot, binware_s* splash, int usb_timeout_in_seconds)
{
	unsigned char ack[64];
	
	if (cart_io_sim > 1)
		return 0;

	memset(&sm, 0, sizeof(sm));

	// Boot the GBA
	sm.command=CMD_MULTIBOOT1;
	if (f2a_write_msg(&sm) == -1)
		return -1;
	
	// Increase timeout as booting depends on user manipulation
	if (usb_timeout_in_seconds)
		cart_usb_timeout = usb_timeout_in_seconds * 1000;

	sm.command=CMD_MULTIBOOT2;
	sm.size = multiboot->size;
	if (f2a_write_msg(&sm) == -1)
	{
		cart_usb_timeout = USB_TIMEOUT; // restore usb timeout
		return -1;
	}

	// Send the multiboot image
	if (f2a_write(multiboot->data, multiboot->size) == -1)
	{
		cart_usb_timeout = USB_TIMEOUT; // restore usb timeout
		return -1;
	}

	cart_usb_timeout = USB_TIMEOUT; // restore usb timeout
	
	// Compulsory F2A reply
	// NOTE: this is not seen in win32 usb logs
	//       this comes from original code from Ulrich Hecht <uli@emulinks.de>
	if (f2a_read(ack, sizeof(ack)) == -1)
		return -1;

	if (cart_verbose >= 2)
	{
		print("Post-boot:\n");
		print_array_dual(ack, 64);
	}
	
	// Send background picture
	sm.command=CMD_WRITEDATA;
	sm.subcommand=SUBCMD_WRITEDATA;
	sm.size=splash->size;
	sm.magic=MAGIC_NUMBER;
	sm.address=GBA_VRAM;
	sm.sizekb=sm.size/1024;

	// Command
	if (f2a_write_msg(&sm) == -1)
		return -1;
	
	// Actual data
	if (f2a_write(splash->data, splash->size) == -1)
		return -1;

	/* 
	 * Give time to GBA to copy background image to VRAM, we would otherwise
	 * experience info request failures whereas boot had been reported to be 
	 * have occurred fine.
     * This is also important for auto-detection as boot may have occured fine
     * but autodetection will fail because the image is not entirely copied to
     * VRAM.
	 */
	sleep(0.5);

	return 0;
}

/* 
 * Get info from F2A linker
 * This is used to know whether the linker is ready to accept commands
 *
 * Returns:
 *  0 if linker is ready to accept commands
 *  1 if linker is not ready/not initialized
 *  -1 if there was something wrong with the USB commands
 */
static int f2a_info (void)
{
	if (cart_io_sim > 1)
		return 0;

	memset(&sm, 0, sizeof(sm));
	memset(&rm, 0, sizeof(rm));

	sm.command=CMD_GETINF;

	/* 
	 * Info command takes very little time (63 + 64 bytes)
	 * Tweak timeout accordingly
	 */
	cart_usb_timeout = 250; // 250ms
	
	if (f2a_write_msg(&sm) == -1)
		return -1;

	if (f2a_read((unsigned char*)&rm, sizeof(rm)) == -1)
		return -1;

	// Restore timeout to its default value
	cart_usb_timeout = USB_TIMEOUT;
	
	if (cart_verbose > 1)
	{
		print("F2A info:\n");
		print_array_dual((unsigned char*)&rm, sizeof(rm));
	}

	if (rm.data[0] == 0x4)
		return 0; // ready to accept commands
	else
		return 1; // not yet initialized/ready
}

/*
 * This API is used to check whether the linker is available for commands. After
 * renumeration, the linker is expected to reply "not ready".
 *
 * FIXME : there is a fundamental problem here : how to know whether the
 * multiboot image is running? This is important because the linker can reply
 * "busy" with the image running (in which case we should wait) or "busy"
 * without an image running (we should then boot it).
 *
 * Without this distinction, this API will always be broken : we risk waiting
 * indefinitely (image is in fact not loaded) or break things up (send booting
 * command whereas the image is actually booting and needs time to be able to
 * answer queries). A peek at sniffs may have the answer here.
 *
 * update: 2005-10-25 v0.94.3.3: this seems to be stable now
 */
static int f2a_usb_linker_init (void)
{
	int result;
	int problem_counter = 0;

	// After renumeration, a linker not ready means an unitialized linker.
	while ((result = f2a_info()) != 0) // multiboot not yet loaded or error
	{
    		if (result == 1)
    		{
			printerr("Please turn OFF then ON your GBA "
				 "with SELECT and START held down.\n");
			if (f2a_boot(&multiboot, &splash, 20) < 0)
			{
				printerr("Cannot boot GBA.\n");
				return -1;
			}
		}
		else // problem
		{
			print("Linker not ready to accept commands. "
			      "Retrying.\n");
			//msleep(500);
			if (++problem_counter == 5)
			{
				printerr("There was a problem querying the "
					 "USB linker. Disconnect it and "
					 "retry.\n");
				return -1;
			}
		}
	}
    
	if (cart_verbose)
		print("F2A multiboot image uploaded.\n");
	return 0;
}

static int f2a_usb_connect (void)
{
	return linker_usb_connect
	(
		0x547, 0x2131,
		0x547, 0x1002,
		1,	// f2a linker usb configuration
		0x00,	// f2a linker usb interface
		0x83,	// f2a linker usb read endpoint
		0x04	// f2a linker usb write endpoint
	);
}

int select_f2a_firmware (const char* name)
{
	return binware_load(&firmware, binware_f2a_usb_firmware, name, "F2A-usb-linker-firmware");
}

int select_f2a_linker_multiboot (const char* name)
{
	return binware_load(&multiboot, binware_f2a_multiboot, name, "F2A-usb-linker-multiboot");
}

int select_f2a_splash (const char* name)
{
	return binware_load(&splash, binware_f2a_splash, name, "F2A-splash");
}

void cart_reinit_f2a_usb (void)
{
	f2a_read = linker_usb_read;
	f2a_write = linker_usb_write;

	cartio.select_firmware = select_f2a_firmware;
	cartio.select_linker_multiboot = select_f2a_linker_multiboot;
	cartio.select_splash = select_f2a_splash;
	cartio.select_loader = select_f2a_loader;

	cartio.linker_reinit = linker_usb_reinit;
	cartio.linker_release = linker_usb_release;
	cartio.linker_connect = f2a_usb_connect;
	cartio.linker_multiboot = f2a_usb_linker_init;
	cartio.autodetect = f2a_get_type;
	cartio.user_multiboot = f2a_multiboot;
	cartio.direct_write = f2a_writemem;
	cartio.read = f2a_readmem;

	cartio.setup = 1;
	cart_reinit();
}
