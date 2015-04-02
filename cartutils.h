/* 
 * Based in f2a by Ulrich Hecht <uli@emulinks.de>
 * if2a by D. Gauchard <deyv@free.fr>
 * F2A Ultra support by Vincent Rubiolo <vincent.rubiolo@free.fr>
 * Licensed under the terms of the GNU Public License version 2
 */


#ifndef __CARTUTILS_H__
#define __CARTUTILS_H__

#include "libf2a.h"

int		binware_load		(binware_s* dst, const binware_s binware[], const char* file, const char* name);
unsigned char*	load_from_file		(const char* filename, unsigned char* user_buffer, int* size);
void		check_endianness	(void);
u_int16_t	ntoh16			(u_int16_t x);
u_int16_t	hton16			(u_int16_t x);
u_int32_t	ntoh32			(u_int32_t x);
u_int32_t	hton32			(u_int32_t x);
u_int16_t	tolittleendian16	(u_int16_t x);
u_int32_t	tolittleendian32	(u_int32_t x);
u_int32_t	swap32			(u_int32_t x);
int		is_littleendian_host	(void);		// 0 if false (= big endian)

#endif // __CARTUTILS_H__
