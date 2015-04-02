/* 
 * Based on f2a by Ulrich Hecht <uli@emulinks.de>
 * if2a by D. Gauchard <deyv@free.fr>
 * F2A Ultra support by Vincent Rubiolo <vincent.rubiolo@free.fr>
 * Licensed under the terms of the GNU Public License version 2
 */
 
// Cart/ROM functions

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>

#include "libf2a.h"
#include "cartrom.h"
#include "cartmap.h"
#include "cartutils.h"
#include "binware.h"

// cart map generation by auto_loadandburn_rom() activation:
// cartmap is self-sufficient and no more needs to be generated here. code
// is now disabled by the following define, and will be removed in the
// future if everyone's happy with it. - david 20051005
#define ENABLE_MAP		0	// cartmap generation enable(1) or disable(0)

#define GBA_HEADNAME		"GBAROM-"
#define GBA_EXTNAME		".gba"

int cart_trim_always = 0;
int cart_trim_allowed = 1;
int cart_size_mbits = -1;		/* autodetect, see cart_get_type() */
int cart_write_block_size_log2 = -1;	/* autodetect, see cart_get_type() */
int cart_rom_block_size_log2 = -1;	/* autodetect, see cart_get_type() */
int cart_thorough_compare = 0;
int cart_correct_header_allowed = 1;
int cart_burn_without_comparison = 0;

/*
 * Precomputed CRC32-Values for 00..255
 *
 * c(x) = 1+x+x^2+x^4+x^5+x^7+x^8+x^10+x^11+x^12+x^16+x^22+x^23+x^26+x^32
 */
static const unsigned int CRC32_TAB[256] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
    0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
    0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
    0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
    0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
    0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
    0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
    0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
    0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
    0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
    0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
    0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
    0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
    0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
    0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
    0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
    0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
    0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
    0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
    0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
    0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
    0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
    0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
    0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
    0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
    0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d };
    
#define CRC32_UPDATE(crc, nxt) (crc = CRC32_TAB[(crc^(nxt))&0xff]^(crc >> 8))

/*
 * Computes a 32 bit CRC value with the polynom
 * c(x) = 1+x+x^2+x^4+x^5+x^7+x^8+x^10+x^11+x^12+x^16+x^22+x^23+x^26+x^32
 * on a given string.
 * We are initializing with 0xff's and not inverting the result
 */
int cart_crc32(const unsigned char *str, int *crc32buf, int size) 
{
	unsigned int temp = 0xffffffff;
	int idx ;
	const unsigned char *ptr = str;

	if (str == NULL)
		return -1;

	for (idx = 0; idx < size; idx++)
	{
		CRC32_UPDATE(temp, *ptr++);
	}

	*crc32buf = temp;
	
	return 0;
}

// burned addresses have to be on CART_WRITE_BLOCK_SIZE multiples boundaries
void adjust_burn_addresses (int* offset, int* size)
{
	// adjust offset so that it's a multiple of block
	// adjust size so that it's a multiple of block
	*size += *offset;
	*offset &= ~(CART_WRITE_BLOCK_SIZE - 1);
	*size -= *offset;
	if (*size & (CART_WRITE_BLOCK_SIZE - 1))
		*size += CART_WRITE_BLOCK_SIZE;
	*size &= ~(CART_WRITE_BLOCK_SIZE - 1);
}

// burned rom offset and offset+size have to be on CART_ROM_BLOCK_SIZE multiples boundaries
void adjust_rom_addresses (int* offset, int* size)
{
	// adjust offset so that it's a multiple of block
	// adjust size so that it's a multiple of block
	// (this is needed if we want to rewrite a rom between two others)
	*size += *offset;
	*offset &= ~(CART_ROM_BLOCK_SIZE - 1);
	*size -= *offset;
	if (*size & (CART_ROM_BLOCK_SIZE - 1))
		*size += CART_ROM_BLOCK_SIZE;
	*size &= ~(CART_ROM_BLOCK_SIZE - 1);
}

