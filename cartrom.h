/* 
 * Based in f2a by Ulrich Hecht <uli@emulinks.de>
 * if2a by D. Gauchard <deyv@free.fr>
 * F2A Ultra support by Vincent Rubiolo <vincent.rubiolo@free.fr>
 * Licensed under the terms of the GNU Public License version 2
 */


#ifndef __CARTROM_H__
#define __CARTROM_H__

#define ASCII(x)	_ASCII((unsigned char)(x))
#define _ASCII(x)	(((x) >= 32 && isascii(x))? (x): '.')

int		cart_crc32			(const unsigned char *str, int *crc32buf, int size);
int		trim				(const unsigned char* rom, int size);
const char*	romname				(const unsigned char* rom);
const char*	filename2romname 		(const char* filename);
void		correct_header			(unsigned char* rom, const char* name, int force_name);
void		adjust_rom_size			(int* size);
void		adjust_burn_addresses		(int* offset, int* size);
void		adjust_load_addresses		(int* offset, int* size);
unsigned char*	prepare_loadandwrite_sram	(const char* file, int offset, int size);

#endif // __CARTROM_H__
