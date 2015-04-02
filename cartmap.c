/* 
 * Based in f2a by Ulrich Hecht <uli@emulinks.de>
 * if2a by D. Gauchard <deyv@free.fr>
 * F2A Ultra support by Vincent Rubiolo <vincent.rubiolo@free.fr>
 * Licensed under the terms of the GNU Public License version 2
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "libf2a.h"
#include "cartmap.h"
#include "cartrom.h"
#include "cartutils.h"

/*///////////////////////////////////////////////////////////////////////////

This is complex to understand because it is huge, so here are some
informations in order to make things a bit more clear:

The goal is to manage the cart map, in which it is possible to do in one go:
- add new roms
- remove roms
- leave the others

In order to do so, a cart map (variables cart_map*) is maintained into the
flashcart just next to the loader. The cart map is created when the loader
is burned (option -L in if2a, function auto_loadandburn_rom()).

When an operation is made (add or remove in map), the cart map is loaded
first by the function load_cart_map().
The goal is to insert new roms in the best places so to maximize the biggest
empty place (called holes).
In order to do so, what is then done is:
- find holes (variables cart_map_hole*) (including the removed roms)
- find the best places for new roms (variables cart_map_file*)
- build an action map (variables cart_map_file* too) this will reflects the
  exact rom map before burning (whole space: kept+added rom and holes).  the
  final and burnt new map is built against this action map by removing holes
  from list.
- build the new map to burn and replace the old one (variables cart_map_new*)
- and finally burn everything, chunk by chunk.
  a chunk is a group of contiguous roms (including map) to burn.

Some notes:

1) A placement is a set of positions of added roms in holes. A good
placement is a placement that allows to later put new big roms. To find out
the best placement, one try to get the best score after having placed all
added roms. After having placed all roms in a particular placement #p, there
might be some remaining holes, whose sizes are called holesize(i,j) for each
hole numbered #i for the placement #j. The placement score is then
calculated this way:
	score of placement #j = sum(holesize(i,j)^2), i in 1..nbholes(j)
All possible placements are tried and the one with the best score is
retained. That's the way it works now, please tell me if you find out a
better way. (maybe a rom size sorting when more than one rom is added
at the same time?)
The best placement is processed by cart_map_find_best_insertion_for_files().

2) To burn a rom in the cart, one has to respect the cart's constraints
which are the minimum size to burn (CART_WRITE_BLOCK_SIZE). The rom offset in
cart as to be a multiple of CART_ROM_BLOCK_SIZE so that the loader (proloader
or pogoshell) might be able to recognise them. They indeed scan the entire
cart to find out what are the available roms (pogoshell has its own
filesystem but is also able to use roms out from the filesystem). So when a
chunk has to be burned, it never has the good size or position: the
borders of the chunk have to be read from the cart to get a good size and
position to burn. That's the burn_map_chunk()'s job. It is called by
cart_map_process_changes().

3) When a rom is removed, it is only removed from the map. Its header is not
wiped out from the cart. It is not a problem. If the header is still valid,
then the whole rom is still valid. If later something is burned over, the
header will be affected first and this rom won't show up anymore in the gba
loader.

///////////////////////////////////////////////////////////////////////////*/

//////////////
// inside cart
unsigned char*	cart_around_map = NULL;
int		cart_around_map_offset = 0;
int		cart_around_map_size = 0;
int		cart_map_location = 0;
int		loader_and_cart_map_size = 0;

///////////
// cart map
int		cart_map_max_number = 0;
int		cart_map_number = 0;
cart_map_s*	cart_map = NULL;

/////////////
// new loader
binware_s*	new_loader = NULL;
int		new_loader_and_cart_map_size = 0;
int		new_loader_trimmed_size = 0;

//////////////////
// hole management
cart_map_s*	cart_map_hole = NULL;
int		cart_map_hole_number = -1;

////////////////////////////////////
// new burned cart map after changes

cart_map_s* cart_map_new = NULL;
int cart_map_new_number = 0;
int cart_map_new_max_number = 0;
int cart_map_new_location = 0;

///////////////////////////////////
// rom insertion in cart management

typedef enum
{
	MAP_ACTION_DONTOUCH,	// this rom already exists, and stays alive
	MAP_ACTION_REMOVE,	// this rom's header has to be blown up
	MAP_ACTION_ADD,		// this rom is new, must be burnt
} map_action_e;

typedef struct
{
	// section 1
	// always filled
	const char* filename;		// readable file name
	char romname [MAP_NAMELEN];	// rom name (add, del & keep funcs)
	char* userromname;		// rom name overwrite if not NULL (add func)
	int original_size;		// file original size
	int size;			// after trim and expand to CART_ROM_BLOCK_SIZE
	
	// section 2 (needs section 1 to be built)
	// used to find best placement
	int hole_index;
	int hole_index_test;		// temporary
	
	// section 3 (needs section 1&2 to be built)
	// used to rebuild cart
	int		offset;
	map_action_e	action;
} cart_map_file_s;

cart_map_file_s		cart_map_file[FILE_NUMBER_MAX];
int			cart_map_file_number = 0;
static int*		cart_map_hole_remaining_size = NULL;

cart_map_file_s*	change_map_file = NULL;
int			change_map_file_number = 0;

static u_int64_t	cart_map_insertion_best_score = 0;

static int		something_to_be_done = 0;

///////////////////////////////////////