// load addresses have to be on CART_ROM_BLOCK_SIZE multiples boundaries
void adjust_load_addresses (int* offset, int* size)
{
	// adjust offset so that it's a multiple of block
	// adjust size so that it's a multiple of block
	*size += *offset;
	*offset &= ~(SIZE_1K - 1);
	*size -= *offset;
	if (*size & (SIZE_1K - 1))
		*size += SIZE_1K;
	*size &= ~(SIZE_1K - 1);
}

void adjust_write_size (int* size)
{
	if (*size & (CART_WRITE_BLOCK_SIZE - 1))
		*size += CART_WRITE_BLOCK_SIZE;
	*size &= ~(CART_WRITE_BLOCK_SIZE - 1);
}

void adjust_rom_size (int* size)
{
	if (*size & (CART_ROM_BLOCK_SIZE - 1))
		*size += CART_ROM_BLOCK_SIZE;
	*size &= ~(CART_ROM_BLOCK_SIZE - 1);
}

int trim (const unsigned char* rom, int size)
{
	unsigned char last;
	int trimmed_size;
	
	if (cart_trim_allowed)
	{
		// trim to get interesting size
		last = rom[size - 1];
		for (trimmed_size = size - 2; trimmed_size > 0 && rom[trimmed_size] == last; trimmed_size--);
		trimmed_size += 2;
	}
	else
		trimmed_size = size;
	
	return trimmed_size;
}

// struct got from devkitpro project (devkitpro.org),
// subproject buildscripts, file tools/gba/gbafix.c
typedef struct
{
	unsigned char	start_code1;			// B instruction
	unsigned char	start_code2;			// B instruction
	unsigned char	start_code3;			// B instruction
	unsigned char	start_code4;			// B instruction
	unsigned char	logo[0xA0-0x04];		// logo data
	char		title[0xC];			// game title name
	unsigned long	game_code;			//
	unsigned char	maker_code1;			//
	unsigned char	maker_code2;			//
	unsigned char	fixed;				// 0x96
	unsigned char	unit_code;			// 0x00
	unsigned char	device_type;			// 0x80
	unsigned char	unused[7];			//
	unsigned char	game_version;			// 0x00
	unsigned char	complement;			// 800000A0..800000BC
	u_int16_t	checksum;			// 0x0000
} rom_header_s;

// data got from devkitpro project (devkitpro.org),
// subproject buildscripts, file tools/gba/gbafix.c
static const rom_header_s good_header =
{
	// start_code
	0x2e, 0x00, 0x00, 0xea,
	// logo
	{ 0x24,0xFF,0xAE,0x51,0x69,0x9A,0xA2,0x21,0x3D,0x84,0x82,0x0A,0x84,0xE4,0x09,0xAD,
	0x11,0x24,0x8B,0x98,0xC0,0x81,0x7F,0x21,0xA3,0x52,0xBE,0x19,0x93,0x09,0xCE,0x20,
	0x10,0x46,0x4A,0x4A,0xF8,0x27,0x31,0xEC,0x58,0xC7,0xE8,0x33,0x82,0xE3,0xCE,0xBF,
	0x85,0xF4,0xDF,0x94,0xCE,0x4B,0x09,0xC1,0x94,0x56,0x8A,0xC0,0x13,0x72,0xA7,0xFC,
	0x9F,0x84,0x4D,0x73,0xA3,0xCA,0x9A,0x61,0x58,0x97,0xA3,0x27,0xFC,0x03,0x98,0x76,
	0x23,0x1D,0xC7,0x61,0x03,0x04,0xAE,0x56,0xBF,0x38,0x84,0x00,0x40,0xA7,0x0E,0xFD,
	0xFF,0x52,0xFE,0x03,0x6F,0x95,0x30,0xF1,0x97,0xFB,0xC0,0x85,0x60,0xD6,0x80,0x25,
	0xA9,0x63,0xBE,0x03,0x01,0x4E,0x38,0xE2,0xF9,0xA2,0x34,0xFF,0xBB,0x3E,0x03,0x44,
	0x78,0x00,0x90,0xCB,0x88,0x11,0x3A,0x94,0x65,0xC0,0x7C,0x63,0x87,0xF0,0x3C,0xAF,
	0xD6,0x25,0xE4,0x8B,0x38,0x0A,0xAC,0x72,0x21,0xD4,0xF8,0x07 } ,
	// title
	{ 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 },
	// game code
	0x00000000,
	// maker code
	0x30, 0x31,
	// fixed
	0x96,
	// unit_code
	0x00,
	// device type
	0x80,
	// unused
	{ 0x00,0x00,0x00,0x00,0x00,0x00,0x00 },
	// game version
	0x00,
	// complement
	0x00,
	// checksum
	0x0000
};

