#include "hexhashlib.h"

int int_str_comp(uint32_t ui, char *s, int index, int size)
{
    char str[size + 1];
    uint32_t sinint;
    
    memcpy(str, s + index, size);
    str[size] = '\0';

    sinint = a64l(str);

    if (ui == sinint)
    {
        return 0;
    }

    return -1;
}

int hash_to_int(char *hash, int index, int size)
{
    char str[size + 1];
    uint32_t result;

    memcpy(str, hash + index, size);
    str[size] = '\0';

    result = a64l(str);

    return result;
}