void reset_cart_map (void)
{
	// map
	if (cart_around_map)
		free(cart_around_map);
	cart_around_map = NULL;
	cart_map = NULL;
	cart_map_max_number = 0;
	cart_map_number = 0;
	cart_around_map_offset = 0;
	cart_around_map_size = 0;
	cart_map_location = 0;
	loader_and_cart_map_size = 0;
	
	// new loader
	new_loader = NULL;
	new_loader_and_cart_map_size = 0;
	new_loader_trimmed_size = 0;
	
	// hole
	if (cart_map_hole)
		free(cart_map_hole);
	cart_map_hole = NULL;
	cart_map_hole_number = 0;
	
	// insertion
	
	if (cart_map_hole_remaining_size)
		free(cart_map_hole_remaining_size);
	cart_map_hole_remaining_size = NULL;
	cart_map_hole_number = 0;
	cart_map_file_number = 0;
	cart_map_insertion_best_score = 0;
	
	if (change_map_file)
		free(change_map_file);
	change_map_file = NULL;
	change_map_file_number = 0;
	
	if (cart_map_new)
		free(cart_map_new);
	cart_map_new = NULL;
	cart_map_new_number = 0;
	cart_map_new_max_number = 0;
	cart_map_new_location = 0;
}

///////////////////////
// cart map management

void convert_cart_map_endian_from_cart_to_host (cart_map_s* cart_map, int cart_map_max_number)
{
	int i;
	for (i = 0; i < cart_map_max_number; i++)
	{
		cart_map[i].offset = ntoh32(cart_map[i].offset);
		cart_map[i].size = ntoh32(cart_map[i].size);
	}
}

void convert_cart_map_endian_from_host_to_cart (cart_map_s* cart_map, int cart_map_max_number)
{
	int i;
	for (i = 0; i < cart_map_max_number; i++)
	{
		cart_map[i].offset = hton32(cart_map[i].offset);
		cart_map[i].size = hton32(cart_map[i].size);
	}
}

///////////////////////////////////////
// stage 1: get map
///////////////////////////////////////

void display_cart_map_ptr (cart_map_s* cart_map, int cart_map_number, int map_has_loader)
{
	int i, j;
	int next_addr;
	
	print("Cart map:\n");

	if (map_has_loader)
		next_addr = 0;
	else
	{
		// show loader
		next_addr = new_loader_and_cart_map_size? new_loader_and_cart_map_size: loader_and_cart_map_size;
		print("\t- Loader / First rom size is %.4gMBits (including map).\n",
			next_addr * 8.0 / 1024 / 1024);
	}

	j = 0;
	for (i = 0; i < cart_map_number; i++)
	{
		cart_map_s* item = &cart_map[i];
		
		assert(next_addr <= item->offset);
		if (next_addr < item->offset)
			print("\t#%02i- --- empty --- - offset=0x%x=%.4gMb - size=0x%x=%.4gMb - last=0x%x=%.4gMb\n",
				j,
				next_addr,
				next_addr * 8.0 / 1024 / 1024,
				item->offset - next_addr,
				(item->offset - next_addr) * 8.0 / 1024 / 1024,
				item->offset - 1,
				item->offset * 8.0 / 1024 / 1024);
		
		next_addr = item->offset + item->size;
		print("\t#%02i- rom '%s' - offset=0x%x=%.4gMb - size=0x%x=%.4gMb - last=0x%x=%.4gMb\n",
			j,
			item->name[0]? item->name: "(to remove)",
			item->offset,
			item->offset * 8.0 / 1024 / 1024,
			item->size,
			item->size * 8.0 / 1024 / 1024,
			next_addr - 1,
			next_addr * 8.0 / 1024 / 1024);
		j++;
	}

	if (cart_map_number)
		print("Cart map stops at %.4gMBits.\n", 8.0 / 1024 / 1024 * (cart_map[cart_map_number - 1].offset + cart_map[cart_map_number - 1].size));
}

void display_cart_map (void)
{
	display_cart_map_ptr(cart_map, cart_map_number, /* map has loader */ 0);
}

void brand_new_empty_cart_map (void)
{
	cart_map_max_number = 8;
	cart_map_number = 0;
	loader_and_cart_map_size = cart_map_max_number * sizeof(cart_map_s);
	if ((cart_map = (cart_map_s*)malloc(loader_and_cart_map_size)) == NULL)
	{
		printerrno("Cannot allocate %i bytes: malloc:", loader_and_cart_map_size);
		cart_exit(1);
	}
	cart_map[0].name[0] = 0;
	cart_map[0].offset = 0;
	cart_map[0].size = 0;
}

int load_cart_map (void)
{
	unsigned char		small_cart [SIZE_1K];
	cart_map_locator_s*	endian_locator = NULL;
	int			offset;
	
	if (cart_io_sim > 1)
	{
		printerr("Cannot manage cart map if reads are simulated\n");
		return -1;
	}
	
	// Find locator
	if (cart_verbose)
		print("Searching for locator...\n");
	for (offset = 0; !endian_locator && offset < CART_SIZE_BYTES; offset += CART_ROM_BLOCK_SIZE)
	{
		if (cart_read_mem(small_cart, GBA_ROM + offset + CART_ROM_BLOCK_SIZE - SIZE_1K, SIZE_1K) == -1)
			return -1;
		endian_locator = (cart_map_locator_s*)&small_cart[SIZE_1K - sizeof(cart_map_locator_s)];
		if (ntoh32(endian_locator->magic) == MAP_MAGIC)
		{
			cart_map_max_number = ntoh16(endian_locator->number_of_entries);
			cart_map_location = ntoh32(endian_locator->location);
			loader_and_cart_map_size = offset + CART_ROM_BLOCK_SIZE;

			if (cart_verbose)
				print("Rom map locator found ! - offset=0x%x size=0x%x (%i roms)\n", 
				       cart_map_location, 
				       loader_and_cart_map_size - cart_map_location, 
				       cart_map_max_number);

			// Reload in a proper dimensionned place

			cart_around_map_offset = GBA_ROM + cart_map_location;
			cart_around_map_size = cart_map_max_number * sizeof(cart_map_s);
			adjust_load_addresses(&cart_around_map_offset, &cart_around_map_size);

			assert(cart_around_map == NULL);
			if ((cart_around_map = (unsigned char*)malloc(cart_around_map_size)) == NULL)
			{
				printerrno("Cannot allocate %i bytes to load cart map: malloc:", cart_around_map_size);
				return -1;
			}

			if (cart_read_mem(cart_around_map, cart_around_map_offset, cart_around_map_size) == -1)
			{
				reset_cart_map();
				return -1;
			}

			cart_map = (cart_map_s*)&cart_around_map[cart_map_location - (cart_around_map_offset - GBA_ROM)];
			convert_cart_map_endian_from_cart_to_host(cart_map, cart_map_max_number);
			for (cart_map_number = 0;
			        cart_map_number < cart_map_max_number
			     && cart_map[cart_map_number].size > 0;
			     cart_map_number++);

			return 0;
		}
		else
			endian_locator = NULL;
	}
	printerr("Could not find cart map locator.\n");
	return -1;
}