// code got from devkitpro project (devkitpro.org),
// subproject buildscripts, file tools/gba/gbafix.c
static unsigned char rom_header_complement (const unsigned char* rom)
{
	int n;
	char c = 0;
	const unsigned char* p = rom + 0xA0;
	for (n=0; n<0xBD-0xA0; n++)
		c += *p++;
	return (unsigned)(-(0x19+c));
}

int has_good_header_for_reading (const unsigned char* rom)
{
	return ((rom_header_s*)rom)->complement == rom_header_complement(rom)? 1: -1;
}

#if 0 // not used anymore
int has_good_header_for_burning (const unsigned char* rom)
{
	return    has_good_header_for_reading(rom) == 1
	       && (rom[3] & 0xfe) == 0xea	// from pogoshell
	       && rom[0xb2] == 0x96		// from pogoshell
	       ? 1: -1;
}
#endif

void correct_header (unsigned char* rom, const char* name, int force_name)
{
	// this code seems to be now working.
	// it might be cleaner if accurately using the rom_header_s structure.
	
	if (cart_verbose > 1)
		print("(Updating header)\n");

	char romname[13];
	char romcode[5];
	const char* shortname;
	
	// replace everything before title (jump instruction + logo)
	memcpy(rom, &good_header, 0xa0);

	memset(romname, 32, 12);
	memset(romcode, 32, 4);
	romname[12] = romcode[4] = 0;

	shortname = filename2romname(name);
	memcpy(romname, shortname, MIN(strlen(shortname) + 1, 12));
	memcpy(romcode, shortname, MIN(strlen(shortname), 4));
	
	if (force_name || !isalnum(rom[0xa0]))
		memcpy(&rom[0xa0], romname, 12);
	if (!isalnum(rom[0xac]))
		memcpy(&rom[0xac], romcode, 4);
	
	// needed (from pogoshell)
	rom[3] = (rom[3] & ~0xfe) | 0xea;
	rom[0xb2] = 0x96;
	
	((rom_header_s*)rom)->complement = rom_header_complement(rom);
}

void display_map (const unsigned char* rom)
{
	int i;
	
	print("Game Title:	");
	for (i = 0xa0; i < 0xac; i++)
		print("%c", ASCII(rom[i]));

	print("\nGame Code:	AGB-");
	for (i = 0xac; i < 0xb0; i++)
		print("%c", ASCII(rom[i]));
    
	print("\nMaker Code:	");
	for (i = 0xb0; i < 0xb2; i++)
		print("%c", ASCII(rom[i]));
    
	print("\n");
	//print("\n0x96:		0x%02x\n", rom[0xb2]);
}

const char* romname (const unsigned char* rom)
{
	int len;
	static char name [13];
	
	for (len = 0; len < 12; len++)
		name[len] = ASCII((char)rom[0xa0 + len]);
	for (len--; len>=0 && (name[len]==32 || name[len]=='.'); len--);
		name[len+1] = 0;
	return name;
}

// Remove directory part and extension, limit to 12 chars
const char* filename2romname (const char* filename)
{
	int i, len;
	static char name [13];
	
	len = 0;
	for (i = strlen(filename) - 1; i >= 0 && filename[i] != '/' && filename[i] != '\\'; i--)
		if (filename[i] == '.')
			len = 0;
		else
			len++;
	if (len > 12)
		len = 12;
	strncpy(name, &filename[i + 1], 12);
	name[len] = 0;

	return name;
}

