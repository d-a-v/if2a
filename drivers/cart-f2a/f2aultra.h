/* 
 * Based on f2aby Ulrich Hecht <uli@emulinks.de>
 * if2a by D. Gauchard <deyv@free.fr>
 * F2A Ultra support by Vincent Rubiolo <vincent.rubiolo@free.fr>
 * Licensed under the terms of the GNU Public License version 2
 */
 
// F2A Ultra-specific functions

#ifndef __F2AULTRA_H__
#define __F2AULTRA_H__

#include "../../libf2a.h"

#ifndef __F2A_ULTRA_TYPES
#define __F2A_ULTRA_TYPES

// Each ROM has such an entry in the content descriptor
typedef struct rom_entry_s
{
	u_int32_t	list_id;		// ROM nb in the release lists (shows up in CIZ)
	u_int32_t	game_id;		// GameID displayed by CIZ. CRC-based
	char		rom_name[12];		// ASCII ROM name
	char		unknown1[4];
	u_int32_t	rom_size;		// ROM size in bytes
	u_int32_t	_padding1[2];
	char		unknown2[2];
	char		ciz_idx;		// index in CIZ list. Index starts at 0
	char		unknown3;
	u_int32_t	list_crc;		// ROM CRC from release lists
	char		rom_lg[2];		// ROM language (shows up in CIZ)
	char		rom_saver[2];		// ROM saver type (shows up in CIZ)
	char		unknown5[6];
	char		_padding2[10];
	
} __attribute__ ((packed)) rom_entry;

// Defines cart contents - see PROTOCOL for more details
typedef struct content_desc_s
{
	u_int32_t	_fixed1[896];		// zeroes ?
	u_int32_t	lp_list_id;		// last played ROM ID from release lists
	u_int32_t	lp_game_id;		// last played ROM GameID
	u_int32_t	lp_idx;			// last played ROM index in CIZ menu
	u_int32_t	_fixed2[125];		// zeroes ?
	u_int32_t	unknown;		// probably CRC on descriptor
	u_int32_t	_padding1[5];
	u_int32_t	fixed3;			// 0x8000021e
	u_int32_t	fixed4;			// 0x01000000
	u_int32_t	_padding2[2];
	char		fixed5;			// zeroes ?
	char		nb_visible;		// number of visible roms in CIZ menu
	char		fixed6[214];
	rom_entry	roms[100];		// ROM entries (see above)
	u_int32_t	_padding5[13696];

} __attribute__ ((packed)) content_desc;

#endif // __F2A_ULTRA_TYPES
#if 0
int		f2au_SVD_to_file	(const char* filename);
int		f2au_SVD_from_file	(const char* file);
int		f2au_DH_to_file		(const char* file);
int		f2au_DH_from_file	(const char* file);
int		f2au_CD_to_file		(const char* file);
int		f2au_CD_print		(const content_desc* descriptor);
int		f2au_CD_check		(const content_desc* desc);
content_desc*	f2au_CD_read		(void);
int		f2au_GameID_gen		(const char* filename, int* game_id);
int		f2au_loadandwrite_sram	(const char* file, int offset, int size);
#endif
#endif // __F2AULTRA_H__
