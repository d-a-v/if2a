
#include "../../binware.h"
#include "../../libf2a.h"
#include "../../cartio.h"
#include "../../cartutils.h"
#include "../linker-usb/usblinker.h"

#include "f2amisc.h"

static int select_firmware (const char* name)
{
	return binware_load(&firmware, binware_f2a_usb_writer_firmware, name, "F2A-USB-Writer-firmware");
}

static int select_linker_multiboot (const char* binware_file)
{
	// no multiboot possible
	return 0;
}

static int select_splash (const char* binware_file)
{
	// no splash possible
	return -1;
}

static int f2a_usb_writer_connect (void)
{
	return linker_usb_connect
	(
		0x0547, 0x2131,
		0x0547, 0x1002,
		1,	// f2a writer usb configuration
		0x00,	// f2a writer usb interface
		0x83,	// f2a writer usb read endpoint
		0x04	// f2a writer usb write endpoint
	);
}

static int linker_multiboot (void)
{
	// no further operation needed for the f2a usb writer
	return 0;
}

static cart_type_e autodetect (int* size_mbits, int* write_block_size_log2, int* rom_block_size_log2)
{
	*size_mbits = -1;		// cannot autodetect (?)
	*write_block_size_log2 = 18;	// this should be a working value, less to be tried
	*rom_block_size_log2 = 15;	// (=32KB) rom alignment for loader to detect them
	return CART_TYPE_UNDEF;		
}

static int user_multiboot (const char* file)
{
	// cannot multiboot file
	return 0;
}

static int direct_write (const unsigned char* data, int base, int offset, int size, int blocksize, int first_offset, int overall_size)
{
	print("\t(f2a usb writer driver) direct_write(data, base=0x%x, offset=0x%x, size=0x%x, blocksize=0x%x, first_offset=0x%x, overall_size=0x%x)\n", base, offset, size, blocksize, first_offset, overall_size);
	return 0;
}

static int read_ (unsigned char* data, int address, int size)
{
	print("\t(f2a usb writer driver) read(to_data, address=0x%x, size=0x%x)\n", address, size);
	return 0;
}

void cart_reinit_f2a_usb_writer (void)
{
	cartio.select_firmware = select_firmware;
	cartio.select_linker_multiboot = select_linker_multiboot;
	cartio.select_splash = select_splash;
	cartio.select_loader = select_f2a_loader;

	cartio.linker_reinit = linker_usb_reinit;
	cartio.linker_release = linker_usb_release;
	cartio.linker_connect = f2a_usb_writer_connect;
	cartio.linker_multiboot = linker_multiboot;
	cartio.autodetect = autodetect;
	cartio.user_multiboot = user_multiboot;
	cartio.direct_write = direct_write;
	cartio.read = read_;

	cartio.setup = 1;
	cart_reinit();
}
