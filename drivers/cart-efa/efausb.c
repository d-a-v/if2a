
#include "../../binware.h"
#include "../../libf2a.h"
#include "../../cartio.h"
#include "../../cartutils.h"
#include "../linker-usb/usblinker.h"

static int select_firmware (const char* name)
{
	return binware_load(&firmware, binware_efa_usb_firmware, name, "EFA-usb-linker-firmware");
}

static int select_linker_multiboot (const char* binware_file)
{
	// no multiboot possible
	return -1;
}

static int select_splash (const char* binware_file)
{
	// no splash possible
	return -1;
}

static int select_loader (cart_type_e cart_type, const char* name)
{
	return binware_load(&loader, binware_efa_loader, name, "EFA-loader");
}
	
static void reinit (void)
{
	print("\t(efa driver) reinit() todo\n");
}

static void release (void)
{
	print("\t(efa driver) release() todo\n");
}

static int connect_ (void)
{
	return linker_usb_connect
	(
		0x0547, 0x2131,
		0x5094, 0x2060,
		1,	// efa linker usb configuration
		0x00,	// efa linker usb interface
		0x84,	// efa linker usb read endpoint
		0x04	// efa linker usb write endpoint
	);
}

static int linker_multiboot (void)
{
	// no further operation needed for linker
	return 0;
}

static cart_type_e autodetect (int* size_mbits, int* write_block_size_log2, int* rom_block_size_log2)
{
	print("\t(efa driver) autodetect() todo\n");
	*size_mbits = -1;		// cannot autodetect (?)
	*write_block_size_log2 = 18;	// this should be a working value, less to be tried
	*rom_block_size_log2 = 15;	// (=32KB) rom alignment for loader to detect them
	return CART_TYPE_EFA;
}

static int user_multiboot (const char* file)
{
	// cannot multiboot file
	return 0;
}

static int direct_write (const unsigned char* data, int base, int offset, int size, int blocksize, int first_offset, int overall_size)
{
	print("\t(efa driver) direct_write(data, base=0x%x, offset=0x%x, size=0x%x, blocksize=0x%x, first_offset=0x%x, overall_size=0x%x)\n", base, offset, size, blocksize, first_offset, overall_size);
	return 0;
}

static int read_ (unsigned char* data, int address, int size)
{
	print("\t(efa driver) read(to_data, address=0x%x, size=0x%x)\n", address, size);
	return 0;
}

void cart_reinit_efa (void)
{
	cartio.select_firmware = select_firmware;
	cartio.select_linker_multiboot = select_linker_multiboot;
	cartio.select_splash = select_splash;
	cartio.select_loader = select_loader;

	cartio.linker_reinit = reinit;
	cartio.linker_release = release;
	cartio.linker_connect = connect_;
	cartio.linker_multiboot = linker_multiboot;
	cartio.autodetect = autodetect;
	cartio.user_multiboot = user_multiboot;
	cartio.direct_write = direct_write;
	cartio.read = read_;

	cartio.setup = 1;
	cart_reinit();
}