void cart_map_mark_for_remove (const char* del_files[], int del_files_number)
{
	int i, j, removed;
	for (i = 0; i < del_files_number; i++)
	{
		removed = 0;
		for (j = 0; j < cart_map_number; j++)
			if (strcmp(del_files[i], cart_map[j].name) == 0)
			{
				cart_map[j].name[0] = 0;
				removed = 1;
				something_to_be_done = 1;
			}
		if (!removed)
			printerr("Rom '%s' not found in map !\n", del_files[i]);
	}
}

void cart_map_replace_loader (binware_s* loader)
{
	new_loader = loader;
}

///////////////////////////////////////
// stage 2: find holes
///////////////////////////////////////

///////////////////
// hole management

int cart_map_build_hole (void)
{
	int cart_map_index;
	int cart_map_hole_size = 2 * cart_map_max_number * sizeof(cart_map_s);
	int test_hole_offset;
	
	assert(cart_map_max_number >= MAP_MINIMUM_ENTRIES);
	
	if (loader_and_cart_map_size == sizeof(cart_map_s) * cart_map_max_number && !new_loader)
	{
		printerr("When creating a map, a loader must be specified.\n");
		return -1;
	}
	
	if ((cart_map_hole = (cart_map_s*)malloc(cart_map_hole_size)) == NULL)
	{
		printerrno("cannot allocate %s bytes for cart management", cart_map_hole_size);
		return -1;
	}
	cart_map_hole_number = 0;

	// our test begins right after the loader&map
	test_hole_offset = loader_and_cart_map_size;

	// loop on the cart map
	for (cart_map_index = 0; cart_map[cart_map_index].size > 0; cart_map_index++)
	{
		// if this rom is to be erased by user, then forget it so to count it in holes
		if (cart_map[cart_map_index].name[0] == 0)
			continue;
	
		// new data, check if it is contiguous to previous one
		assert(cart_map[cart_map_index].offset >= test_hole_offset);
		if (cart_map[cart_map_index].offset > test_hole_offset)
		{
			// it is not, so we have crossed a hole
			cart_map_hole[cart_map_hole_number].offset = test_hole_offset;
			cart_map_hole[cart_map_hole_number].size = cart_map[cart_map_index].offset - test_hole_offset;
			cart_map_hole_number++;
		}
		test_hole_offset = cart_map[cart_map_index].offset + cart_map[cart_map_index].size;
	}

	// is there a hole at the end ?
	if (test_hole_offset < CART_SIZE_BYTES)
	{
		cart_map_hole[cart_map_hole_number].offset = test_hole_offset;
		cart_map_hole[cart_map_hole_number].size = CART_SIZE_BYTES - test_hole_offset;
		cart_map_hole_number++;
	}
	
	// now we know the holes, we check if we can insert the new loader
	if (new_loader)
	{
		int max_size;
		int minimum_cart_map_number;
		
		// now if we need to burn a new loader, try to fit it at the
		// beginning of the address space:
		
		// max size for loader
		max_size = loader_and_cart_map_size;
		if (cart_map_hole_number && cart_map_hole[0].offset == loader_and_cart_map_size)
			max_size += cart_map_hole[0].size;
		// calculate trimmed size
		new_loader_trimmed_size = trim(new_loader->data, new_loader->size);

		// try to fit everything (new loader and map)
		new_loader_and_cart_map_size = new_loader_trimmed_size;
		minimum_cart_map_number = MAX(cart_map_number, MAP_MINIMUM_ENTRIES);

		new_loader_and_cart_map_size--;
		do
		{
			new_loader_and_cart_map_size++;
			adjust_rom_size(&new_loader_and_cart_map_size);
			cart_map_new_max_number = (new_loader_and_cart_map_size - new_loader_trimmed_size - sizeof(cart_map_locator_s)) / sizeof(cart_map_s);
		} while (new_loader_and_cart_map_size < max_size && cart_map_new_max_number < minimum_cart_map_number);

		// can the current map max number fit ?
		if (new_loader_and_cart_map_size > max_size)
		{
			printerr("New loader '%s' cannot fit, it needs %i bytes with map and\n"
				 "- current loader + map are %i bytes\n"
				 "- CRAP first hole size is %i bytes\n"
				 "=> total = %i bytes\n",
				 new_loader->name,
				 new_loader_and_cart_map_size,
				 loader_and_cart_map_size,
				 cart_map_hole_number? cart_map_hole[0].size: 0,
				 max_size);
			printerr("------ (show how which roms to remove to fit)\n");
			reset_cart_map();
			return -1;
		}
		cart_map_new_location = new_loader_and_cart_map_size - sizeof(cart_map_locator_s) - cart_map_new_max_number * sizeof(cart_map_s);
		
		if (cart_verbose > 0)
			print("New loader is fitting:\n"
			      "\twithout map:  size=0x%x=%gMbits - trimmed size=0x%x=%gMbits\n"
			      "\twith fit+map: size=0x%x=%gMbits - cart map size = %i roms\n",
			      new_loader->size,
			      new_loader->size * 8.0 / 1024 / 1024,
			      new_loader_trimmed_size,
			      new_loader_trimmed_size * 8.0 / 1024 / 1024,
			      new_loader_and_cart_map_size,
			      new_loader_and_cart_map_size * 8.0 / 1024 / 1024,
			      cart_map_new_max_number);
		
		// adjust/add first hole
		if (new_loader_and_cart_map_size != loader_and_cart_map_size)
		{

			// first hole right after old map
			if (cart_map_hole_number && cart_map_hole[0].offset == loader_and_cart_map_size)
			{
				if (cart_verbose > 0)
					print("\tadjusting first hole\n");
				// change its size (grow or reduce)
				cart_map_hole[0].offset += new_loader_and_cart_map_size - loader_and_cart_map_size;
				cart_map_hole[0].size -= new_loader_and_cart_map_size - loader_and_cart_map_size;
				assert(cart_map_hole[0].size >= 0);
				// if the first hole is now null-sized, we simply remove it
				if (cart_map_hole[0].size == 0)
					memmove(&cart_map_hole[0], &cart_map_hole[1], --cart_map_hole_number * sizeof(cart_map_s));
			}
			// there was a rom right after the loader+map or there was no hole,
			else
			{
				if (cart_verbose > 0)
					print("\tinserting a first new hole\n");
				// so we have to create a new hole
				if (cart_map_hole_number)
				{
					// shift holes
					assert(cart_map_hole_number < cart_map_hole_size); // just pray - things are big enough
					memmove(&cart_map_hole[1], &cart_map_hole[0], cart_map_hole_number * sizeof(cart_map_s));
				}
				cart_map_hole_number++;
				cart_map_hole[0].offset = new_loader_and_cart_map_size;
				cart_map_hole[0].size = loader_and_cart_map_size - new_loader_and_cart_map_size;
				assert(   cart_map_hole[0].size > 0
				       && (   cart_map_hole_size == 1
				           || cart_map_hole[0].offset + cart_map_hole[0].size < cart_map_hole[1].offset));
			}
		}
		something_to_be_done = 1;
	}
	else
		cart_map_new_max_number = cart_map_max_number;
	
	return 0;
}