int display_scanned_cart_map (void)
{
	int i;
	unsigned char rom[SIZE_1K];
	
	if (cart_io_sim > 1)
	return 0;

	for (i = GBA_ROM; i < GBA_ROM + CART_SIZE_BYTES; i += CART_ROM_BLOCK_SIZE)
	{
		print("Trying address 0x%x...\r", i);
		if (cart_read_mem(rom, i, SIZE_1K) == -1)
			return -1;
		if (has_good_header_for_reading(rom) > -1)
		{
			print("\n");
			display_map(rom);
		}
	}
	print("\n");

	return 0;
}

void auto_readandsave_rom (int numfiles, char* files[])
{
	int i;
	int indexfile;
	int nextblockloaded;
	const char* name;
	char autoname[64];
	unsigned char rom[CART_ROM_BLOCK_SIZE];
	
	assert(CART_ROM_BLOCK_SIZE >= SIZE_1K);
	
	if (cart_io_sim > 1)
		return;

	indexfile = 0;
	i = GBA_ROM;
	if (cart_read_mem(rom, i, SIZE_1K) < 0)
		return;
	do
	{
		nextblockloaded = 0;
		
		print("Trying address 0x%x...\r", i);
		if (has_good_header_for_reading(rom) != -1)
		{
			/* Prepare filename */
			name = romname(rom);
			strcpy(autoname, GBA_HEADNAME);
			strcat(autoname, name);
			strcat(autoname, GBA_EXTNAME);
			
			print("\n");
			print("Detected rom '%s'\n", name);
			
			/* If no arg is given, autoname; otherwise, use specified name */
			if (indexfile >= numfiles)
				name = autoname;
			else
				name = files[indexfile];
			indexfile++;
			
			if (strcmp(name, "@") == 0)
				name = autoname;
			
			if (strcmp(name, ".") == 0) /* user don't want this rom */
			{
				if (cart_verbose)
					print("Skipping on user request\n");
			}
			else
			{
				FILE* f;
				if (cart_verbose)
					print("Using name file name '%s'\n", name);
				if ((f = fopen(name, "wb")) == NULL)
					printerrno("fopen(%s)", name);
				else
				{
					if (cart_read_mem(rom, i, CART_ROM_BLOCK_SIZE) < 0)
						return;
					do /* read data up to next ROM/valid header */
					{
						if (fwrite(rom, CART_ROM_BLOCK_SIZE, 1, f) != 1)
						{
							if (ferror(f))
								printerrno("write(%s)", name);
							else
								printerr("could not write to file %s\n", name);
							break;
						}
						if ((i+=CART_ROM_BLOCK_SIZE) < GBA_ROM + CART_SIZE_BYTES && cart_read_mem(rom, i, CART_ROM_BLOCK_SIZE) < 0)
							return;
					} while (i < GBA_ROM + CART_SIZE_BYTES && has_good_header_for_reading(rom) == -1);
					fclose(f);
					nextblockloaded = 1;
				}
			}
		}
		
		if (!nextblockloaded) // continue to look for ROM/valid header
		{
			if ((i += CART_ROM_BLOCK_SIZE) < GBA_ROM + CART_SIZE_BYTES && cart_read_mem(rom, i, SIZE_1K) < 0)
				return;
		}
	} while (i < GBA_ROM + CART_SIZE_BYTES);
	
	print("\n");
}

void display_memory_map (const unsigned char* rom, int size)
{
    int i;

    for (i = 0; i < size; i += CART_ROM_BLOCK_SIZE)
    {
	print("Trying address 0x%x...\r", i);
	if (has_good_header_for_reading(&rom[i]) > -1)
	{
	    print("\n");
	    display_map(&rom[i]);
	}
    }
}

int is_same (int offset, int size, const unsigned char* mem)
{
    int ret, i;
    unsigned char rom [SIZE_1K];
	
    if (cart_io_sim > 1)
	return -1;

    for (i = offset; i < offset + size; i += SIZE_1K)
    {
        // check beginning
        cart_read_mem(rom, GBA_ROM + i, SIZE_1K);
        ret = memcmp(rom, &mem[i], SIZE_1K);
        if (cart_verbose > 1 && ret != 0)
            print("(offset 0x%x-0x%x BAD)\n", i, i + SIZE_1K - 1);
        if (ret != 0)
            return -1;
    }
    return 1;
}

