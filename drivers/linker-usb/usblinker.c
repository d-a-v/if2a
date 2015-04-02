/* 
 * Based on f2a by Ulrich Hecht <uli@emulinks.de>
 * if2a by D. Gauchard <deyv@free.fr>
 * F2A Ultra support by Vincent Rubiolo <vincent.rubiolo@free.fr>
 * Firmware loading debug by Julien Janier <julien@janier.org>
 * Licensed under the terms of the GNU Public License version 2
 */

#include <string.h>
#include <assert.h>
#if !_WIN32
#include <unistd.h>
#include <sys/wait.h>
#endif

#include <usb.h>

#include "../../libf2a.h"
#include "../../cartutils.h"
#include "../linker-usb/an2131.h"
#include "usblinker.h"

int			cart_usb_timeout = USB_TIMEOUT;
static usb_dev_handle*	linker_handle = NULL;
static int		linker_usb_1_major = -1;
static int		linker_usb_1_minor = -1;
static int		linker_usb_2_major = -1;
static int		linker_usb_2_minor = -1;
static int		linker_usb_interface = -1;
static int		linker_usb_configuration = -1;
static int		linker_usb_read_endpoint = -1;
static int		linker_usb_write_endpoint = -1;

//////////////////////////////////////////////////////////////////////////////
// linux-2.4.19- specifics
#if __linux__

#include <sys/utsname.h>	// uname()
#include <sys/stat.h>		// open()
#include <fcntl.h>		// open()
#define EZUSB2131_MODULE "/proc/ezusb/dev0"

/* This function tests against the kernel version to know whether a USB reset 
 * and usb_set_configuration is needed (or unwanted).
 * If the machine runs linux-2.6, we do need to reset, if it is 2.4, we must not.
 * Also under 2.4, usb_set_configuration won't work whereas it is required
 * under win32.
 * 
 * We pass a verbosity parameter instead of relying on the global verbosity 
 * level because otherwise the kernel info would be printed each time we are 
 * called.
 */

static int linker_usb_linux24 (int verbosity)
{
	struct utsname utsname;
	int version, patchlevel;
	
	if (uname(&utsname) == -1)
	{
		printerrno("utsname");
		return 0;
	}

	version = atoi(strtok(utsname.release, "."));
	patchlevel = atoi(strtok(NULL, "."));

	if (verbosity == 1)
	{
		print("System information:\n");
		print("\tSystem name = %s\n", utsname.sysname);
		print("\tNode name = %s\n", utsname.nodename);
		print("\tRelease = %s\n", utsname.release);
		print("\tVersion = %s\n", utsname.version);
		print("\tMachine = %s\n", utsname.machine);
	}

	if (strcmp(utsname.sysname, "Linux") == 0)
	{
		if (verbosity == 1)
			print("\tRunning on Linux %i.%i\n", version, patchlevel);
		return version == 2 && patchlevel == 4;
	}

	return 0;
}

static int ezusb_load_firmware_linux24 (const char* dev, const unsigned char* data, int size)
{
	int f;
	int w, tot;
	
	/* Trying to use ezusb2131 module (linux 2.4) */
	if ((f = open(dev, O_WRONLY)) == -1)
	{
		printerrno("open(%s)", dev);
		return -1;
	}
		
	if (cart_verbose)
		print("Uploading EZ-USB firmware in %s...\n", dev);

	for (tot = 0; tot < size; tot += w)
	{
		w = write(f, data + tot, size - tot);
		if (w == -1)
		{
			printerrno("write on "EZUSB2131_MODULE);
			return -1;
		}
		
		if (cart_verbose > 1)
			print(EZUSB2131_MODULE": wrote %i bytes [%i->%i / %i]\r", w, tot, tot+w, size);
	}

	close(f);

	if (cart_verbose > 1)
		print("\n");

	return 0;
}

#else // !__linux__

#define linker_usb_linux24(x...) 0

#endif // !__linux__

//////////////////////////////////////////////////////////////////////////////
// static functions