void display_cart_map_hole (void)
{
	int i;
	
	if (cart_map_hole_number > 0)
	{
		print("Located holes in cart map are:\n");
		for (i = 0; i < cart_map_hole_number; i++)
			print("\t- #%i: offset=0x%x=%.4gMb - size=0x%x=%.4gMb - last=0x%x=%.4gMb\n",
				i,
				cart_map_hole[i].offset,
				cart_map_hole[i].offset * 8.0 / 1024 / 1024,
				cart_map_hole[i].size,
				cart_map_hole[i].size * 8.0 / 1024 / 1024,
				cart_map_hole[i].offset + cart_map_hole[i].size - 1,
				(cart_map_hole[i].offset + cart_map_hole[i].size) * 8.0 / 1024 / 1024);
	}
	else
		print("Cart is full, no hole to fill.\n"
		      "You are either a good developper or just lazy... or both ;-)\n");
}

///////////////////////////////////////
// stage 3: files insertion
///////////////////////////////////////

void cart_map_file_display_test_score (char* message, u_int64_t score)
{
	int i;
	
	print("%s placement (score %.4g):\n", message, sqrt(score) * 8 / 1024 / 1024);
	for (i = 0; i < cart_map_file_number; i++)
		print("\t- file '%s' in hole #%i (rem. 0x%x)\n", cart_map_file[i].romname, cart_map_file[i].hole_index_test, cart_map_hole_remaining_size[cart_map_file[i].hole_index_test]);
}

void cart_map_file_display_best_score ()
{
	int i;
	
	print("Best placement (score %.4g):\n", sqrt(cart_map_insertion_best_score) * 8 / 1024 / 1024);
	for (i = 0; i < cart_map_file_number; i++)
		print("\t- file '%s' in hole #%i\n", cart_map_file[i].romname, cart_map_file[i].hole_index);
}

// all files have been placed, how much does it cost ?
void cart_map_file_update_score (void)
{
	int i;
	u_int64_t score = 0;
	
	// calculate fitting score (use euclidian norm)
	for (i = 0; i < cart_map_hole_number; i++)
		score += (u_int64_t)cart_map_hole_remaining_size[i] * cart_map_hole_remaining_size[i];

	if (score > cart_map_insertion_best_score)
	{
		// this is good, we have a best score, we store the current placement
		for (i = 0; i < cart_map_file_number; i++)
		{
			cart_map_file[i].hole_index = cart_map_file[i].hole_index_test;
			assert(cart_map_file[i].hole_index >= 0 && cart_map_file[i].hole_index < cart_map_hole_number);
		}
		cart_map_insertion_best_score = score;
		if (cart_verbose > 0)
			cart_map_file_display_test_score("so far, best", score);
	}
	else if (cart_verbose > 1)
		cart_map_file_display_test_score("tried", score);
}

