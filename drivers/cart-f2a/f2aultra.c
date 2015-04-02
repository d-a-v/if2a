/* 
 * Based in f2a by Ulrich Hecht <uli@emulinks.de>
 * if2a by D. Gauchard <deyv@free.fr>
 * F2A Ultra support by Vincent Rubiolo <vincent.rubiolo@free.fr>
 * Licensed under the terms of the GNU Public License version 2
 */

// F2A Ultra-specific functions

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "../../libf2a.h"
#include "../../cartutils.h"
#include "../../cartrom.h"
#include "../../cartio.h"
#include "f2aultra.h"
#include "f2aio.h"

static void content_descriptor_convertion_from_cart (content_desc* d)
{
	if (!is_littleendian_host())
	{
		int i;
		
		#define C32(x) { d->x = swap32(d->x); }
	
		C32(lp_list_id);
		C32(lp_game_id);
		C32(lp_idx);
		C32(fixed3);
		C32(fixed4);
		//XXXFIXME -	should we do a loop to convert unknowns, _fixed and _padding?
		//		this might be necessary once we try to write the CD back to cart
	
		for (i = 0; i < (sizeof(d->roms) / sizeof(rom_entry)); i++)
		{
			C32(roms[i].list_id);
			C32(roms[i].game_id);
			C32(roms[i].rom_size);
			C32(roms[i].list_crc);
			//XXXFIXME - same notice
		}
		
		#undef C32
	}
	
#if 0
	assert(d->fixed3 == 0x8000021e && d->fixed4 == 0x01000000);
#else
	if (!(d->fixed3 == 0x8000021e && d->fixed4 == 0x01000000))
		printerr("problem in content descriptor\n");
#endif
}

#if 0 // uncomment when needed
static void content_descriptor_convertion_to_cart (content_desc* desc)
{
	// this is the same function
	content_descriptor_convertion_from_cart(desc);
}
#endif

int f2au_SVD_to_file(const char* filename)
{
	const int BLOCK_SIZE = 131072;
	
	// 3 reads are necessary to get all SVD data (see PROTOCOL)
	
	if (cart_read_mem_to_file(filename, F2AU_SVD_BASE, BLOCK_SIZE, READ_ONCE)== -1)
		return -1;
		
	if (cart_read_mem_to_file(filename, F2AU_SVD_BASE + BLOCK_SIZE, BLOCK_SIZE, READ_MANY) == -1)
		return -1;

	if (cart_read_mem_to_file(filename, F2AU_SVD_BASE + 2*BLOCK_SIZE, 65536, READ_MANY) == -1)
		return -1;

	return 0;
}

int f2au_SVD_from_file(const char* file)
{
	const int SVD_SIZE = 327680;
	int block_size = 131072;
	int nb_blocks = 3;
	int i = 0;
	FILE* f;
	struct stat st;
	unsigned char* block = NULL;
	int offset;
		
	if (stat(file, &st)==-1)
	{
		printerrno(file);
		return -1;
	}

	// Some checks
	if (st.st_size > SVD_SIZE)
	{
		printerr("File too big (0x%x bytes) to fit in 0x%x bytes\n",
			(int)st.st_size, SVD_SIZE);
		return -1;
	}

	if (cart_verbose)
		print("Loading file: %s (size=0x%x)\n", file, (int)st.st_size);

	if ((f = fopen(file, "rb")) == NULL)
		printerrno("fopen(%s)", file);

	// Split SVD file into blocks written to GBA at different addresses
	for (i=1; i <= nb_blocks; i ++)
	{	
		// Last block has different size
		if (i==nb_blocks) block_size = 65536;

		// Allocate memory
		if ((block = (unsigned char*)malloc(block_size)) == NULL)
		{
			printerr("Cannot allocate 0x%x bytes - ", block_size);
			printerrno("malloc");
			return -1;
		}
		memset(block, 0xff, block_size);
		
		// Load block into "allocated rom"
		if (fread(block, block_size, 1, f) != 1)
		{
			if (cart_verbose > 1)
				printerr("while reading 0x%x bytes: ", block_size);
			if (ferror(f))
				printerrno("fread(%s)", file);
			return -1;
		}
		if (cart_direct_write(block, GBA_SRAM, i*0x20000, block_size, SIZE_1K, 0x20000, 131072 * nb_blocks - 65536) < 0)
			return -1;
		free(block);

		// Select a new block
	      	if (fseek(f, i*block_size, SEEK_CUR) == -1 || (offset = ftell(f)) == -1)
		{
			printerr("Cannot advance to next block\n");
			printerrno("fseek/ftell");
			return -1;
		}
	}
	fclose(f);
	return 0;
}