static int linker_usb_connect_root(void)
{
    struct usb_bus *bus;
    struct usb_device *dev;
    struct usb_device *linker = NULL;

    int firmware_loaded=0;
    int init_hack = linker_usb_linux24(cart_verbose > 1);

    usb_set_debug(cart_verbose > 1);

    usb_init();
    usb_find_busses();

find_linker:

    usb_find_devices();

    // Loop on busses
    for (bus = usb_busses; !linker && bus; bus = bus->next)
    {
	if (cart_verbose)
	    print("Scanning bus '%s'\n", bus->dirname);
        
        // Loop on devices
	for (dev = bus->devices; !linker && dev; dev = dev->next)
	{
	    if (cart_verbose && (dev->descriptor.idVendor || 
                                 dev->descriptor.idProduct))
	        print("\tFound device '%s': 0x%04x/0x%04x\n", dev->filename, 
                      dev->descriptor.idVendor, dev->descriptor.idProduct);
            
            // Found ready-to-use linker
	    if ((dev->descriptor.idVendor == linker_usb_2_major) && 
                (dev->descriptor.idProduct == linker_usb_2_minor))
	    {
	        linker = dev;
		break;
	    }
            // Linker is here but needs firmware loading
	    else if ((dev->descriptor.idVendor == linker_usb_1_major) && 
                     (dev->descriptor.idProduct == linker_usb_1_minor) && 
                      !firmware_loaded)
	    {
		if (ezusb_load_firmware(dev, firmware.data, 
                    cart_usb_timeout) < 0
#if __linux__
		    && (init_hack && 
                        ezusb_load_firmware_linux24(EZUSB2131_MODULE, 
                                                    firmware.data, 
                                                    firmware.size) < 0)
#endif
			)
		{
		    return -1;
	        }
			
		firmware_loaded = 1;
                
		if (cart_verbose > 1)
		    print("EZ-USB renumerating. Please wait\n");
                
		sleep(2);	// give the EZUSB some time to renumerate
		goto find_linker;
                
	    } // firmware needs loading
	} // loop on devices
    } // loop on busses
    
    if (linker == NULL)
    {
        printerr("Couldn't find linker attached to USB.\n");
	return -1;
    }
	
    return !!(linker_handle = linker_usb_open(linker, init_hack))? 0: -1;
}

//////////////////////////////////////////////////////////////////////////////
// public functions

int linker_usb_connect (int first_stage_major, int first_stage_minor,
			int second_stage_major, int second_stage_minor,
			int usb_configuration, int usb_interface,
			int usb_read_endpoint, int usb_write_endpoint)
{
	int result;
	
	linker_usb_1_major = first_stage_major;
	linker_usb_1_minor = first_stage_minor;
	linker_usb_2_major = second_stage_major;
	linker_usb_2_minor = second_stage_minor;
	linker_usb_configuration = usb_configuration;
	linker_usb_interface = usb_interface;
	linker_usb_read_endpoint = usb_read_endpoint;
	linker_usb_write_endpoint = usb_write_endpoint;
	result = linker_usb_connect_root();

#if !_WIN32
	if (getuid() > 0)
		seteuid(getuid());
	else
	{
		uid_t uid;
		char* sudo_uid = getenv("SUDO_UID");
		if (sudo_uid && (uid = atoi(sudo_uid)) > 0)
		{
			setuid(uid);
			seteuid(uid);
		}
	}
	if (getgid() > 0)
		setegid(getgid());
	else
	{
		gid_t gid;
		char* sudo_gid = getenv("SUDO_GID");
		if (sudo_gid && (gid = atoi(sudo_gid)) > 0)
		{
			setgid(gid);
			setegid(gid);
		}
	}
#endif

	return result;
}

