/* 
 * Based on f2a by Ulrich Hecht <uli@emulinks.de>
 * if2a by D. Gauchard <deyv@free.fr>
 * F2A Ultra support by Vincent Rubiolo <vincent.rubiolo@free.fr>
 * Licensed under the terms of the GNU Public License version 2
 */

#ifndef __CARTIO_H__
#define __CARTIO_H__

//////////////////////////////////////
// generic I/O operations structure

typedef struct
{

	int		setup;	/* 0=false otherwise true */

	int		(*select_firmware)		(const char* binware_file);
	int		(*select_linker_multiboot)	(const char* binware_file);
	int		(*select_splash)		(const char* binware_file);
	int		(*select_loader)		(cart_type_e cart_type, const char* binware_file);
	
	void		(*linker_reinit)		(void);
	void		(*linker_release)		(void);
	int		(*linker_connect)		(void);
	int		(*linker_multiboot)		(void);
	cart_type_e	(*autodetect)			(int* size_mbits, int* write_block_size_log2, int* rom_block_size_log2);
	int		(*user_multiboot)		(const char* file);
	int		(*direct_write)			(const unsigned char* data, int base, int offset, int size, int blocksize, int first_offset, int overall_size);
	int		(*read)				(unsigned char* data, int address, int size);

} cartio_s;

extern cartio_s cartio;

void	cart_init		(void);
void	cart_reinit		(void);
void	print_array		(const unsigned char* array, int size);
void	print_array_dual	(const unsigned char* array, int size);

#endif // __CARTIO_H__
