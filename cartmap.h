/* 
 * Based in f2a by Ulrich Hecht <uli@emulinks.de>
 * if2a by D. Gauchard <deyv@free.fr>
 * F2A Ultra support by Vincent Rubiolo <vincent.rubiolo@free.fr>
 * Licensed under the terms of the GNU Public License version 2
 */

//////////////////////////////////////
// cart map structures

#include "libf2a.h"

#define MAP_NAMELEN		32
#define MAP_MINIMUM_ENTRIES	2
#define MAP_MAGIC		0x1F2A0002	// IF2A-0002 map version

typedef struct
{
	char		name[MAP_NAMELEN];
	u_int32_t	offset;
	u_int32_t	size;
} __attribute__ ((packed)) cart_map_s;

typedef struct
{
	u_int32_t	magic;
	u_int32_t	location;
	u_int16_t	number_of_entries;
} __attribute__ ((packed)) cart_map_locator_s;

void	reset_cart_map				(void);

void	brand_new_empty_cart_map		(void);
int	load_cart_map				(void);
void	display_cart_map			(void);
void	cart_map_mark_for_remove		(const char* del_files[], int del_files_number);
void	cart_map_replace_loader			(binware_s* loader);
int	cart_map_build_hole			(void);
void	display_cart_map_hole			(void);
int	cart_map_find_best_insertion_for_files	(const char* add_files[], int add_files_number);
void	cart_map_file_display_best_score	(void);
int	cart_map_process_changes		(void);
