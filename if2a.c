 /* 
  * Based in f2a by Ulrich Hecht <uli@emulinks.de>
  * if2a by D. Gauchard <deyv@free.fr>
  * F2A Ultra support by Vincent Rubiolo <vincent.rubiolo@free.fr>
  * Licensed under the terms of the GNU Public License version 2
  */

// Main if2a file

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "getopt.h"
#include "libf2a.h"

void print_if2a_version(void)
{
	print("if2a-" VERSION " - http://if2a.free.fr\n");
}

void help(const char *name)
{
	// FIXME : some of this needs to go in a manpage
	print("Usage: %s options [file [file [...]]]\n"
	      "Linker type selection:\n"
	      "	-l <t>	Linker type (gba    : USB linker [default],\n"
	      "		             writer : USB writer [unsupported])\n"
	      "Cart selection:\n"
	      "	-t <t>  f2a cart type (T for Turbo, P for Pro, U for Ultra)\n"
	      "	-S <s>	cart size (suffix kb,mb,kB,mB)\n"
	      "	-L <f>	load loader or use an internal one (-L list)\n"
	      "	-F <f>	load firmware or use an internal one (-F list)\n"
	      "	-B <f>	load multiboot or use an internal one (-B list)\n"
	      "	-I <f>	load splash image or an internal one (-I list)\n"
	      "\nEASYROM;) options:\n"
	      "	-p	show cart rom map\n"
	      "	-A <r>	insert rom into cart (multiple -A allowed)\n"
	      "	   <r>,<n> will change the name of the rom [unsupported]\n"
	      "	-X <r>	remove rom from cart (match map name - multiple -X allowed)\n"
	      "	-Y	create (or overwrite) cart map\n"
	      "\nROM options:\n"
	      "	-R	read ROMs from cart (and generate filenames)\n"
	      "		Individual ordered ROM selection (optional):\n"
	      "		.	: do not save ROM\n"
	      "		@	: automatically generate filename\n"
	      "		<name>	: assign file<name> to ROM\n"
	      "	-W	write ROMs to cart\n"
	      "	-M	display ROM map (scanning)\n"
	      "	-s	do not try to reduce ROM size (default: reduce to fit loader)\n"
	      "	-a	always try to reduce ROM size (default: reduce to fit loader)\n"
	      "	-f	force writing (do not compare with cart contents)\n"
	      "	-C	clean remaining space\n"
	      "\nSRAM options:\n"
	      "	-r <f>  read SRAM from cart\n"
	      "	-w <f>  write SRAM to cart\n"
	      "\nDebugging options:\n"
	      "	-c	(with -W) check whole file (not just borders) before burning\n"
	      "	-n	do not insert f2a loader (default is to insert one)\n"
	      "	-H	do not check and correct ROM headers\n"
	      "\nOther options:\n"
	      "	-d	dummy - do not write -\n"
	      "	-d	(one more -d) - do not read -\n"
	      "	-v	be more verbose (Max verbosity is -vv)\n"
	      "	-m <f>	send multiboot file\n"
	      "\nLoader-PRO's (GBA-loader-3.x) SRAM manager specific:\n"
	      "       -b <b>  specify bank in SRAM\n"
	      "	A bank can be 'all', '1' or '2a' or '3b2' (same format as f2apro's cart loader)\n"
	      "	1a1 (8KB) \\\n"
	      "	1a2 (8KB)  \\  1a   \\\n"
	      "	1a3 (8KB)  / (32KB) \\\n"
	      "	1a4 (8KB) /          \\   1\n"
	      "	1b1 (8KB) \\          / (64KB)\n"
	      "	1b2 (8KB)  \\  1b    /\n"
	      "	1b3 (8KB)  / (32KB)/\n"
	      "	1b4 (8KB) /\n"
	      "	2a1 (8KB)\n"
	      "	...\n"
	      "	4b4 (8KB)\n"
	      "\nF2A ULTRA specific:\n"
	      "	-u <f>	dump SVD (Saver Virtual Disk) data from cart into file\n"
	      "	-U <f>	write SVD data to cart\n"
	      "	-K <f>	write content descriptor to cart\n"
	      "	-k <f>	dump content descriptor from cart into file\n"
	      "	-G <f>	generate GameID for ROM\n"
	      "	-E <f>	dump Die Hard data to file [unsupported]\n"
	      "	-e <f>	write Die Hard data to cart [unsupported]\n"
	      "\nNotes:\n"
	      " + Whole cart contents should always be specified when writing ROMS without\n"
	      "   EASYROM;). Cart will be updated according to differences between cmdline\n"
	      "   and cart contents.\n"
	      " + EASYROM;) allows to add/remove ROMS on the fly, with if2a finding room\n"
	      "   automatically. There is thus no need to rewrite whole cart each time to\n"
	      "   remove the first written ROMS.\n", name);
}

