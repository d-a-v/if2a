/* 
 * Based on f2a by Ulrich Hecht <uli@emulinks.de>
 * if2a by D. Gauchard <deyv@free.fr>
 * F2A Ultra support by Vincent Rubiolo <vincent.rubiolo@free.fr>
 * Licensed under the terms of the GNU Public License version 2
 */


#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <sys/stat.h>

#include "libf2a.h"

int cart_verbose = 0;
int cart_io_sim = 0;

int filesize (const char* filename)
{
    struct stat st;
    if (stat(filename, &st) < 0)
	{
        printerrno("stat(%s)", filename);
        return -1;
    }
    return st.st_size;
}

unsigned char* load_from_file(const char* filename, unsigned char* user_buffer, int* size)
{
	// if buffer is NULL then memory is allocated and *size updated
	// if buffer is not NULL, then
	//	- if *size if 0, then it is updated, otherwise it is used
	//	- buffer is filled with *size bytes
	//	(filesize must be >= *size)
	// returns NULL if error otherwise the (possibly allocated) buffer
	
	FILE* f;
	int fsize;
	unsigned char* buffer = user_buffer;
	
	if ((fsize = filesize(filename)) < 0)
		return NULL;

	if (*size == 0)
		*size = fsize;
	if (*size > fsize)
	{
		printerr("Problem during size check for file %s (requested size %i < file size %i)\n", filename, *size, fsize);
		return NULL;
	}

	if (!buffer && (buffer = (unsigned char*)malloc(*size)) == NULL)
	{
		printerrno("malloc(%i) for file '%s' loading", *size, filename);
		return NULL;
	}

	if ((f = fopen(filename, "rb")) == NULL)
	{
		printerrno("fopen(%s)", filename);
		if (!user_buffer)
			free(buffer);
		return NULL;
	}
	
        if (fread(buffer, *size, 1, f) != 1)
	{
		if (ferror(f))
		        printerrno("read(%s)", filename);
		else
			printerr("Could not read %i bytes from file %s\n", fsize, filename);
		if (!user_buffer)
			free(buffer);
		return NULL;
	}

	fclose(f);
	return buffer;
}

int buffer_from_file(const char* filename, unsigned char* buffer, int size_to_check)
{
	// fills in buffer with contents of file named 'filename'
	// checks that file is at minimum of size size_to_check
	
	return load_from_file(filename, buffer, &size_to_check)? 0: -1;
}

unsigned char* download_from_file(const char* filename, int* size)
{
	// load whole file, modify *size
	*size = 0;
	return load_from_file(filename, NULL, size);
}

int buffer_to_file(const char* filename, const unsigned char* buffer, int size_to_write)
{
	// writes the contents of buffer into file named 'filename'
	// if file exists, its contents get overwritten

	FILE* f;
	int res;

	if ((f = fopen(filename, "wb")) == NULL)
	{
		printerrno("fopen(%s)", filename);
		return -1;
	}
	
	if ((res = fwrite(buffer, size_to_write, 1, f)) != 1)
	{
		if (ferror(f))
			printerrno("write(%s)", filename);
		else
			printerr("Could not write to file %s\n", filename);
		return -1;
	}
	
	fclose(f);
	return 0;
}

// binware_load:
// * parameters
//   this function setups the dst binware according to the parameters.
//   const binware_s binware[] is the binware array (null terminated) containing available internal choices
//   char* file (can be null) is the user selection
//   name is a text string to be displayed for verbosity
// * how to use
//   file can be either
//   - a digit ("0", "1"...) matching the desired internal binary (the digit is the index inside binware[])
//   - a file name matching an existing file to load
//   if the file is a digit and fits the binware[] array size, the matching binware is selected
//   if the digit does not fit, the file is loaded
//   if the file is null or cannot be loaded, an error is emitted and the list of available internal binaries is displayed
//   (no error is emitted if the file matches the special string "list")

