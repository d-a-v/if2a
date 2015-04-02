
#include <stdlib.h>

#include "libf2a.h"

int main (void)
{
	cart_verbose = 2;
	cart_reinit_efa();

	if (cart_select_firmware(NULL /* default */) < 0)
		cart_exit(1);

	if (cart_connect() < 0)
	{
		printerr("Could not initialize linker\n");
		cart_exit(1);
	}
	
	print("ok\n");
	cart_exit(0);
	return 0;
}