usb_dev_handle* linker_usb_open (struct usb_device* dev, int init_hack)
{
	int err;
	usb_dev_handle* handle;
	
#if 0
#if __linux__
	// some linux distribution enable the module usbtest in the kernel
	// which is hotplugatically loaded once ezusb2131 has renumerated.
	// this is bad. the cable becomes busy.
	// so this is a first test for friendly automagic things:
	system("(/sbin/lsmod | /bin/grep ^usbtest && /sbin/rmmod usbtest) >& /dev/null");
#endif // __linux__
#endif

	// Open the USB device and get handle
	if ((handle = usb_open(dev)) == NULL)
	{
		printerr("usb_open: %s\n", usb_strerror());
		return NULL;
	}

	// Set configuration (needed for win32, unwanted in linux-2.4, no
	// matter for linux-2.6)
	// FIXME : several breaks occurred on Vince"s machine (2.6) because
	// of that
	// (error : usb_set_configuration : operation timed out)
	if (   !init_hack
	    && (err = usb_set_configuration(handle, linker_usb_configuration)) < 0)
	{
		printerr("usb_set_configuration: %s\n", usb_strerror());
		usb_close(handle);
#if __linux__
		// too bad in libusb err==-1 so we can't check if it is
		// really a EBUSY error to display the following message:

		printerr("\nNote: under some linux distributions, hotplug is"
			 " automatically configured to\nload the 'usbtest'"
			 " kernel module if it is available. You should"
			 " type:\n\tdmesg | tail\nand check if the the module"
			 " 'usbtest' claims something. If this is\nthe"
			 " case you should try the command 'rmmod"
			 " usbtest' and restart if2a. For\nfuture use, you"
			 " can also add the keyword usbtest at the end of"
			 " the file\n/etc/hotplug/blacklist.\n\n");
#endif // __linux__
		return NULL;
	}

	// Reset if needed (yes for 2.6 kernels, no for 2.4 kernel, no
	// matter for win32?)
	// FIXME vince: if multiboot image is running and linker ready, this
	// will break things up (error: usb_reset : no such device)
	// update david (20051028): better behaviour if reset is called -
	// reactivating code (this is a test)
#if 1
	if (!init_hack && usb_reset(handle) < 0)
	{
		printerr("usb_reset: %s\n", usb_strerror());
		usb_close(handle);
		return NULL;
	}
#endif

	// claim interface
	if (usb_claim_interface(handle, linker_usb_interface) < 0)
	{
		printerr("usb_claim_interface(%i): %s\n", linker_usb_interface, usb_strerror());
		usb_close(handle);
		return NULL;
	}

	return handle;
}


void linker_usb_reinit (void)
{
	linker_usb_release();
}

void linker_usb_disconnect (void)
{
	if (usb_release_interface(linker_handle, linker_usb_interface) < 0)
		printerr("usb_release: %s\n", usb_strerror());

	if (usb_close(linker_handle) < 0)
		printerr("usb_close: %s\n", usb_strerror());
}

void linker_usb_release ()
{
	if (linker_handle)
		linker_usb_disconnect();
	linker_handle = NULL;
}

int linker_usb_read (unsigned char* buffer, int buffer_size)
{
	int result;

	result = usb_bulk_read(linker_handle, linker_usb_read_endpoint, (char*)buffer, buffer_size, cart_usb_timeout);
	if (result != buffer_size)
	{
		printerr("usb_bulk_read (request %i, got %i): %s\n", buffer_size, result, usb_strerror());
		return -1;
	}
	return result;
}

int linker_usb_write(const unsigned char* buffer, int buffer_size)
{
	int result;
	
	// this would be nice if libusb defined a const buffer
	union
	{
		const unsigned char* buffer;
		char* noconstsignedbuffer;
	} badconv;
	badconv.buffer = buffer;

	result = usb_bulk_write(linker_handle, linker_usb_write_endpoint, badconv.noconstsignedbuffer, buffer_size, cart_usb_timeout);
	if (result != buffer_size)
	{
		printerr("usb_bulk_write (request %i, got %i): %s\n", buffer_size, result, usb_strerror());
		return -1;
	}
	return result;
}

int linker_usb_write_by_block (const unsigned char* buffer, int buffer_size, int block_size)
{
	int sent, result;
	
	sent = 0;
	while (sent < buffer_size)
	{
		result = linker_usb_write(buffer + sent, buffer_size - sent > block_size? block_size: buffer_size - sent);
		if (result < 0)
			return -1;
		sent += result;
	}
	return sent;
}
