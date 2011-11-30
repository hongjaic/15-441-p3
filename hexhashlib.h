#ifndef _HEXHASHLIB_H_
#define _HEXHASHLIB_H_

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

int int_str_comp(uint32_t ui, char *s, int index, int size);
int hash_to_int(char *hash, int index, int size);

#endif /* _HESHASHLIB_H_ */