// called by cart_map_file_find_best_insertion
static void cart_map_file_find_best_insertion_recursive (const int cart_map_file_index)
{
	// no more index to test?
	if (cart_map_file_index >= cart_map_file_number)
	{
		// then all rom are placed, calculate score
		cart_map_file_update_score();
		return;
	}
	
	// try next hole for cart_map_file_index file
	while (1)
	{
		// try next hole
		// (the initializing value is -1 so the hole #0 is the first to be tried)
		if (++cart_map_file[cart_map_file_index].hole_index_test >= cart_map_hole_number)
		{
			// no more hole to test
			cart_map_file[cart_map_file_index].hole_index_test = -1;
			return;
		}

		// can this file be inserted in its current test hole?
		if (cart_map_hole_remaining_size[cart_map_file[cart_map_file_index].hole_index_test] >= cart_map_file[cart_map_file_index].size)
		{
			// yes, take room
			cart_map_hole_remaining_size[cart_map_file[cart_map_file_index].hole_index_test] -= cart_map_file[cart_map_file_index].size;
			// now try to insert subsequent files
			// best try will be recorded (call to cart_map_file_update_score() above)
			cart_map_file_find_best_insertion_recursive(cart_map_file_index + 1);

			// a good placement has now been or not been found, we go on seeking...
			// remove this file from its hole
			cart_map_hole_remaining_size[cart_map_file[cart_map_file_index].hole_index_test] += cart_map_file[cart_map_file_index].size;
			
			// sanity check
			assert(cart_map_hole_remaining_size[cart_map_file[cart_map_file_index].hole_index_test] <= cart_map_hole[cart_map_file[cart_map_file_index].hole_index_test].size);
		}
	}
}

// this will fill the global structure cart_map_file in.
// it needs cart_map and cart_map_hole structures correctly initialized
int cart_map_file_find_best_insertion (void)
{
	int i;
	
	// initialise the int array that keeps the current remaining size in holes
	if ((cart_map_hole_remaining_size = (int*)malloc(cart_map_hole_number * sizeof(int))) == NULL)
	{
		printerr("cannot allocate %s bytes for cart management\n", cart_map_hole_number * sizeof(int));
		return -1;
	}
	for (i = 0; i < cart_map_hole_number; i++)
		cart_map_hole_remaining_size[i] = cart_map_hole[i].size;
	
	// nothing is placed yet
	for (i = 0; i < cart_map_file_number; i++)
		cart_map_file[i].hole_index = cart_map_file[i].hole_index_test = -1;
	
	// dumbfully try everything
	cart_map_insertion_best_score = 0;
	cart_map_file_find_best_insertion_recursive(/* start with file index number */ 0);

	// have we succeeded ?
	if (cart_map_insertion_best_score == 0)
	{
		printerr("Could not fit all roms into cart, that's too bad.\n");
		return -1;
	}

	return 0;
}

int cart_map_find_best_insertion_for_files (const char* add_files[], int add_files_number)
{
	int i;
	
	if (add_files_number > FILE_NUMBER_MAX)
	{
		printerr("Too many (%i) files to add, raise FILE_NUMBER_MAX(%i) in source code and recompile.\n", add_files_number, FILE_NUMBER_MAX);
		return -1;
	}

	cart_map_file_number = add_files_number;
	for (i = 0; i < add_files_number; i++)
	{
		int			size;
		int			trimmed_size;
		unsigned char*		rom;
		char*			comma;
		const char*		finalromname;
		char*			userromname = NULL;
		const char*		add_file = add_files[i];

		// check if user wants to rename the rom
		if ((comma = strstr(add_file, ",")))
		{
			userromname = &comma[1];
			*comma = 0;
		}

		// load, check and trim rom
		size = 0;
		if ((rom = load_from_file(add_file, NULL, &size)) == NULL)
			return -1;

		trimmed_size = trim(rom, size);
		
		// pad size to be "loader compatible"
		adjust_rom_size(&trimmed_size);

		if (userromname)
			finalromname = userromname;
		else
		{
			const char* name = romname(rom);
			finalromname = name[0]? name: filename2romname(add_file);
		}
		strcpy(cart_map_file[i].romname, finalromname);
		cart_map_file[i].userromname = userromname;
		cart_map_file[i].filename = add_file;
		cart_map_file[i].original_size = size;
		cart_map_file[i].size = trimmed_size;
		
		if (cart_verbose)
			print("Adding file '%s' name '%s' size=0x%x / %.4gMb (trim+fit: %+g%%)\n",
				add_file,
				finalromname,
				trimmed_size,
				trimmed_size * 8.0 / 1024 / 1024,
				100.0 * trimmed_size / size - 100.0);

		free(rom);
	}
	
	if (cart_map_file_number)
	{
		something_to_be_done = 1;
		return cart_map_file_find_best_insertion();
	}
	return 0;
}

/////////////////////////////////////////////
// stage 4: build new map and burn everything
/////////////////////////////////////////////

void display_change_map_file (void)
{
	int change_map_file_index;
	
	print("Changes in map:\n");
	for (change_map_file_index = 0; change_map_file_index < change_map_file_number; change_map_file_index++)
	{
		cart_map_file_s* item = &change_map_file[change_map_file_index];
		if (cart_verbose || (item->action == MAP_ACTION_DONTOUCH && cart_verbose > 1))
			print("\taction:%s rom:'%s' offset:0x%x size:0x%x last:0x%x\n",
				item->action == MAP_ACTION_ADD? "add   ":
				item->action == MAP_ACTION_DONTOUCH? "keep  ":
				item->action == MAP_ACTION_REMOVE? "remove":
				"error",
				item->romname,
				item->offset,
				item->size,
				item->offset + item->size - 1);
	}
}