// modes used to parse the quite too complex command-line
enum mode_e
{
	MODE_WRITE_ROM,
	MODE_WRITE_SRAM,
	MODE_WRITE_SVD,
	MODE_WRITE_CD,
	MODE_WRITE_DH,
	MODE_READ_ROM,
	MODE_READ_SRAM,
	MODE_READ_SVD,
	MODE_READ_CD,
	MODE_READ_DH,
	MODE_SCANNED_MAP,
	MODE_MB_USER,
	MODE_EASYROM,
	MODE_EASYROM_MAP,
	MODE_GEN_ID,
	MODE_UNDEF,
};

int main(int argc, char *argv[])
{
	int opt;

	typedef enum
	{
		LINKER_TEMPLATE,	// test only
#if F2AL
		LINKER_F2A_USB_GBA,	// working
#endif
#if F2AW
		LINKER_F2A_USB_WRITER,	// unsupported yet
#endif
#if F2AP
		LINKER_F2A_PARALLEL_GBA,	// unsupported yet
#endif
	} linker_t;

#if F2AL
	linker_t linker_type = LINKER_F2A_USB_GBA;	// default
#elif F2AW
	linker_t linker_type = LINKER_F2A_USB_WRITER;	// default
#else
	linker_t linker_type = LINKER_TEMPLATE;	// default
#endif

	enum mode_e mode = MODE_UNDEF;
	int autodetection = 1;
	char *bank = NULL;
	char *multiboot_user_file = NULL;
	char *sram_file = NULL;
	char *svd_file = NULL;

	char *new_cart_size = NULL;
	//int non_opt_idx = 0;
	int non_opt_nb = 0;

	int enum_fw = 0;
	int enum_spl = 0;
	int enum_mb = 0;
	int enum_ld = 0;

	char *multiboot_file = NULL;
	char *firmware_file = NULL;
	char *splash_file = NULL;
	char *loader_file = NULL;

	int create_cart_map = 0;
	const char *add_files[FILE_NUMBER_MAX];
	int add_files_number = 0;
	const char *del_files[FILE_NUMBER_MAX];
	int del_files_number = 0;

	int clean_cart = 0;
	int cart_use_loader = 1;
	cart_type_e cart_type = CART_TYPE_UNDEF;

	// Defaults for SRAM: -1 (128KB for Ultra, 256KB for pro)
	//                    and no offset (i.e. all SRAM)
	int sram_offset = 0;
	int sram_size = -1;

	// getop() behaviour : do not print error messages
	opterr = 0;

	print_if2a_version();

	// Initialize cart-specific settings (trimming, etc)
	cart_init();

	// Print help when no arguments
	if (argc == 1)
	{
		help(argv[0]);
		exit(5);
	}

	do
	{
		// The first colon should stay here : it is a getopt() setting.
		opt = getopt(argc, argv,
			     ":dvhMfasRHCcpnb:S:m:t:e:E:u:U:k:K:G:L:F:B:I:A:X:l:r:w:YW");
		switch (opt)
		{

		case 'l':
			if (0);
#if F2AL
			else if (strcasecmp(optarg, "gba") == 0)
				linker_type = LINKER_F2A_USB_GBA;
#endif
#if F2AW
			else if (strcasecmp(optarg, "writer") == 0)
				linker_type = LINKER_F2A_USB_WRITER;
#endif
#if F2AP
			else if (strcasecmp(optarg, "par") == 0)
				linker_type = LINKER_F2A_PARALLEL_GBA;
#endif
			else
			{
				printerr("Unrecognized linker type '%s' !\n",
					 optarg);
				exit(1);
			}
			break;

		case 't':
			autodetection = 0;

			switch (optarg[0])
			{
			case 'P':
				cart_type = CART_TYPE_F2A_PRO;
				break;
			case 'U':
				cart_type = CART_TYPE_F2A_ULTRA;
				break;
			default:
				printerr("Unknown cart type: '%c'\n",
					 optarg[0]);
				exit(1);	// no cart_exit(1) as cart is not available yet
			}
			break;

		case 'E':
			mode = MODE_WRITE_DH;
			break;

		case 'e':
			mode = MODE_READ_DH;
			break;

		case 'U':
			mode = MODE_WRITE_SVD;
			break;

		case 'u':
			mode = MODE_READ_SVD;
			svd_file = optarg;
			break;

		case 'K':
			mode = MODE_WRITE_CD;
			break;

		case 'k':
			mode = MODE_READ_CD;
			break;

		case 'r':
			mode = MODE_READ_SRAM;
			sram_file = optarg;
			break;

		case 'R':
			mode = MODE_READ_ROM;
			break;

		case 'w':
			mode = MODE_WRITE_SRAM;
			sram_file = optarg;
			break;

		case 'W':
			mode = MODE_WRITE_ROM;
			break;

		case 'H':
			cart_correct_header_allowed = 0;
			break;

		case 'C':
			clean_cart = 1;
			break;

		case 'b':
			bank = optarg;
			break;

		case 'v':
			cart_verbose++;
			break;

		case 'n':
			cart_use_loader = 0;
			break;

		case 'f':
			cart_burn_without_comparison = 1;
			break;

		case 'c':
			cart_thorough_compare = 1;
			break;

		case 's':
			cart_trim_allowed = 0;
			break;

		case 'a':
			cart_trim_always = 1;
			break;

		case 'S':
			new_cart_size = optarg;
			break;

		case 'd':
			cart_io_sim++;
			break;

		case 'p':
			mode = MODE_EASYROM_MAP;
			break;

		case 'M':
			mode = MODE_SCANNED_MAP;
			break;

		case 'm':
			mode = MODE_MB_USER;
			multiboot_user_file = optarg;
			break;

		case 'G':
			mode = MODE_GEN_ID;
			break;

		case 'L':
			loader_file = optarg;
			if (strcmp(loader_file, "list") == 0)
				enum_ld = 1;
			break;

		case 'F':
			firmware_file = optarg;
			if (strcmp(firmware_file, "list") == 0)
				enum_fw = 1;
			break;

		case 'B':
			multiboot_file = optarg;
			if (strcmp(multiboot_file, "list") == 0)
				enum_mb = 1;
			break;

		case 'I':
			splash_file = optarg;
			if (strcmp(splash_file, "list") == 0)
				enum_spl = 1;
			break;

		case 'A':
			mode = MODE_EASYROM;
			assert(add_files_number < FILE_NUMBER_MAX);
			add_files[add_files_number++] = optarg;
			break;

		case 'X':
			mode = MODE_EASYROM;
			assert(del_files_number < FILE_NUMBER_MAX);
			del_files[del_files_number++] = optarg;
			break;

		case 'Y':
			mode = MODE_EASYROM;
			create_cart_map = 1;
			break;

		case 'h':
			help(argv[0]);
			exit(1);

			/*
			 * For getopt() to differentiate between missing arguments and
			 * unknown options, a colon must start the option list string
			 * (see above).
			 */

			// Unknown option.
		case '?':
			printerr("Unknown option '-%c'.\n"
				 "Use 'if2a -h' to learn about possible options.\n",
				 optopt);
			exit(1);

			// Missing argument.
		case ':':
			printerr("Missing argument for option '-%c'.\n"
				 "Use 'if2a -h' to learn about possible options.\n",
				 optopt);
			exit(1);
		}
	}
	while (opt != -1);

	// optind now points to the first non-option of ARGV.
	//non_opt_idx = optind;
	non_opt_nb = argc - optind;

	/* 
	 * The reinit() APIs initialize the generic linker structure depending
	 * on the linker type.
	 */
	switch (linker_type)
	{
#if F2AL
	case LINKER_F2A_USB_GBA:
		cart_reinit_f2a_usb();
		break;
#endif
#if F2AW
	case LINKER_F2A_USB_WRITER:
		cart_reinit_f2a_usb_writer();
		break;
#endif
#if F2AP
	case LINKER_F2A_PARALLEL_GBA:
		cart_reinit_f2a_parallel();
		break;
#endif
	default:
		cart_reinit_template();
		break;
	}

	/*
	 * Load default binaries or the ones specified by the options.
	 *
	 * Negative return values indicate a problem (arg == NULL or arg ==
	 * 'list' are not errors).
	 *
	 * Starting to this point, cart info is available : this means cart_exit()
	 * must be used when exiting. However, cart type is not yet available until
	 * autodetection.
	 */
	int fw = cart_select_firmware(firmware_file);
	int mb = cart_select_linker_multiboot(multiboot_file);
	int sp = cart_select_splash(splash_file);

	if (fw < 0 || mb < 0 || sp < 0 || enum_fw || enum_mb || enum_spl)
		cart_exit(1);

	/* 
	 * To allow for enumerating loaders without being connected, we must
	 * have a cart type specified (otherwise, we wait for autodetection)
	 */
	if (enum_ld && cart_type != CART_TYPE_UNDEF)
	{
		cart_select_loader(cart_type, loader_file);
		cart_exit(1);
	}

	/* 
	 * Missing actions. 
	 *
	 * enum_ld is listed here as we have to wait for autodetection to check
	 * whether the given binary is valid (the API needs a cart type),
	 * something that the other select_ APIs can do immediately.
	 *
	 * No need for a special MODE_ENUM as it is only used here.
	 *
	 * The idea is to do as many sanity checks as possible without going
	 * offline. Normally, only tests which require a cart type should wait for
	 * connection/autodetection.
	 */
	if (mode == MODE_UNDEF && !enum_ld)
	{
		if (loader_file)
		{
			//print("Loader but no more action specified, "
			//      "so action set to write loader only.\n");
			mode = MODE_EASYROM;
		}
		else
		{
			printerr("No action (read, write, etc) was specified.\n"
				 "Use 'if2a -h' to learn about possible actions.\n");
			cart_exit(1);
		}
	}

	/*
	 * Non-option arguments.
	 *
	 * Most of the argument job has already been handled by getopt(). We
	 * only need non-option arguments when writing to ROM without EASYROM;).
	 *
	 * On the other hand, we cannot allow any non-option argument if we are
	 * using EASYROM;)
	 */
	if (mode == MODE_WRITE_ROM && non_opt_nb < 1)
	{
		printerr("A filename is missing here.\n");
		cart_exit(1);
	}
	if (mode == MODE_EASYROM && !loader_file && non_opt_nb > 0)
	{
		printerr("ROMs to be added/deleted with EASYROM;) require a -X/-A "
		         "option for each.\n");
		cart_exit(1);
	}

	// Prevent multiple arguments
	if ((mode == MODE_READ_SRAM || mode == MODE_WRITE_SRAM
	     || mode == MODE_READ_DH || mode == MODE_WRITE_DH
	     || mode == MODE_READ_SVD || mode == MODE_WRITE_SVD
	     || mode == MODE_READ_CD || mode == MODE_WRITE_CD)
	    && non_opt_nb > 0)
	{
		printerr("Cannot read/write SRAM, SVD, Die Hard or descriptor "
			 "from/to multiple files.\n");
		cart_exit(1);
	}

	// Cart size settings
	if (new_cart_size != NULL)	// user specified a size
	{
		// Try to convert user's value
		int cart_size_bytes = convsize(new_cart_size);
		cart_size_mbits = cart_size_bytes >> 17;

		if (cart_size_mbits > 0)
		{
			print("Setting cart size to %iMbits / %iKBytes.\n",
			      CART_SIZE_BYTES >> 17, CART_SIZE_BYTES >> 10);
		}
		else
		{
			if (cart_size_bytes > 0)
				printerr("Specified cart size is too small "
					 "(%i bytes).\n", cart_size_bytes);
			else
				printerr("Invalid specified cart size: '%s'.\n",
					 new_cart_size);
			cart_exit(1);
		}
	}

	// Connection. This means parsing the USB bus and uploading firmware.
	if (cart_connect() < 0)
		cart_exit(1);

	// Transfer and launch the multiboot image
	if (cart_check_or_init_linker() < 0)
		cart_exit(1);

	/* 
	 * Cart type and parameters autodetection. We assume the user knows what
	 * he does and we disabled autodetection when a type is specified.
	 */
	if (autodetection)
	{
		int cart_detect_cartsize_mb = 0;
		int cart_detect_write_block_size_log2 = 0;
		cart_rom_block_size_log2 = 0;

		cart_type_e detect_cart_type =
			cart_autodetect(&cart_detect_cartsize_mb,
					&cart_detect_write_block_size_log2,
					&cart_rom_block_size_log2);
		cart_type = detect_cart_type;
		cart_size_mbits = cart_detect_cartsize_mb;
		cart_write_block_size_log2 = cart_detect_write_block_size_log2;

		if (cart_rom_block_size_log2 <= 0)
		{
			printerr("Bad autodetection for rom alignment.\n");
			cart_exit(1);
		}
	}

	/* 
	 * To allow for enumerating loaders without being connected, we must
	 * have a cart type specified.
	 */
	if (enum_ld)
	{
		if (cart_type != CART_TYPE_UNDEF)
			cart_select_loader(cart_type, loader_file);
		else
			printerr("A cart type is needed for loader enumeration.\n"
			         "Use 'if2a -h' to learn about cart types.\n");

		cart_exit(1);
	}

	// Send multiboot
	if (mode == MODE_MB_USER)
	{
		if (cart_user_multiboot(multiboot_user_file) == -1)
		{
			printerr("Cannot send multiboot '%s' file to GBA.\n",
				 multiboot_user_file);
			cart_exit(1);
		}
	}

	/* 
	 * Now is the good moment to select a loader since we have a cart type. 
	 *
	 * This means that tests/operations relying on a cart_type have to be done
	 * after this.
	 */
	if (cart_select_loader(cart_type, loader_file) < 0)
		cart_exit(1);

	// Unsupported features for non Ultras
	if ((mode == MODE_READ_SVD || mode == MODE_WRITE_SVD
	     || mode == MODE_READ_DH || mode == MODE_WRITE_DH)
	    && cart_type != CART_TYPE_F2A_ULTRA)
	{
		printerr("SVD and Content Descriptor operations are only "
			 "supported on Ultra carts.\n");
		cart_exit(1);
	}

	/* 
	 * SRAM bank management (after autodetection)
	 * Defaults is set to 128KB(ultra)|256KB(pro) / no offset (all SRAM).
	 */
	switch (cart_type)
	{
	case CART_TYPE_F2A_PRO:
		sram_size = 262144;	// 256KB for pros
		break;
	case CART_TYPE_F2A_ULTRA:
		sram_size = 131072;	// 128KB for ultra
		break;
	default:
		sram_size = 65536;	// 64KB at least (turbo?)
		printerr("What SRAM size for your yet-unknown cart?\n");
		break;
	}
	if (bank && strcmp(bank, "all") != 0)
	{
		// Not allowed for other modes than SRAM R/W.
		if (!(mode == MODE_READ_SRAM || mode == MODE_WRITE_SRAM))
		{
			printerr("SRAM bank specification not allowed without "
				 "reading/writing to SRAM.\n");
			cart_exit(1);
		}

		// Try to convert given bank
		if (conv_f2apro_bank(bank, &sram_offset, &sram_size) < 0)
		{
			printerr("Bank %s does not exist.\n", bank);
			cart_exit(1);
		}
	}

	/* 
	 * Banks not allowed for Ultra carts. Test is here so that other generic
	 * SRAM verifications can be done offline.
	 */
	if (cart_type == CART_TYPE_F2A_ULTRA && bank)
	{
		printerr("No SRAM bank specification is allowed with Ultra carts.\n"
		         "Operations are always done on full SRAM.\n");
		cart_exit(1);
	}

	// Show cart map : scanning method
	if (mode == MODE_SCANNED_MAP)
	{
		if (display_scanned_cart_map() == -1)
			cart_exit(1);
	}

	// Show cart map : EASYROM;)
	if (mode == MODE_EASYROM_MAP)
	{
		if (load_cart_map() >= 0)
		{
			display_cart_map();
			reset_cart_map();
		}
		else
		{
			printerr("The cart map can be generated with the -Y option.\n");
			cart_exit(1);
		}
	}

	// Write SRAM
	if (mode == MODE_WRITE_SRAM)
	{
		if (cart_verbose)
			print("Uploading %iKB to SRAM (offsets 0x%x -> 0x%x) from file %s.\n",
			      (int)(sram_size / 1024),
			      GBA_SRAM + sram_offset,
			      GBA_SRAM + sram_offset + sram_size - 1,
			      sram_file);

		assert(!(mode == MODE_WRITE_CD || mode == MODE_READ_CD));
		if (cart_file2sram(sram_file, sram_offset, sram_size) == -1)
		{
			printerr("Cannot read GBA SRAM.\n");
			cart_exit(1);
		}
	}

	// Read SRAM
	if (mode == MODE_READ_SRAM)
	{
		if (cart_verbose)
			print("Downloading %iK from SRAM (offsets 0x%x -> 0x%x) to file %s.\n",
			      (int)(sram_size / 1024),
			      GBA_SRAM + sram_offset,
			      GBA_SRAM + sram_offset + sram_size - 1,
			      sram_file);

		if (cart_read_mem_to_file
		    (sram_file, GBA_SRAM + sram_offset, sram_size,
		     READ_ONCE) == -1)
		{
			printerr("Cannot read GBA SRAM.\n");
			cart_exit(1);
		}
	}

	// Write ROMs
	if (mode == MODE_WRITE_ROM
	    && auto_loadandburn_rom(cart_type, cart_use_loader, clean_cart,
				    argc - optind, argv + optind) < 0)
		cart_exit(1);

	// Read ROMs
	if (mode == MODE_READ_ROM)
		auto_readandsave_rom(argc - optind, argv + optind);

	// Write SVD
	if (mode == MODE_WRITE_SVD)
	{
		if (cart_verbose)
			print("Writing 320K of SVD data from file %s.\n",
			      argv[optind]);
		f2au_SVD_from_file(argv[optind]);
	}

	// Read SVD
	if (mode == MODE_READ_SVD)
	{
		if (cart_verbose)
			print("Reading 320K of SVD data to file %s.\n",
			      svd_file);
		if (f2au_SVD_to_file(svd_file) == -1)
		{
			printerr("Cannot read cart's SVD.\n");
			cart_exit(1);
		}
	}

	// Write DH
	if (mode == MODE_WRITE_DH)
	{
		if (cart_verbose)
			print("Writing XXK of Die Hard data from file %s.\n",
			      argv[optind]);
		if (f2au_DH_from_file(argv[optind]) == -1)
		{
			printerr("Cannot write XXK of Die Hard data to cart.\n");
			cart_exit(1);
		}
	}

	// Read DH
	if (mode == MODE_READ_DH)
	{
		if (cart_verbose)
			print("Reading XXK of Die Hard data to file %s.\n",
			      argv[optind]);
		if (f2au_DH_to_file(argv[optind]) == -1)
		{
			printerr("Cannot read cart's XXK of Die Hard data.\n");
			cart_exit(1);
		}
	}

	// Write CD
	if (mode == MODE_WRITE_CD)
	{
		if (cart_verbose)
			print("Writing descriptor.\n");
		if (f2au_loadandwrite_sram(argv[optind], F2AU_CD_BASE, SIZE_64K)
		    == -1)
		{
			printerr("Cannot write cart's descriptor.\n");
			cart_exit(1);
		}
	}

	// Read CD
	if (mode == MODE_READ_CD)
	{
		content_desc *desc;

		if ((desc = f2au_CD_read()) == NULL)
			cart_exit(1);

		if (f2au_CD_check(desc) == -1)
		{
			printerr("Content descriptor seems invalid or corrupted. Exiting.\n");
			cart_exit(1);
		}

		buffer_to_file(argv[optind], (unsigned char *)&desc,
			       sizeof(desc));
	}

	// Generate GameID
	if (mode == MODE_GEN_ID)
	{
		int game_id = 0;
		f2au_GameID_gen(argv[optind], &game_id);
		print("GameID for file %s is: %8X.\n", argv[optind], game_id);
	}

	// EASYROM;)
	if (mode == MODE_EASYROM)
	{
		if (create_cart_map)
			brand_new_empty_cart_map();
		else if (load_cart_map() < 0)
		{
			printerr("The cart map can be generated with the -Y option.\n");
			cart_exit(1);
		}

		if (del_files_number)
			cart_map_mark_for_remove(del_files, del_files_number);

		if (loader_file)
			cart_map_replace_loader(&loader);

		if ((del_files_number + add_files_number) == 0
		    || cart_verbose > 0)
		{
			if (del_files_number)
				print("After removing files, ");
			display_cart_map();
		}

		if (del_files_number || add_files_number || loader_file)
		{
			if (cart_map_build_hole() < 0)
			{
				reset_cart_map();
				cart_exit(1);
			}
			if (cart_verbose > 0)
				display_cart_map_hole();

			if (add_files_number)
			{
				if (cart_map_find_best_insertion_for_files
				    (add_files, add_files_number) < 0)
				{
					reset_cart_map();
					cart_exit(1);
				}
				if (cart_verbose > 0)
					cart_map_file_display_best_score();
			}

			if (cart_map_process_changes() < 0)
			{
				reset_cart_map();
				cart_exit(1);
			}
		}

		reset_cart_map();
	}

	// Cleanup after every action performed
	cart_exit(0);

	return 0;		// make compiler happy
}
