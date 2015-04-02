
#ifndef __F2AMISC_H__
#define __F2AMISC_H__

#include "../../libf2a.h"

int conv_f2apro_bank		(const char* bank, int* offset, int* size);
int select_f2a_loader		(cart_type_e cart_type, const char* loader_file);

#endif // __F2AMISC_H__