// burn contiguous change_map_file chunks:
// all indexes'actions have to be MAP_ACTION_ADD so that we are assured that
// the addresses are also contiguous
int burn_map_chunk (int burn_map_file_index_start, int burn_map_file_index_end)
{
	int change_chunk_offset = 0, change_chunk_size = 0;	// that we need
	int burn_chunk_offset = 0, burn_chunk_size = 0;		// for burning
	int burn_cart_map_location = 0;				// cart map burning
	int burn_cart_map_max_number = 0;			// cart map burning

	unsigned char* chunkrom;				// this will be burned
	int border_offset, border_size;
	int index;
	
	if (burn_map_file_index_end > burn_map_file_index_start)
		print("\nBurn map entries #%i..#%i...\n", burn_map_file_index_start, burn_map_file_index_end);
	else
		print("\nBurn map entry #%i...\n", burn_map_file_index_start);
	
	// find limits
	
	// is it the first chunk ?
	if (burn_map_file_index_start == 0)
	{
		// so map has to be burned
		if (new_loader)
		{
			change_chunk_offset = 0;
			burn_cart_map_location = cart_map_new_location;
			change_chunk_size = change_map_file[burn_map_file_index_end].offset + change_map_file[burn_map_file_index_end].size;
		}
		else
		{
			change_chunk_offset = burn_cart_map_location = cart_map_location; // map location does not change
			change_chunk_size = change_map_file[burn_map_file_index_end].offset + change_map_file[burn_map_file_index_end].size - change_map_file[burn_map_file_index_start].offset;
			change_chunk_size -= change_chunk_offset; // don't touch the loader
		}
		// cart_map_new_max_number is always initialized to cart map size + 1 (to cart_map_max_number+1 if !new_loader)
		burn_cart_map_max_number = cart_map_new_max_number - 1;
	}
	else
	{
		change_chunk_offset = change_map_file[burn_map_file_index_start].offset;
		change_chunk_size = change_map_file[burn_map_file_index_end].offset + change_map_file[burn_map_file_index_end].size - change_chunk_offset;
	}
	
	burn_chunk_offset = change_chunk_offset;
	burn_chunk_size = change_chunk_size;
	adjust_burn_addresses(&burn_chunk_offset, &burn_chunk_size);

	// allocate memory
	if ((chunkrom = (unsigned char*)malloc(burn_chunk_size)) == NULL)
	{
		printerr("cannot allocate %i/0x%x bytes for burning changes (index %i .. %i)\n", burn_map_file_index_start, burn_map_file_index_end);
		return -1;
	}
	// which is the good value when cart map is erased
	memset(chunkrom, 0xff, burn_chunk_size);
	
	// get the rom border from cart
	
	// low border:
	border_offset = burn_chunk_offset;
	border_size = change_chunk_offset - burn_chunk_offset;
	adjust_load_addresses(&border_offset, &border_size);
	assert(border_offset == burn_chunk_offset); // at least we want this ok
	assert(border_offset + border_size >= change_chunk_offset);
	if (border_size > 0)
	{
		if (cart_verbose)
			print("Loading low border\n");
		if (cart_read_mem(&chunkrom[0], GBA_ROM + border_offset, border_size) < 0)
		{
			free(chunkrom);
			return -1;
		}
	}

	// high border:
	border_offset = change_chunk_offset + change_chunk_size;
	border_size = burn_chunk_offset + burn_chunk_size - border_offset;
	adjust_load_addresses(&border_offset, &border_size);
	assert(border_offset + border_size == burn_chunk_offset + burn_chunk_size); // at least we want this ok
	assert(border_offset <= change_chunk_offset + change_chunk_size);
	if (border_size > 0)
	{
		if (cart_verbose)
			print("Loading high border\n");
		if (cart_read_mem(&chunkrom[border_offset - burn_chunk_offset], GBA_ROM + border_offset, border_size) < 0)
		{
			free(chunkrom);
			return -1;
		}
	}
	
	if (burn_map_file_index_start == 0)
	{
		int locator_offset_in_chunk;
		cart_map_locator_s* burn_cart_map_locator;
		
		// remember that the loader is not described in burnt cart map (which is womewhere in chunkrom[])
		// while it is present in cart_map_new[0]

		if (new_loader)
		{
			// copy the new loader in chunk
			assert(burn_chunk_offset == 0);
			memcpy(chunkrom, new_loader->data, new_loader_trimmed_size);
			
			assert(burn_cart_map_location > new_loader_trimmed_size);
		}
		else
			assert(burn_cart_map_location == cart_map_location); // check same location

		// check burned map's termination
		assert(   cart_map_new_number == burn_cart_map_max_number + 1
		       || (   cart_map_new[cart_map_new_number].size == 0
			   && cart_map_new[cart_map_new_number].offset == 0));

		// locate the locator
		locator_offset_in_chunk = burn_cart_map_location + (burn_cart_map_max_number * sizeof(cart_map_s)) - burn_chunk_offset;
		if (burn_map_file_index_end == 0)
			assert(locator_offset_in_chunk + burn_chunk_offset - change_chunk_offset + sizeof(cart_map_locator_s) == change_chunk_size);
		else
			assert(locator_offset_in_chunk + burn_chunk_offset - change_chunk_offset + sizeof(cart_map_locator_s) < change_chunk_size);
		burn_cart_map_locator = (cart_map_locator_s*)&chunkrom[locator_offset_in_chunk];
	
		// copy map but skip loader, we don't want it in map
		memcpy(&chunkrom[burn_cart_map_location - burn_chunk_offset], &cart_map_new[1], burn_cart_map_max_number * sizeof(cart_map_s));
		convert_cart_map_endian_from_host_to_cart((cart_map_s*)&chunkrom[burn_cart_map_location - burn_chunk_offset], burn_cart_map_max_number);
		burn_cart_map_locator->magic = hton32(MAP_MAGIC);
		burn_cart_map_locator->location = hton32(burn_cart_map_location);
		burn_cart_map_locator->number_of_entries = hton16(burn_cart_map_max_number);
	}
	
	// now we can fill the rom with files, skip 0 which is loader+map
	for (index = MAX(burn_map_file_index_start, 1); index <= burn_map_file_index_end; index++)
	{
		int size_to_load;
		cart_map_file_s* item = &change_map_file[index];
		
		assert(item->action == MAP_ACTION_ADD);
		assert(item->filename);
		assert(item->offset >= burn_chunk_offset && item->offset + item->size <=  burn_chunk_offset + burn_chunk_size);
		size_to_load = item->original_size;
		// trimmed rom ?
		if (size_to_load > item->size)
			size_to_load = item->size;
		assert(size_to_load > 0);
		if (load_from_file(item->filename, &chunkrom[item->offset - burn_chunk_offset], &size_to_load) < 0)
		{
			free(chunkrom);
			return -1;
		}
	
		// pad file with the last byte
		if (item->size > size_to_load)
			memset(&chunkrom[item->offset - burn_chunk_offset + size_to_load], chunkrom[item->offset - burn_chunk_offset + size_to_load - 1], item->size - size_to_load);

		// homebrew roms often need correction, correct_header() has to be reworked
		correct_header(&chunkrom[item->offset - burn_chunk_offset],
			       item->userromname? item->userromname: item->romname,
			       /* force name */ item->userromname? 1: 0);
	}
	
	// yeah! it's time to burn (at last!! I've been waiting for that moment for a while...)

	if (cart_io_sim)
		print("No burning (simulation)\n");
	else if (cart_burn(GBA_ROM, burn_chunk_offset, chunkrom, 0, burn_chunk_size) < 0)
	{
		free(chunkrom);
		return -1;
	}

	free(chunkrom);
	return 0;
}

