/* 
 * Based on f2a by Ulrich Hecht <uli@emulinks.de>
 * if2a by D. Gauchard <deyv@free.fr>
 * F2A Ultra support by Vincent Rubiolo <vincent.rubiolo@free.fr>
 * Licensed under the terms of the GNU Public License version 2
 */

#include <string.h>
#include <ctype.h>

#include "../../libf2a.h"
#include "../../binware.h"
#include "../../cartutils.h"
#include "f2amisc.h"

int conv_f2apro_bank (const char* bank, int* offset, int* size)
{
	int len = strlen(bank);

    /* 
     * Bank numbers higher than 4 are incorrect. So are banks wih more than 3
     * letters.
     */ 
	if (len > 3 || (bank[0] - '0') > 4)
		return -1;

	*offset = (bank[0] - '1') << 16;
    
	if (bank[1])
	{
	    *offset += (tolower(bank[1]) - 'a') << 15;
			
		if (bank[2])
		{
       	    *offset += (bank[2] - '1') << 13;
            *size = 1 << 13;
        }
        else
            *size = 1 << 15;
    }
    else
        *size = 1 << 16;

	if (cart_verbose && bank[0])
		print("Bank '%s' is SRAM is (offset=0x%x/%ibytes size=0x%x/%ibytes)\n",
		bank, *offset, *offset, *size, *size);
	return 0;
}

int select_f2a_loader (cart_type_e cart_type, const char* loader_file)
{
	int result = -1;

	if (loader.size == 0)
	{
		switch (cart_type)
		{
			case CART_TYPE_F2A_PRO:
				result = binware_load(&loader, binware_f2a_loader_pro, loader_file, "F2A-PRO-loader");
				break;
			case CART_TYPE_F2A_ULTRA:
				result = binware_load(&loader, binware_f2a_loader_ultra, loader_file, "F2A-Ultra-loader");
				break;
			case CART_TYPE_F2A_TURBO:
				printerr("Looking for TURBO cart testers - assuming PRO -\n"
                                         "Please contact us at if2a@ml.free.fr\n");
				result = binware_load(&loader, binware_f2a_loader_pro, loader_file, "F2A-Turbo-loader");
				break;
			default:
				break; // result will be -1
		}
		return result;
	}
	else
		return 0;   // already a loader
}

