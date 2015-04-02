/* 
 * Based on f2a by Ulrich Hecht <uli@emulinks.de>
 * if2a by D. Gauchard <deyv@free.fr>
 * F2A Ultra support by Vincent Rubiolo <vincent.rubiolo@free.fr>
 * Licensed under the terms of the GNU Public License version 2
 */

#ifndef __LIBF2A_H__
#define __LIBF2A_H__

#define VERSION			"0.94.4"

#if _WIN32
	#include <windows.h>
	#define sleep(n)	Sleep(1000*(n))
	#define msleep(n)	Sleep(n)
	typedef unsigned	short u_int16_t;	// ANSI-C does not define u_int16_t
	typedef unsigned	int u_int32_t;		// ANSI-C does not define u_int32_t
	typedef unsigned	long long u_int64_t;	// ANSI-C does not define u_int64_t
#else
	#include <sys/types.h>				// u_intNN_t
	#define msleep(n)	usleep(1000*(n))
#endif

#define FILE_NUMBER_MAX		32

//////////////////////////////////////
// general defines

#ifndef MAX
#define MAX(x,y)		((x)>(y)? (x): (y))
#endif

#ifndef MIN
#define MIN(x,y)		((x)<(y)? (x): (y))
#endif

#define HEADERSIZE_B		0x9C			// 156 bytes
#define SIZE_1K			1024			// minimum read data size on cart - used for SRAM operations
#define SIZE_64K		65536
#define MAXBURNCHUNK		(8 << 20)		// maximum burning size at once in bytes - 8MB (do not raise!)

enum read_type_e
{
	READ_ONCE,
	READ_MANY,					// several GBA memory areas into a file
};

//////////////////////////////////////
// GBA defines

#define GBA_EWRAM		0x02000000		// ram address for multiboot files
#define GBA_VRAM		0x06000000		// Video RAM for loader background image
#define GBA_OAM			0x07000000		// OAM (Object Attribute Memory?)
#define	GBA_ROM			0x08000000
#define GBA_SRAM		0x0e000000		// cart SRAM

#define F2AU_SVD_BASE		(GBA_SRAM + 0x020000)	// f2au
#define F2AU_CD_BASE		(GBA_SRAM + 0x0f0000)	// f2au content descriptor
#define	F2AU_DH_BASE		(GBA_SRAM + 0x370000)	// f2au

//////////////////////////////////////
// cart types

typedef enum
{
	CART_TYPE_F2A_TURBO,
	CART_TYPE_F2A_PRO,
	CART_TYPE_F2A_ULTRA,
	CART_TYPE_UNDEF,
	CART_TYPE_TEMPLATE,
	CART_TYPE_EFA,
//	CART_TYPE_XG2,
//	CART_TYPE_EZF3,
} cart_type_e;

const char* cart_type_str (cart_type_e cart_type);

//////////////////////////////////////
// binwares

#ifndef __BINWARE_STRUCT__
#define __BINWARE_STRUCT__

typedef struct
{
	const char*		name;
	int			size;
	const unsigned char*	data;
} binware_s;

#endif // __BINWARE_STRUCT__

extern binware_s	loader;				// ROM loader binaries
extern binware_s	firmware;			// Linker firmware binaries
extern binware_s	multiboot;			// F2A multiboot binaries
extern binware_s	splash;				// F2A splash binaries

//////////////////////////////////////
// cart global control variables

extern int	cart_rom_block_size_log2;		// log2(CART_ROM_BLOCK_SIZE) - roms have to have a size multiple of CART_ROM_BLOCK_SIZE
extern int	cart_write_block_size_log2;		// log2(CART_WRITE_BLOCK_SIZE) - minimum block size to write on cart
extern int	cart_size_mbits; 			// cart size in Mbits. Now set by f2a_get_type()
extern int	cart_io_sim;				// 0: normal, 1: read, no write, >1: no read, no write
extern int	cart_verbose;				// 0, 1, 2...

// wrappers
#define		CART_SIZE_BYTES		((cart_size_mbits) * 1024 * 1024 / 8)
#define		CART_ROM_BLOCK_SIZE	(1<<cart_rom_block_size_log2)
#define		CART_WRITE_BLOCK_SIZE	(1<<cart_write_block_size_log2)

// booleans
extern int	cart_thorough_compare;
extern int	cart_correct_header_allowed;
extern int	cart_burn_without_comparison;
extern int	cart_trim_always;
extern int	cart_trim_allowed;

//////////////////////////////////////
// cart I/O operations

// initialize global vars
void		cart_init				(void);

// drivers' entry point
void		cart_reinit_template			(void);
void		cart_reinit_f2a_usb			(void);
void		cart_reinit_f2a_usb_writer		(void);
void		cart_reinit_f2a_parallel		(void);
void		cart_reinit_efa				(void);