// the most interesting part: burn parts (new file and/or erased headers) and reburn cart map
int cart_map_process_changes (void)
{
	cart_map_file_s* new;
	int change_map_file_index;
	int cart_map_index;
	int cart_map_hole_index;
	int cart_map_new_index;
	int hole_offset;
	int burn_map_file_index_start;
	int burn_map_file_index_end;

	if (!something_to_be_done)
	{
		print("Nothing to do.\n");
		return 0;
	}

	////////////////////////
	// build cart action map
	// (collate chunks)
	
	// change_map_file_number is temporarily the max number
	change_map_file_number = 1 + cart_map_number + cart_map_hole_number + cart_map_file_number;
	if ((change_map_file = (cart_map_file_s*)malloc(sizeof(cart_map_file_s) * change_map_file_number)) == NULL)
	{
		printerr("cannot allocate %i bytes for change map processing !\n", sizeof(cart_map_file_s) * change_map_file_number);
		return -1;
	}
	
	// in change map (change_map_file) and new cart map (cart_map_new),
	// the loader+map rom is described (so that we can possibly manage its
	// replacement), but it will not be burned.
	
	new = &change_map_file[0];
	strcpy(new->romname, "Loader+map");
	new->filename = NULL;
	new->original_size = new->size = new_loader? new_loader_and_cart_map_size: loader_and_cart_map_size;
	new->hole_index = -1;
	new->offset = 0;
	new->action = MAP_ACTION_DONTOUCH;

	change_map_file_index = 1;
	cart_map_index = 0;
	cart_map_hole_index = 0;
	hole_offset = -1;
	
	while (cart_map_index < cart_map_number || cart_map_hole_index < cart_map_hole_number)

		// Do we have a hole to fill = rom(s) to write and...
		//	no more rom in original map or...
		//	hole starts before next rom or...
		//	hole covers a deleted rom
		
		if (   cart_map_hole_index < cart_map_hole_number
		    && (   cart_map_index >= cart_map_number
		        || cart_map_hole[cart_map_hole_index].offset < cart_map[cart_map_index].offset
		        || (   cart_map_hole[cart_map_hole_index].offset == cart_map[cart_map_index].offset
		            && !cart_map[cart_map_index].name[0])))
		{
			// added rom (in hole)

			int cart_map_file_index;

			hole_offset = cart_map_hole[cart_map_hole_index].offset;
			
			// check if we do not step out
			assert(change_map_file_index < change_map_file_number);

			// look for files placed in this hole
			for (cart_map_file_index = 0; cart_map_file_index < cart_map_file_number; cart_map_file_index++)
				if (cart_map_file[cart_map_file_index].hole_index == cart_map_hole_index)
				{
					cart_map_file_s* src = &cart_map_file[cart_map_file_index];
					cart_map_file_s* new = &change_map_file[change_map_file_index];
					*new = *src;
					new->offset = hole_offset;
					new->action = MAP_ACTION_ADD;
					
					hole_offset += new->size;

					// next in change map
					change_map_file_index++;
				}
			
			// next hole
			cart_map_hole_index++;
		}
		// Otherwise, do we have a kept rom in original map ?
		else if (cart_map_index < cart_map_number && cart_map[cart_map_index].name[0])
		{
			// existing rom
			
			cart_map_s* src = &cart_map[cart_map_index];
			cart_map_file_s* new = &change_map_file[change_map_file_index];
			
			// check if we do not step out
			assert(change_map_file_index < change_map_file_number);

			strcpy(new->romname, src->name);
			new->filename = NULL;
			new->original_size = new->size = src->size;
			new->hole_index = -1;
			new->offset = src->offset;
			new->action = MAP_ACTION_DONTOUCH;
			
			// next in change map
			change_map_file_index++;

			// next rom in original map
			cart_map_index++;
		}
		// Then we should have a deleted rom.
		else if (cart_map_index < cart_map_number && !cart_map[cart_map_index].name[0])
		{
			// if there is no more hole or (we are in a hole
			// and) if cart rom has not been covered yet (holes
			// are covered if they have to before we reach this
			// point, so if hole_offset == cart_map[cart_map_index].offset,
			// then we had nothing to put at this hole_offset).
			
			if (   cart_map_hole_index >= cart_map_hole_number
			    || hole_offset <= cart_map[cart_map_index].offset)
			{
				// then we have to blow this cart rom header out of our way
				
				cart_map_s* src = &cart_map[cart_map_index];
				cart_map_file_s* new = &change_map_file[change_map_file_index];
			
				// check if we do not step out
				assert(change_map_file_index < change_map_file_number);

				strcpy(new->romname, src->name);
				new->filename = NULL;
				new->original_size = new->size = src->size;
				new->hole_index = -1;
				new->offset = src->offset;
				new->action = MAP_ACTION_REMOVE;
			
				// next in change map
				change_map_file_index++;
			}
			// otherwise we simply ignore it, it has already been overridden by added roms

			// next rom in original map
			cart_map_index++;
		}
		else
			assert(0);

	// now change_map_file_number reflects the real entries we have in change_map_file
	change_map_file_number = change_map_file_index;

	if (cart_verbose)
		display_change_map_file();
	
	/////////////////////////////////
	// build updated cart map to burn
	
	cart_map_new_number = 0;
	for (change_map_file_index = 0; change_map_file_index < change_map_file_number; change_map_file_index++)
		if (cart_map_file[change_map_file_index].action != MAP_ACTION_REMOVE)
			cart_map_new_number++;
	if (cart_map_new_number > cart_map_max_number)
	{
		printerr("new cart map size is %i, but the maximum number of rom in map is %i.\n"
		         "something has to be improved here...\n",
		         cart_map_new_number,
		         cart_map_max_number);
		reset_cart_map();
		return -1;
	}
	// cart_map_new will contain loader+map but it will not be burned, so we +1 here
	// cart_map_new_max_number has been initialized in buid_hole().
	assert((new_loader && cart_map_new_max_number > 0) || cart_map_new_max_number == cart_map_max_number);
	// now we add one because we need to deal with the loader too in the following
	// remember that the loader will not be present in final burnt cart map
	cart_map_new_max_number++;
	if ((cart_map_new = (cart_map_s*)malloc(cart_map_new_max_number * sizeof(cart_map_s))) == NULL)
	{
		printerr("cannot allocate %i bytes of memory for new cart map.\n", cart_map_new_number * sizeof(cart_map_s));
		reset_cart_map();
		return -1;
	}

	// build new map:
	// copy from change_map_file except removed rom
	// loader+map (item 0 in change_map_file) will be copied
	cart_map_new_index = 0;
	for (change_map_file_index = 0; change_map_file_index < change_map_file_number; change_map_file_index++)
		if (change_map_file[change_map_file_index].action != MAP_ACTION_REMOVE)
		{
			strcpy(cart_map_new[cart_map_new_index].name, change_map_file[change_map_file_index].romname);
			cart_map_new[cart_map_new_index].offset = change_map_file[change_map_file_index].offset;
			cart_map_new[cart_map_new_index].size = change_map_file[change_map_file_index].size;
			cart_map_new_index++;
		}
	cart_map_new_number = cart_map_new_index;
	if (cart_map_new_number < cart_map_new_max_number)
	{
		strcpy(cart_map_new[cart_map_new_number].name, "nomore");
		cart_map_new[cart_map_new_number].size = cart_map_new[cart_map_new_number].offset = 0;
	}
			
	if (cart_verbose > 0)
	{
		print("New ");
		display_cart_map_ptr(cart_map_new, cart_map_new_number, /* map has loader */ 1);
	}
	
	//////////////////////////////////////////
	// reburn changed cart chunks (map + roms)

	print("Applying changes...\n");

	// at minimum, we do this:
	burn_map_file_index_start = 0; // cart map must be reburned
	burn_map_file_index_end = 0; // cart map must be reburned
	
	if (change_map_file_number == 1)
	{
		// user want to reburn loader and only loader (there are no other roms than loader)
		burn_map_chunk(0, 0);
	}
	else for (change_map_file_index = 1; change_map_file_index < change_map_file_number; change_map_file_index++)
	{
		cart_map_file_s* item = &change_map_file[change_map_file_index];
		if (item->action == MAP_ACTION_ADD)
		{
			if (burn_map_file_index_start < 0)
				burn_map_file_index_start = change_map_file_index;
			else
				// sanity check
				assert((change_map_file[change_map_file_index - 1].offset + change_map_file[change_map_file_index - 1].size) == item->offset);
			burn_map_file_index_end = change_map_file_index;
		}

		// the hole has been filled, or there is nothing more in change_map_file, time to burn !
		if (   (item->action != MAP_ACTION_ADD || change_map_file_index == change_map_file_number - 1)
		    && burn_map_file_index_start >= 0)
		{
			if (burn_map_chunk(burn_map_file_index_start, burn_map_file_index_end) < 0)
			{
				reset_cart_map();
				return -1;
			}
			burn_map_file_index_start = burn_map_file_index_end = -1;
		}
	}
	

	print("... done\n");
	reset_cart_map();
	return 0;
}
