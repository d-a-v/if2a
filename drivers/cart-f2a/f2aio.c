/* 
 * Based in f2a by Ulrich Hecht <uli@emulinks.de>
 * Multiboot by Eli Curtz <eli@nuprometheus.com>
 * if2a by D. Gauchard <deyv@free.fr>
 * F2A Ultra support by Vincent Rubiolo <vincent.rubiolo@free.fr>
 * Licensed under the terms of the GNU Public License version 2
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../../libf2a.h"
#include "../../cartio.h"
#include "../../cartrom.h"
#include "../../cartutils.h"
#include "f2aio.h"

f2arecvmsg	rm;
f2asendmsg	sm;
f2a_read_f	f2a_read = NULL;
f2a_write_f	f2a_write = NULL;
static int	displayed_block_size_log2 = 32; // cosmetic needs (default: always displays 0)

cart_type_e f2a_get_type (int* size_mbits, int* write_block_size_log2, int* rom_block_size_log2)
{
	int i, ascshift;
	unsigned char buffer [SIZE_1K];
	int shift = 21;
	int txtsize = 20;
	char text[txtsize + 1];
	int b_version = 0;
	int detected_size = 0;
	char multiplier_char = 0;
	cart_type_e type = CART_TYPE_UNDEF;
	
	*write_block_size_log2 = DEFAULT_WRITEBLOCKSIZE_LOG2;
	*rom_block_size_log2 = DEFAULT_ROMBLOCKSIZE_LOG2;
	displayed_block_size_log2 = 32; // displays 0
	
	if (cart_io_sim > 1)
		return type;
	
	*size_mbits = 0;

	// read background info from OAM: text boxes are drawn using tiled mode
	sm.command=CMD_READDATA;
	sm.subcommand=SUBCMD_READDATA;
	sm.size=SIZE_1K;
	sm.magic=MAGIC_NUMBER;
	sm.address=GBA_OAM;
	sm.sizekb=sm.size/1024;

	if (f2a_write_msg(&sm) == -1)
		return type;

	if (f2a_read(buffer, SIZE_1K) == -1)
		return type;

	// Translate from sprite memory (OAM) to ASCII (hopefully this is easy)
	switch ((unsigned char)buffer[172] / 2)
	{
		case 'F':	ascshift = 0;	break;	// this is a multiboot<=v2.4u
		case 'F'-32:	ascshift = 32;	break;  // this is a multiboot>=v2.6bU
		default:
			ascshift = -1;
			text[0] = 0;
	}

	if (ascshift >= 0)
	{
		for (i = 0; i < txtsize; i++)
			text[i] = (unsigned char)buffer[(i + shift) * 8 + 4] / 2 + ascshift;
	
		// Remove trailing spaces
		for (i--; i > 0 && text[i] == ' '; i--)
			text[i] = 0;
	}
	
	if (cart_verbose)
		print("\tFound '%s' as cartridge descriptor\n", text);

	// parse descriptor
	if (strncmp(text, "F2A-", 4) == 0)
	{
		i = 3;
		while (isdigit(text[++i]));
		multiplier_char = text[i];
		text[i] = 0;
		detected_size = atoi(&text[4]);
		switch (multiplier_char)
		{
		case 'M':	*size_mbits = detected_size; break;
		case 'G':	*size_mbits = 1024 * detected_size; break;
		default:	*size_mbits = 32;
				printerr("Could not recognize cart size '%s' !\n", text);
		}
		
		if (strstr(&text[i+1], "turbo"))
			type = CART_TYPE_F2A_TURBO;
		else if (strstr(&text[i+1], "pro"))
			type = CART_TYPE_F2A_PRO;
		else if (strstr(&text[i+1], "ultra"))
			type = CART_TYPE_F2A_ULTRA;

		if ((b_version = strstr(&text[i+1], "-B") || strstr(&text[i+1], "-b"))) // which one, both ?
			displayed_block_size_log2 = 17; // 128Kbits
		else
			displayed_block_size_log2 = 18; // 256Kbits
	}
	else
	{
		displayed_block_size_log2 = 32; // displays 0
		*size_mbits = 32;
		b_version = 0;
	}

	// 17 randomly gives bad result when writing cart map on a f2a-pro-b-256mbits
	assert(*write_block_size_log2 >= 18);

	// 15=32KB, has to be 17(128KB) for Turbo carts ? need to detect them, need testers!	
	*rom_block_size_log2 = 15;

	if (ascshift > 0 && type != CART_TYPE_F2A_ULTRA && *size_mbits > 256)
	{
		if (   (*size_mbits == 512 && !b_version)
		    || (*size_mbits == 320 && b_version))
		{
			printerr("\tToo recent multiboot for your cart detected! - "
			         "Using autodetection workaround.\n");
			*size_mbits = 256;
			sprintf(text, "F2A-256M pro%s[p!]", b_version? "-B": "  ");
			for (i = 0; i < strlen(text); i++)
				buffer[(i + shift) * 8 + 4] = (text[i] - ascshift) * 2;

			// rewrite to OAM
			sm.command=CMD_WRITEDATA;
			sm.subcommand=SUBCMD_WRITEDATA;
			sm.size=SIZE_1K;
			sm.magic=MAGIC_NUMBER;
			sm.address=GBA_OAM;
			sm.sizekb=sm.size/1024;

			if (f2a_write_msg(&sm) == -1 || f2a_write(buffer, SIZE_1K) == -1)
				printerr("Could not patch screen.");
		}
		else
		{
			printerr("AARRGH - due to a too recent multiboot program, the autodetection failed.\n"
				     "         Please report your cartrige descriptor and type to if2a@ml.free.fr.\n");
			printerr("You must either\n"
				 "- specify your cart type and size, or\n"
				 "- use an older loader (multiboot-f2a-usb-v2.4b.mb might suit your cart).\n");
			*size_mbits = 32;
		}
	}

	if (cart_verbose)
		print("\tAutodetected type is '%s', size is %iMbits, write block size\n"
		      "\tis %iKbits and on-screen block size is %iKbits.\n",
			cart_type_str(type),
			*size_mbits,
			1 << (*write_block_size_log2 - 10),
			1 << (displayed_block_size_log2 - 10));
	
	return type;
}

int f2a_write_msg (f2asendmsg* original_sm)
{
	f2asendmsg sm;
	// this sm is a copy of *original_sm in
	// values are converted into usb firmware's little endian
	
	// this is important
	memset(&sm, 0, sizeof(sm));

	sm.command = tolittleendian32(original_sm->command);
	sm.size = tolittleendian32(original_sm->size);
	sm.magic = tolittleendian32(original_sm->magic);
	sm.subcommand = tolittleendian32(original_sm->subcommand);
	sm.address = tolittleendian32(original_sm->address);
	sm.sizekb = tolittleendian32(original_sm->sizekb);
	
	return f2a_write((unsigned char*)&sm, sizeof(sm));
}

int f2a_writemem (const unsigned char* rom, int base, int offset, int size, int blocksize, int first_offset, int overall_size)
{
	int i;
	f2asendmsg sm;

	if (cart_verbose)
		print("Burning: base=0x%x offset=0x%x size=0x%x\n", base, offset, size);

	// flash the ROM

	// initialize command buffer
	memset(&sm, 0, sizeof(sm));
	sm.command = CMD_WRITEDATA;
	// From sniffer under windows (pro): 6: GameSRAM a: GameROM
	// ULTRA : only 7 for write and 6 for read same as command field
	// FIXME RECHECK THIS FOR PRO
	sm.subcommand = (base >= GBA_ROM && base < GBA_SRAM)? SUBCMD_WRITEROM: SUBCMD_WRITERAM;
	sm.magic = MAGIC_NUMBER;
	sm.size = size;
	sm.address = base + offset;
	sm.sizekb = size >> 10;

	if (!cart_io_sim)
		if (f2a_write_msg(&sm) == -1)
		{
			printerr("error sending command\n");
			return -1;
		}

	for (i = 0; i < size; i += blocksize)
	{
		if (!cart_io_sim)
		{
			if (cart_verbose > 2)
				print("Writing 0x%x bytes at base 0x%x offset 0x%x\n", blocksize, base, offset + i);
			if (f2a_write(&rom[i], blocksize) == -1)
			{
 				printerr("error sending data\n");
				return -1;
			}
		}

		print("\r%s (%i%%) ",
		      sm.subcommand == SUBCMD_WRITERAM? "Wrote SRAM": "Burning...",
		      (i + offset - first_offset + blocksize) * 100 / overall_size);
		printflush();
	}
	
	return 0;
}

int f2a_readmem (unsigned char* data, int address, int size)
{
	int i;

//FIXME we should be able to read mem by chunks bigger than SIZE_1K,

	assert((size & (SIZE_1K - 1)) == 0);

	memset(&sm, 0, sizeof(sm));
	sm.command=CMD_READDATA;
	sm.magic=MAGIC_NUMBER;
	sm.subcommand=SUBCMD_READDATA;
	sm.address=address;
	sm.size=size;
	sm.sizekb=size/1024;

	if (cart_io_sim > 1)
		return -1;
		
	if (f2a_write_msg(&sm) == -1)
		return -1;
	
	for (i = 0; i < sm.size; i += SIZE_1K)
	{
		if (f2a_read(data, SIZE_1K) == -1)
			return -1;
		data += SIZE_1K;
	}
	return 0;
}

int f2a_multiboot (const char* fileName)
{
	int		fileSize;
	unsigned char*	buffer;
   
	if (cart_io_sim > 1)
		return 0;

	if ((buffer = download_from_file(fileName, &fileSize)) == NULL)
		return -1;

	if (f2a_writemem(buffer, GBA_EWRAM, 0, fileSize, fileSize, 0, fileSize) < 0)
		return -1;
	
	free(buffer);

	print("Now booting %s...\n", fileName);

	// Launch the program
	memset(&sm, 0, sizeof(sm));
	sm.command = CMD_WRITEDATA;
	sm.subcommand = SUBCMD_BOOTEWRAM;
	sm.magic = MAGIC_NUMBER;
	sm.address = GBA_EWRAM;
	   
	if (f2a_write_msg(&sm) == -1)
	{
		print("F2A Error: Unable to initiate Multiboot download\n");
		return -1;
	}
   
	// we cannot talk anymore to the linker
   	cart_exit(0);
	return 0;
} 