// generic calls
int		cart_select_firmware			(const char* binware_file);
int		cart_select_linker_multiboot		(const char* binware_file);
int		cart_select_splash			(const char* binware_file);
int		cart_select_loader			(cart_type_e cart_type, const char* binware_file);
int		cart_connect				(void);
void		cart_exit				(int status);
cart_type_e	cart_autodetect				(int* size_mbits, int* write_block_size_log2, int* rom_block_size_log2);
int		cart_check_or_init_linker		(void);
int		cart_user_multiboot			(const char* file);
int		cart_direct_write			(const unsigned char* data, int base, int offset, int size, int blocksize, int first_offset, int overall_size);
int		cart_read_mem				(unsigned char* data, int address, int size);
int		cart_read_mem_to_file			(const char* file, int address, int size, enum read_type_e read_type);
int		cart_burn				(int cart_base, int cart_offset, const unsigned char* rom, int rom_offset, int rom_size);

//////////////////////////////////////
// cartrom functions

int		convsize				(const char* size);
int		display_scanned_cart_map		(void);
int		cart_file2sram				(const char* file, int offset, int size);
int		auto_loadandburn_rom			(cart_type_e cart_type, int cart_use_loader, int clean_cart, int numfiles, char* files[]);
void		auto_readandsave_rom			(int numfiles, char* files[]);

// f2a specifics
int		conv_f2apro_bank			(const char* block, int* offset, int* size);

//////////////////////////////////////
// cartmap functions

void		brand_new_empty_cart_map		(void);
int		load_cart_map				(void);
void		display_cart_map			(void);
void		reset_cart_map				(void);
void		cart_map_mark_for_remove		(const char* del_files[], int del_files_number);
void		cart_map_replace_loader			(binware_s* loader);
int		cart_map_build_hole			(void);
void		display_cart_map_hole			(void);
int		cart_map_find_best_insertion_for_files	(const char* add_files[], int add_files_number);
void		cart_map_file_display_best_score	(void);
int		cart_map_process_changes		(void);

//////////////////////////////////////
// cartutils functions

int		buffer_from_file			(const char* filename, unsigned char* buffer, int size_to_check);
int		buffer_to_file				(const char* filename, const unsigned char* buffer, int size_to_write);
unsigned char*	download_from_file			(const char* filename, int* size);

//////////////////////////////////////
// print functions called by libf2a
// * print, printerr and printerrno have exactly the same syntax as printf()
//   print is used for regular message
//   printerr for error message
//   printerrno for error messages including errno value
//   (ex: perror("text") becomes printerrno("text\n"), printerrno("open(%s)\n", filename) allowed)
// * useful: print & printerr may be redefined so to catch the text and do whatever needed with it,
//           printerrno() calls printerr()
// * default: print calls printf, printerr calls fprintf(stderr)

typedef void (*print_f)		(const char*, ...);
typedef void (*printflush_f)	(void);

extern print_f		print;
extern printflush_f	printflush;
extern print_f		printerr;
extern printflush_f	printerrflush;
void			printerrno (const char* format, ...);

//////////////////////////////////////
// f2a ultra specific definitions
// borrowed from f2aultra.h

#ifndef __F2A_ULTRA_TYPES
#define __F2A_ULTRA_TYPES

// Each ROM has such an entry in the content descriptor
typedef struct rom_entry_s
{
	u_int32_t	list_id;		// ROM nb in the release lists (shows up in CIZ)
	u_int32_t	game_id;		// GameID displayed by CIZ. CRC-based
	unsigned char	rom_name[12];		// ASCII ROM name
	unsigned char	unknown1[4];
	u_int32_t	rom_size;		// ROM size in bytes
	u_int32_t	_padding1[2];
	unsigned char	unknown2[2];
	unsigned char	ciz_idx;		// index in CIZ list. Index starts at 0
	unsigned char	unknown3;
	u_int32_t	list_crc;		// ROM CRC from release lists
	unsigned char	rom_lg[2];		// ROM language (shows up in CIZ)
	unsigned char	rom_saver[2];		// ROM saver type (shows up in CIZ)
	unsigned char	unknown5[6];
	unsigned char	_padding2[10];
	
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
	unsigned char	fixed5;			// zeroes ?
	unsigned char	nb_visible;		// number of visible roms in CIZ menu
	unsigned char	fixed6[214];
	rom_entry	roms[100];		// ROM entries (see above)
	u_int32_t	_padding5[13696];

} __attribute__ ((packed)) content_desc;

#endif // __F2A_ULTRA_TYPES

int		f2au_SVD_to_file        (const char* filename);
int		f2au_SVD_from_file      (const char* file);
int		f2au_DH_to_file         (const char* file);
int		f2au_DH_from_file       (const char* file);
content_desc*	f2au_CD_read		(void);
int		f2au_CD_check           (content_desc* desc);
int		f2au_CD_print           (content_desc* descriptor);
int		f2au_GameID_gen         (const char* filename, int* game_id);
int		f2au_loadandwrite_sram	(const char* file, int offset, int size);

#endif // __LIBF2A_H__
