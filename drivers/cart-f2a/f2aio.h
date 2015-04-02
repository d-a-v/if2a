/* 
 * Based in f2a by Ulrich Hecht <uli@emulinks.de>
 * Multiboot by Eli Curtz <eli@nuprometheus.com>
 * if2a by D. Gauchard <deyv@free.fr>
 * F2A Ultra support by Vincent Rubiolo <vincent.rubiolo@free.fr>
 * Licensed under the terms of the GNU Public License version 2
 */

#ifndef __F2AIO_H__
#define __F2AIO_H__

#include "../../libf2a.h"

#define MAGIC_NUMBER		0xa46e5b91	// needs to be properly set for almost all F2A commands

/* default values are needed in case autodection is not used,
 * 18(256KB) is the most compatible value
 */
#define DEFAULT_ROMBLOCKSIZE_LOG2	17
#define DEFAULT_WRITEBLOCKSIZE_LOG2	18

#define CMD_GETINF		0x05		// get info on the system status
#define CMD_WRITEDATA		0x06		// write data to RAM/ROM/SRAM
#define CMD_READDATA		0x07		// read data from RAM/ROM/SRAM
#define CMD_MULTIBOOT1		0xff		// boot up the GBA stage 1, no parameters
#define CMD_MULTIBOOT2		0x00		// boot up the GBA stage 2, f2asendmsg->size has to be set

#define SUBCMD_WRITEDATA	CMD_WRITEDATA
#define SUBCMD_READDATA		CMD_READDATA
#define SUBCMD_WRITERAM		0x06
#define SUBCMD_WRITEROM		0x0a
#define SUBCMD_BOOTEWRAM	0x08

typedef struct
{
	u_int32_t	command;		// command to execute, see below
	u_int32_t	size;			// size of data block to read/write
	u_int32_t	_reserved1[2];
	u_int32_t	magic;			// magic number, see below
	u_int32_t	_reserved2[3];
	u_int32_t	subcommand;		// subcommand to execute
	u_int32_t	address;		// base address for read/write
	u_int32_t	sizekb;			// size of data block to 
						// read/write in kB
	/* 
	 * for some reason the original software uses a 63 bytes structure for 
	 * outgoing messages, not 64 as it does for incoming messages, hence 
	 * the "-1". It all seems to work fine with 64 bytes, too, and I therefore 
	 * suspect this to be a bug in the original SW.
	 */
	unsigned char	_reserved3[5*4-1]; 
} __attribute__ ((packed)) f2asendmsg;

typedef struct
{
	unsigned char data[64];
} f2arecvmsg;

typedef int (*f2a_read_f)	(unsigned char* data, int size);
typedef int (*f2a_write_f)	(const unsigned char* data, int size);

extern f2a_read_f	f2a_read;
extern f2a_write_f	f2a_write;
extern f2arecvmsg	rm;
extern f2asendmsg	sm;

cart_type_e	f2a_get_type 		(int* size_mbits, int* cart_write_block_size_log2, int* cart_rom_block_size_log2);
int		f2a_write_msg		(f2asendmsg* original_sm);
int		f2a_writemem		(const unsigned char* rom, int base, int offset, int size, int blocksize, int first_offset, int overall_size);
int		f2a_readmem		(unsigned char* data, int address, int size);
int		f2a_readmemtofile	(char* file, int address, int size, enum read_type_e read_type);
int		f2a_multiboot 		(const char* fileName);

#endif // __F2AIO_H__