int f2au_DH_to_file(const char* file)
{
	const int BLOCK_SIZE = 131072;
	int cpt;
	
	// 5 reads are necessary to get all DH data (see PROTOCOL)
	
	if (cart_read_mem_to_file(file, F2AU_DH_BASE, BLOCK_SIZE, READ_ONCE) == -1)
		return -1;

	for (cpt=1; cpt <= 3; cpt++)
	{
		if (cart_read_mem_to_file(file, F2AU_DH_BASE + cpt*BLOCK_SIZE, BLOCK_SIZE, READ_MANY) == -1)
			return -1;
	}

	if (cart_read_mem_to_file(file, F2AU_DH_BASE + 4*BLOCK_SIZE, 3072, READ_MANY) == -1)
		return -1;
	
	return 0;
}

int f2au_DH_from_file(const char* file)
{
	return 0;
}

int f2au_CD_to_file(const char* file)
{
	return cart_read_mem_to_file(file, F2AU_CD_BASE, 65536, READ_ONCE);
}

int f2au_GameID_gen(const char* filename, int* game_id)
{
	/* 
	 * GameIDs are computed by a CRC32 on (first 180 bytes + 189th byte)
	 * of the ROM
	 */
#if 0
	char buffer[181];
	FILE* f;
	
	memset(buffer, 0, sizeof(buffer));

	// Flawed since we need in fact 189 bytes
	if (buffer_from_file(filename, buffer, 180) < 0)
		return -1;

	// Need to reopen to get 189th byte
	if ((f = fopen(filename, "rb")) == NULL)
	{
		printerrno("fopen(%s)", filename);
		return -1;
	}

      	if (fseek(f, 188, SEEK_SET) == -1)
      	{
      		printerrno("fseek");
      		return -1;
      	}
	
	if (fread(buffer+180, 1, 1, f) != 1)
	{
		if (ferror(f))
			printerrno("failed to read from file %s", filename);
		return -1;
	}
	fclose(f);

	cart_crc32(buffer, game_id, sizeof(buffer));
#else
	unsigned char buffer[189];
	if (buffer_from_file(filename, buffer, 189) < 0)
		return -1;
	buffer[180] = buffer[188];
	cart_crc32(buffer, game_id, 181);
#endif
	
	return 0;
}


int f2au_CD_print(content_desc* desc)
{
	// We assume here that CD has been previously validated
	short idx = 0; // no more than 100 rom entries

	print("Content descriptor characteristics:\n");
	print("-----------------------------------\n");
	print("Last played ROM release ID:	%08u\n", desc->lp_list_id);
	print("Last played ROM GameID:		%08x\n", desc->lp_game_id);
	print("Last played ROM CIZ index:	%08u\n", desc->lp_idx);
	print("Unknown (CD CRC?):		%08x\n", desc->unknown);
	print("Fixed3:				%08x\n", desc->fixed3);
	print("Fixed4:				%08x\n", desc->fixed4);
	print("Nb of visible ROMs in CIZ:	%08x\n\n", desc->nb_visible);
	
	print("ROM entries are:\n");
	print("----------------\n");
	for (; desc->roms[idx].game_id != 0; idx ++)
	{
		print("Name : %s\n", desc->roms[idx].rom_name);
		print("\tList ID :	%08u\n", desc->roms[idx].list_id);
		print("\tGame ID :	%08x\n", desc->roms[idx].game_id);
		print("\tUnknown1 :	"); print_array(desc->roms[idx].unknown1, -4);
		print("\tROM size :	%08x\n", desc->roms[idx].rom_size);
		print("\tUnknown2 :	"); print_array(desc->roms[idx].unknown2, -2);
		print("\tCIZ index :	%02x\n", desc->roms[idx].ciz_idx);
		print("\tUnknown3 :	%08x\n", desc->roms[idx].unknown3);
		print("\tList CRC :	%08x\n", desc->roms[idx].list_crc);
		print("\tROM lg :	"); print_array(desc->roms[idx].rom_lg, -2);
		print("\tROM saver :	"); print_array(desc->roms[idx].rom_saver, -2);
		print("\tUnknown5:	");
		print_array(desc->roms[idx].unknown5, -6);
	}
	return 0;

}

int f2au_CD_check(content_desc* desc)
{
	return 0;
}

content_desc* f2au_CD_read (void)
{
	static content_desc desc;
	if (cart_read_mem((unsigned char*) &desc, F2AU_CD_BASE, SIZE_64K) == -1)
	{
		printerr("Cannot read Content descriptor\n");
		return NULL;
	}
	content_descriptor_convertion_from_cart(&desc);
	return &desc;
}

int f2au_loadandwrite_sram (const char* file, int offset, int size)
{
	int ret;
	unsigned char* sram;
	
	if (size != SIZE_64K)
	{
		printerr("Could not write %i bytes from file %s to SRAM, %i bytes expected\n", size, file, SIZE_64K);
		return -1;
	}
	if ((sram = prepare_loadandwrite_sram (file, offset, size)) == NULL)
		return -1;
	ret = cart_direct_write(sram, F2AU_CD_BASE, 0, SIZE_64K, SIZE_1K, 0, SIZE_64K);
	free(sram);
	return ret;
}