int binware_load (binware_s* dst, const binware_s binware[], const char* file, const char* name)
{
	struct stat st;

    // No file specified or file is a digit
	if (!file || (strlen(file) == 1 && isdigit(file[0])))
	{
		int i;
		int n;
		if (file)
		{
			n = file[0] - '0';
			for (i = 0; binware[i].size && i != n; i++);
		}
		else
			i = 0;  // default/fallback is to use first internal binary

		if (binware[i].size || i == 0)
		{
			*dst = binware[i];
			if (cart_verbose)
			{
				if (dst->name)
					print("Will use internal file %s for %s if needed\n", dst->name, name);
				else
					print("No available binware for %s\n", name);
			}
			return 0;
		}
	}

	if (stat(file, &st) == -1)
	{
		int error = errno;
		if (error == ENOENT)
		{
			int i;

            // This was a real file : print the errno 
            if (strcmp(file, "list") !=0)
                printerr("%s: %s\n", file, strerror(error));
                        
            // In all cases, print help about internal binaries
            print("Available internal files for %s:\n", name);
            for (i = 0; binware[i].size; i++)
                print("\t%i = %s (%i bytes)\n", i, binware[i].name, binware[i].size);
            print("or\tname = any external file name\n");

            // If the name was 'list', this is not an error
            if (strcmp(file, "list") == 0)
                return 0;
		}
		return -1;
	}

	if ((dst->data = download_from_file(file, &dst->size)) == NULL)
		return -1;
	dst->name = file;
	
	if (cart_verbose)
		print("Will use external file %s for %s if needed\n", dst->name, name);

	return 0;
}

//////////////////////////////////////
// ntohl/htonl/ntohs/htons replacement

static int little_endian = -1;

int is_littleendian_host (void)
{
	assert(little_endian != -1);
	return little_endian != 0;
}

void check_endianness (void)
{
	u_int16_t x = 1;
	little_endian = ((char*)&x)[0] != 0;
	if (cart_verbose > 1)
		print("%s-endian host detected\n", little_endian? "Little": "Big");
}

u_int16_t swap16 (u_int16_t x)
{
	u_int16_t y = x;
	((char*)&y)[0] = ((char*)&x)[1];
	((char*)&y)[1] = ((char*)&x)[0];
	return y;
}

u_int32_t swap32 (u_int32_t x)
{
	u_int32_t y = x;
	((char*)&y)[0] = ((char*)&x)[3];
	((char*)&y)[1] = ((char*)&x)[2];
	((char*)&y)[2] = ((char*)&x)[1];
	((char*)&y)[3] = ((char*)&x)[0];
	return y;
}

u_int16_t ntoh16 (u_int16_t x)
{
	assert(little_endian != -1);
	return little_endian? swap16(x): x;
}

u_int16_t hton16 (u_int16_t x)
{
	assert(little_endian != -1);
	return little_endian? swap16(x): x;
}

u_int32_t ntoh32 (u_int32_t x)
{
	assert(little_endian != -1);
	return little_endian? swap32(x): x;
}

u_int32_t hton32 (u_int32_t x)
{
	assert(little_endian != -1);
	return little_endian? swap32(x): x;
}

u_int16_t tolittleendian16 (u_int16_t x)
{
	assert(little_endian != -1);
	return little_endian? x: swap16(x);
}

u_int32_t tolittleendian32 (u_int32_t x)
{
	assert(little_endian != -1);
	return little_endian? x: swap32(x);
}

//////////////////////////////////////
// default (terminal) print functions

#define PRINTLEN	256

// this one exactly acts like printf(...) does
void regular_print (const char* format, ...)
{
	va_list ap;
	
	va_start(ap, format);
	vprintf(format, ap);
	va_end(ap);
#if _WIN32
	fflush(stdout);
#endif
}

// this one exactly acts like fprintf(stderr, ...) does
void regular_printerr (const char* format, ...)
{
	va_list ap;
	
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);
#if _WIN32
	fflush(stderr);
#endif
}

// this one uses printerr and acts like perror(...) does
// it is even more interesting since text can be formatted like with printf
// (\n added though - for perror() compatibility)
void printerrno (const char* format, ...)
{
	static char dialog[PRINTLEN];
	va_list ap;
	int err = errno;
	
	va_start(ap,format);
	vsnprintf(dialog, PRINTLEN, format, ap);
	va_end(ap);
	printerr("[errno=%i: %s] %s\n", err, strerror(err), dialog);
}

void regular_printflush (void)
{
	fflush(stdout);
}

void regular_printerrflush (void)
{
	fflush(stderr);
}

print_f		print = regular_print;
printflush_f	printflush = regular_printflush;
print_f		printerr = regular_printerr;
printflush_f	printerrflush = regular_printerrflush;
