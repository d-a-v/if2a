/* 
 * Based on f2a by Ulrich Hecht <uli@emulinks.de>
 * if2a by D. Gauchard <deyv@free.fr>
 * F2A Ultra support by Vincent Rubiolo <vincent.rubiolo@free.fr>
 * Licensed under the terms of the GNU Public License version 2
 */

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "libf2a.h"
#include "cartio.h"
#include "cartrom.h"
#include "cartutils.h"

// defined by binware.c
extern binware_s binware_f2a_loader_pro [];
extern binware_s binware_f2a_loader_ultra [];
extern binware_s binware_f2a_usb_firmware [];
extern binware_s binware_f2a_multiboot [];
extern binware_s binware_f2a_splash [];

binware_s loader	= { NULL, 0, NULL };
binware_s firmware	= { NULL, 0, NULL };
binware_s multiboot	= { NULL, 0, NULL };
binware_s splash	= { NULL, 0, NULL };

cartio_s cartio = { 0, };

const char* cart_type_str (cart_type_e cart_type)
{
	switch (cart_type)
	{
	case CART_TYPE_F2A_PRO:		return "F2A-Pro";
	case CART_TYPE_F2A_ULTRA:	return "F2A-Ultra";
	case CART_TYPE_TEMPLATE:	return ">template<";
	default:			return "(Not detected)";
	}
}

void cart_init (void)
{
	cart_trim_always = 0;
	cart_trim_allowed = 1;
	cart_correct_header_allowed = 1;
	cart_burn_without_comparison = 0;
	cart_io_sim = 0;
	cart_verbose = 0;

	cart_size_mbits = -1;			/* autodetect, see cart_get_type() */
	cart_write_block_size_log2 = -1;	/* autodetect, see cart_get_type() */
	cart_rom_block_size_log2 = -1;		/* autodetect, see cart_get_type() */
	cart_thorough_compare = 0;
}

void cart_reinit (void)
{
	// Re/Initialize all global library settings.
	
	check_endianness();

	if (!cartio.setup)
	{
		printerr("Internal error, cart input/output functions not initialized.\n");
		exit(1);
	}

	cartio.linker_reinit();
}

int cart_select_firmware (const char* binware_file)
{
	return cartio.select_firmware(binware_file);
}

int cart_select_linker_multiboot (const char* binware_file)
{
	return cartio.select_linker_multiboot(binware_file);
}

int cart_select_splash (const char* binware_file)
{
	return cartio.select_splash(binware_file);
}

int cart_select_loader (cart_type_e cart_type, const char* binware_file)
{
	return cartio.select_loader(cart_type, binware_file);
}

void cart_exit (int status)
{
	cartio.linker_release();
	exit(status);
}

int cart_connect (void)
{
	if (cart_io_sim > 1)
		return 0;
	return cartio.linker_connect();
}

int cart_check_or_init_linker (void)
{
	return cartio.linker_multiboot();
}

cart_type_e cart_autodetect (int* size_mbits, int* write_block_size_log2, int* rom_block_size_log2)
{
	return cartio.autodetect(size_mbits, write_block_size_log2, rom_block_size_log2);
}

int cart_user_multiboot (const char* file)
{
	return cartio.user_multiboot(file);
}

int cart_read_mem (unsigned char* data, int address, int size)
{
	return cartio.read(data, address, size);
}

int cart_direct_write (const unsigned char* data, int base, int offset, int size, int blocksize, int first_offset, int overall_size)
{
	return cartio.direct_write(data, base, offset, size, blocksize, first_offset, overall_size);
}

int cart_read_mem_to_file (const char* file, int address, int size, enum read_type_e read_type)
{
	FILE* f;
	unsigned char data[size];

	if (cart_read_mem(data, address, size) < 0)
		return -1;

	switch (read_type)
	{
	case READ_MANY: // to read several memory areas into file
		if ((f = fopen(file, "ab")) == NULL)
		{
			printerrno(file);
			return -1;
		}
		break;
		
	case READ_ONCE:
		if ((f = fopen(file, "wb")) == NULL)
		{
			printerrno(file);
			return -1;
		}
		break;
		
	default:
		printerr("This is bad. Should have reached either WRITE_MANY or WRITE_ONCE\n");
		return -1;
	}

	if (fwrite(data, size, 1, f) != 1)
	{
		printerrno(file);
		fclose(f);
		return -1;
	}
	fclose(f);
	return 0;
}

int cart_burn (int cart_base, int cart_offset, const unsigned char* rom, int rom_offset, int rom_size)
{
	int initial_rom_offset = cart_offset + rom_offset;
	
	// data flash addresses for cartio.direct_write() process must not cross MAXBURNCHUNK multiples
	int size_to_burn = rom_size;
	while (size_to_burn > 0)
	{
		// chunksize is the maximum size such that offset/MAXBURNCHUNK
		// and (offset+chunksize-1)/MAXBURNCHUNK are equals:
		int chunksize = (((cart_offset + rom_offset) + MAXBURNCHUNK) / MAXBURNCHUNK) * MAXBURNCHUNK - (cart_offset + rom_offset);
		if (chunksize > size_to_burn)
			chunksize = size_to_burn;
		assert(chunksize > 0);

		int offset_burn = cart_offset + rom_offset;
		int size_burn = chunksize;
		int offset_save = offset_burn;
		int size_save = size_burn;

		if (cart_verbose)
			print("Burning from 0x%x to 0x%x", cart_base + offset_burn, cart_base + offset_burn + size_burn);
		adjust_burn_addresses(&offset_burn, &size_burn);
		if (cart_verbose)
		{
			if ((offset_save != offset_burn) || (size_save != size_burn))
				print(" (adjusted: from 0x%x to 0x%x)\n", cart_base + offset_burn, cart_base + offset_burn + size_burn);
			else
				print("\n");
		}

		if (cartio.direct_write(&rom[rom_offset], cart_base, offset_burn, size_burn, CART_WRITE_BLOCK_SIZE, initial_rom_offset, rom_size) < 0)
			return -1;

		rom_offset += chunksize;
		size_to_burn -= chunksize;
		
		if (cart_verbose)
			print("\n");
	}
	return 0;
}

void print_array (const unsigned char* array, int size)
{
	// prints contents of an array
	
	int idx;
	for (idx = size - 1 ; idx >= 0; idx--)
		print("%c ", (unsigned char)array[idx]);
	print("\n");
}

void print_array_dual (const unsigned char* array, int size)
{
	// prints contents of an array
	
	int idx;
	int block;

	#define NBLOCKS 4
	size /= sizeof(u_int32_t);
	for (block = 0; block < size; block += NBLOCKS)
	{
		char ascii4[6];
		char ascii[128] = { 0, };
		for (idx = block; idx < block + NBLOCKS; idx++)
		{
			#define H(v) ((unsigned char)(v))
			// reads 32 bits word numbered idx and converts it to host endian format if necessary
			// (assuming gba/arm's endianness is little-endian like network's convention)
			u_int32_t v;
			if (idx < size)
				v = ntoh32(((u_int32_t*)array)[idx]);
			else
				v = 0xffffffff;
		
			// print big endian first
			print("%02x %02x %02x %02x - ", H(v), H(v>>8), H(v>>16), H(v>>24));
			sprintf(ascii4, "%c%c%c%c ", ASCII(v), ASCII(v>>8), ASCII(v>>16), ASCII(v>>24));
			strcat(ascii, ascii4);
		}
		print("%s\n", ascii);
	}
}