int has_same_data (int offset, int size, const unsigned char* mem)
{
	if (cart_thorough_compare)
	{
		int i;
		for (i = offset; i < offset + size; i += CART_ROM_BLOCK_SIZE)
			if (is_same(i, CART_ROM_BLOCK_SIZE, mem) == -1)
				return -1;
	}
	else
	{
		// check beginnning
		if (is_same(offset, SIZE_1K, mem) == -1)
			return -1;

		// roughly check end
		size &= ~(SIZE_1K-1);
		if (is_same(offset + size - SIZE_1K, SIZE_1K, mem) == -1)
			return -1;
	}
	return 1;
}

int convsize (const char* size)
{
	int digitsnum;
	const char* unit;
        
    // Parse numbers and stop at letters
	for (digitsnum = 0; isdigit(size[digitsnum]); digitsnum++);
        
    // Grab unit : Bytes or bits, kilos or megas
    // FIXME : kilos do not work
	unit = &size[digitsnum];
        
	if (strlen(unit) == 2)
	{
        int byte;
		switch (tolower(unit[0]))
		{
		    case 'k':	byte = 1<<10; break;
		    case 'm':	byte = 1<<20; break;
		    default:	byte = -8;
		}
		switch (unit[1])
		{
		    case 'b':	byte >>= 3; break;
		    case 'B':	break;
		    default:	byte = -1;
		}
		if (byte > 0)
		{
			char digits[digitsnum + 1];
			strncpy(digits, size, digitsnum);
			digits[digitsnum] = 0;
			return atoi(digits) * byte;
		}
	}
	return -1;
}

//////////////////////////////////////////////////////////////////////////
// check whole size

#define STAT							\
		if (index == -1)				\
			st.st_size = loader.size;		\
		else if (stat(files[index], &st) == -1)		\
		{						\
			perror(files[index]);			\
			return -1;				\
		} 

int get_wholesize (int cart_use_loader, int numfiles, char* files[])
{
	int index;
	struct stat st;
	int wholesize = 0;
	int roundedfilesize;
	
	// stat every file
	for (index = cart_use_loader? -1: 0; index < numfiles; index++)
	{
		if (cart_verbose)
			print("Stat file: %s\n", index==-1? "Loader": files[index]);

		STAT

		roundedfilesize = st.st_size;
		adjust_rom_size(&roundedfilesize); // round on CART_ROM_BLOCK_SIZE
		wholesize += roundedfilesize;
	}
	
#if 0
	if (wholesize > CART_SIZE_BYTES + loader.size)
	{
		printerr("You have not enough memory !\n");
		cart_exit(1);
	}
#else
	if (wholesize > CART_SIZE_BYTES)
		// We hope that we will be able to reduce overall content by loader.size
		wholesize = CART_SIZE_BYTES;
#endif
	
	adjust_write_size(&wholesize); // round on CART_WRITE_BLOCK_SIZE
	return wholesize;
}

//////////////////////////////////////////////////////////////////////////
// this one is simple, it writes one file to sram

unsigned char* prepare_loadandwrite_sram (const char* file, int offset, int size)
{
	FILE* sramf;
	struct stat st;
	unsigned char* sram = NULL;
	
	//XXXFIXME use buffer_from_file() here
	
	if (stat(file, &st) == -1)
	{
		perror(file);
		return NULL;
	}

	// some checks
	if (st.st_size > size)
	{
		printerr("File too big (0x%x bytes) to fit in 0x%x bytes\n",
			(int)st.st_size, size);
		return NULL;
	}

	// allocate memory
	if ((sram = (unsigned char*)malloc(size)) == NULL)
	{
		printerr("Cannot allocate 0x%x/%i bytes for sram management - ", size, size);
		perror("malloc");
		return NULL;
	}
	memset(sram, 0xff, size);

	// load files into "allocated rom"
	if (cart_verbose)
		print("Loading file: %s (size=0x%x)\n", file, (int)st.st_size);

	if ((sramf = fopen(file, "rb")) == NULL)
	{
		printerrno("fopen(%s)", file);
		free(sram);
		return NULL;
	}
	if (fread(sram, size, 1, sramf) != 1)
	{
		if (ferror(sramf))
			printerrno("fread(%s)", file);
		else
			printerr("failed to read from file %s", file);
		free(sram);
		return NULL;
	}
	fclose(sramf);
	
	return sram;
}

