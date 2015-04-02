/* 
 * Based in f2a by Ulrich Hecht <uli@emulinks.de>
 * Multiboot by Eli Curtz <eli@nuprometheus.com>
 * if2a by D. Gauchard <deyv@free.fr>
 * F2A Ultra support by Vincent Rubiolo <vincent.rubiolo@free.fr>
 * Licensed under the terms of the GNU Public License version 2
 */

#include "../../libf2a.h"
#include "../../cartio.h"

static int select_firmware (const char* binware_file)
{
	print("\t(template driver) select_firmware(%s)\n", binware_file);
	return 0;
}

static int select_linker_multiboot (const char* binware_file)
{
	print("\t(template driver) select_linker_multiboot(%s)\n", binware_file);
	return 0;
}

static int select_splash (const char* binware_file)
{
	print("\t(template driver) select_splash(%s)\n", binware_file);
	return 0;
}

static int select_loader (cart_type_e cart_type, const char* binware_file)
{
	print("\t(template driver) select_loader(%s, %s)\n", cart_type_str(cart_type), binware_file);
	return 0;
}
	
static void reinit (void)
{
	print("\t(template driver) reinit()\n");
}

static void release (void)
{
	print("\t(template driver) release()\n");
}

static int connect_ (void)
{
	print("\t(template driver) connect_()\n");
	return 0;
}

static int linker_multiboot (void)
{
	print("\t(template driver) linker_multiboot()\n");
	return 0;
}

static cart_type_e autodetect (int* size_mbits, int* write_block_size_log2, int* rom_block_size_log2)
{
	print("\t(template driver) autodetect()\n");
	*size_mbits = -1;		// cannot autodetect (?)
	*write_block_size_log2 = 18;	// this should be a working value, less to be tried
	*rom_block_size_log2 = 15;	// (=32KB) rom alignment for loader to detect them
	return CART_TYPE_TEMPLATE;
}

static int user_multiboot (const char* file)
{
	print("\t(template driver) user_multiboot(%s)\n", file);
	return 0;
}

static int direct_write (const unsigned char* data, int base, int offset, int size, int blocksize, int first_offset, int overall_size)
{
	print("\t(template driver) direct_write(data, base=0x%x, offset=0x%x, size=0x%x, blocksize=0x%x, first_offset=0x%x, overall_size=0x%x)\n", base, offset, size, blocksize, first_offset, overall_size);
	return 0;
}

static int read_ (unsigned char* data, int address, int size)
{
	print("\t(template driver) read(to_data, address=0x%x, size=0x%x)\n", address, size);
	return 0;
}

void cart_reinit_template (void)
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