int cart_file2sram (const char* file, int offset, int size)
{
	int ret;
	unsigned char* sram;
	
	if ((sram = prepare_loadandwrite_sram(file, offset, size)) == NULL)
		return -1;
	ret = cart_direct_write(sram, GBA_SRAM, offset, size, SIZE_1K, offset, size);
	print("\n");
	free(sram);
	return ret;
}

//////////////////////////////////////////////////////////////////////////
// automatic check cart and burn if necessary

int auto_loadandburn_rom (cart_type_e cart_type, int cart_use_loader, int clean_cart, int numfiles, char* files[])
{
	typedef struct
	{
		int	offset;
		int	size;
	} chunk_s;

	int			burnstart		= -1;	// start address of current chunk
	int			wholesize		= get_wholesize(cart_use_loader, numfiles, files);
#if ENABLE_MAP // code to remove - cartmap is self-sufficient
	cart_map_locator_s*	locator			= NULL;
	cart_map_s*		cart_map		= NULL;
	int			cart_map_entries	= 0;
#endif // ENABLE_MAP
	int			chunks_number		= numfiles + 1;
	int			chunks_used		= 0;
	int			whole_loader_size	= 0;
	int			reduce			= 0;

	int			index;
	FILE*			ROMf;
	struct stat		st;
	int			loadedsize;
	int			roundedfilesize;
	unsigned char*		image;				// image that will contain all cart contents
	chunk_s			chunks [chunks_number];		// chunks to burn array descriptor
	
	if (wholesize <= 0)
		return -1;
	
	// allocate image memory
	if ((image = (unsigned char*)malloc(wholesize)) == NULL)
	{
		printerrno("Cannot allocate 0x%x/%i bytes for cart management - malloc", wholesize, wholesize);
		return -1;
	}
	memset(image, 0xff, wholesize);
	
	// load
	loadedsize = 0;
	for (index = (cart_use_loader && (loader.size > 0)? -1: 0); index < numfiles; index++)
	{
		print("\n");
		
		STAT
		
		if (index == -1) // manage loader, fit cart map
		{
			int i, real_loader_size;
			unsigned char last = loader.data[loader.size - 1];

			assert(loader.size > 0);

			roundedfilesize = loader.size;
			adjust_rom_size(&roundedfilesize);
			// copy f2aloader into image
			memcpy(image, loader.data, loader.size);
			// pad chunk with last loader byte
			memset(image + loader.size, last, roundedfilesize - loader.size);

			// try to find a location for the cart map
			for (i = roundedfilesize - 2; image[i] == last; i--);
			real_loader_size = i + 2;
			assert(real_loader_size <= loader.size);
			
#if ENABLE_MAP // code to remove - cartmap is self-sufficient
			// check whether cart map can fit 
			do
			{
				int spare_size = roundedfilesize - real_loader_size;
				cart_map_entries = spare_size > sizeof(cart_map_locator_s)? (spare_size - sizeof(cart_map_locator_s)) / sizeof(cart_map_s): 0;
				if (cart_verbose)
					print("Spare size for cart map at rounded loader's end is %i bytes, which gives room for %i roms.\n", spare_size, cart_map_entries);
				if (cart_map_entries >= MAP_MINIMUM_ENTRIES)
				{
					int cart_map_locator = roundedfilesize - sizeof(cart_map_locator_s);
					locator = (cart_map_locator_s*)&image[cart_map_locator];
					locator->location = hton32(roundedfilesize - sizeof(cart_map_locator_s) - cart_map_entries * sizeof(cart_map_s));
					locator->number_of_entries = hton16(cart_map_entries);
					locator->magic = hton32(MAP_MAGIC);
					cart_map = (cart_map_s*)&image[ntoh32(locator->location)];
					if (cart_verbose)
						print("Rom map table is at address 0x%x\nRom map locator is at address 0x%x (end of loader)\n",
							GBA_ROM + ntoh32(locator->location),
							GBA_ROM + cart_map_locator);
				}
				else
				{
					int oldfilesize = roundedfilesize;
					roundedfilesize++;
					adjust_rom_size(&roundedfilesize);
					// pad chunk with last loader byte
					memset(image + oldfilesize, last, roundedfilesize - oldfilesize);
					print("Increasing loader size to fit cart map by 0x%x/%i bytes\n", roundedfilesize - oldfilesize, roundedfilesize - oldfilesize);
				}
			} while (cart_map_entries < MAP_MINIMUM_ENTRIES);
			if (cart_verbose)
				print("\n");
#endif // ENABLE_MAP // code to remove // roundedfilesize is unchanged by cartmap operations above unless not enough room for cartmap

			whole_loader_size = roundedfilesize;
			print("Loader %s is:\n", loader.name);
		}
		else
		{
			print("File %s at address 0x%x is:\n", files[index], GBA_ROM + loadedsize);

			roundedfilesize = st.st_size;
			adjust_rom_size(&roundedfilesize);
			
			if (loadedsize + st.st_size > CART_SIZE_BYTES)
			{
				printerr("! WARNING ! Cart size exceeded, truncating file (size 0x%x -> 0x%x bytes) !\n", (int)st.st_size, CART_SIZE_BYTES - loadedsize);
				if ((st.st_size = CART_SIZE_BYTES - loadedsize) == 0)
					break;
				roundedfilesize = st.st_size;
				adjust_rom_size(&roundedfilesize);
			}

			// load ROM into image file
			if ((ROMf = fopen(files[index], "rb")) == NULL)
			{
				if (ferror(ROMf))
					printerrno("fopen(%s)", files[index]);
				else
					printerr("Could not open file %s for reading\n", files[index]);
				return -1;
			}
			if (fread(&image[loadedsize], st.st_size, 1, ROMf) != 1)
			{
				if (ferror(ROMf))
					printerrno("fread(%s)", files[index]);
				else
					printerr("Could not read file %s\n", files[index]);
				return -1;
			}
			fclose(ROMf);
			
			// try to reduce rom size
			if (cart_trim_allowed || cart_trim_always)
			{
				int i, min_limit;
				// last bytes are probably padding
				char last = image[loadedsize + st.st_size - 1];

				// cart_trim_always tries to mad-pad everything
				// !cart_trim_always tries to fit loader only
				min_limit = cart_trim_always? 0: MAX(0, (st.st_size - whole_loader_size - 1));
				// truncate as long as there is padding
				for (i = st.st_size - 2; i >= min_limit && image[loadedsize + i] == last; i--);
				roundedfilesize = i + 2;
				adjust_rom_size(&roundedfilesize);

				if (roundedfilesize < st.st_size)
				{
					print("(reducing ROM size from %ikB to %ikB", 
						((int)st.st_size + 1023) >> 10, 
						(roundedfilesize + 1023) >> 10);
					if (   (reduce += ((int)st.st_size - roundedfilesize)) > whole_loader_size
					    && !cart_trim_always)
					{
						print(" - not trying to further reduce ROM size");
						cart_trim_allowed = 0;
					}
					print(")\n");
				}
			}
		}

		// check file against real ROM
		if (cart_correct_header_allowed && index != -1)
			correct_header(&image[loadedsize], files[index], 0);
		display_map(&image[loadedsize]);
		
#if ENABLE_MAP // code to remove - cartmap is self-sufficient
		// fill the cart locator
		if (locator)
		{
			if (index >= cart_map_entries)
			{
				printerr("Warning: not enough room in cart locator to stock further rom entries !\n");
				locator = NULL;
			}
			else
			{
				strncpy(cart_map[index].name, romname(&image[loadedsize]), MAP_NAMELEN);
				cart_map[index].offset = hton32(loadedsize);
				cart_map[index].size = hton32(roundedfilesize);
			}
		}
#endif // ENABLE_MAP
		
		if (!cart_burn_without_comparison && has_same_data(loadedsize, roundedfilesize, image) >= 0)
		{
			print("No need to burn it!\n");
			// but it's time to burn the previous ones if they changed
			if (burnstart >= 0)
			{
				//XXXFIXME remove all this commentary when ACKed
				//	we don't burn anymore while loading, but record the location
				//	to burn into the chunks array because the map locator
				//	needs to be burned after all files are loaded, although it is
				//	located itself in the beginning of the cart
				//if (f2a_burn(GBA_ROM, image, burnstart, loadedsize - burnstart) < 0)
				//	return -1;
				
				// register chunk
				assert(chunks_used < chunks_number);
				chunks[chunks_used].offset = burnstart;
				chunks[chunks_used].size = loadedsize - burnstart;
				chunks_used++;
				
				// next chunk will be a new separate one
				burnstart = -1;
			}
		}
		else
		{
			print("Not the same !\n");
			// and we remember start address
			if (burnstart < 0)
				burnstart = loadedsize;
		}

		loadedsize += roundedfilesize;
	}

#if ENABLE_MAP // code to remove - cartmap is self-sufficient
	// mark end of map
	if (locator && index < cart_map_entries)
	{
		cart_map[index].name[0] = 0;
		cart_map[index].offset = hton32(0);
		cart_map[index].size = hton32(0);
	}
#endif // ENABLE_MAP

	// register the last chunk
	if (burnstart >= 0)
	{
		assert(chunks_used < chunks_number);
		chunks[chunks_used].offset = burnstart;
		chunks[chunks_used].size = loadedsize - burnstart;
		chunks_used++;
	}
	
	print("\n");
	// now burn all the recorded chunks
	for (index = 0; index < chunks_used; index++)
		if (cart_burn(GBA_ROM, 0, image, chunks[index].offset, chunks[index].size) < 0)
			return -1;
	print("\n");

	/* 
	 * Temporary write support for Ultra carts : we wipe out the descriptor and rely on
	 * CIZ to regenerate it
	 */
	if (cart_type == CART_TYPE_F2A_ULTRA)
	{
		content_desc desc;
		print("Wiping out content descriptor. You will need to launch CIZ to regenerate it.\n");
		memset((char*) &desc, 0x99, sizeof(desc));
		cart_direct_write((unsigned char*) &desc, F2AU_CD_BASE, 0, SIZE_64K, SIZE_1K, 0, SIZE_64K);
		print("\n");
	}
	
	print("Remaining size: %iMbits = %iMBytes = %iKBytes\n",
		(CART_SIZE_BYTES - loadedsize) / 1024 / 1024 * 8,
		(CART_SIZE_BYTES - loadedsize) / 1024 / 1024,
		(CART_SIZE_BYTES - loadedsize) / 1024);

	if (clean_cart)
	{
		//int cleanblocksize = CART_WRITE_BLOCK_SIZE; // too big, unnecessary
		//int cleanblocksize = HEADERSIZE_B; // don't work (?)
		int cleanblocksize = CART_ROM_BLOCK_SIZE; // 32Kb

		// we write cleanblocksize bytes every CART_WRITE_BLOCK_SIZE, I
		// suppose that each write cleans CART_WRITE_BLOCK_SIZE
		// (>=cleanblocksize) bytes. to be verified...
		unsigned char empty[cleanblocksize];

		int dummy = 0;
		print("\nCleaning remaining ROM...\n");
		memset(empty, 0xff, cleanblocksize);
		burnstart = loadedsize + CART_WRITE_BLOCK_SIZE - 1;
		adjust_burn_addresses(&burnstart, &dummy);
		for (; burnstart < CART_SIZE_BYTES; burnstart += CART_WRITE_BLOCK_SIZE)
		{
			print("Cleaning from 0x%x to 0x%x\n", GBA_ROM + burnstart, 
				GBA_ROM + burnstart + cleanblocksize);
			cart_direct_write(empty, GBA_ROM, burnstart, cleanblocksize, cleanblocksize, burnstart, cleanblocksize);
			//break; no break: need to clear everything left
		}
		print("\n");
	}
	
	free(image);
	return 0;
}
