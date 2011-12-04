/**
 * CS 15-441 Computer Networks
 * Project: Congestion Control with BitTorrent
 *
 * @file    hexhashlib.c
 * @author  Hong Jai Cho <hongjaic>, Raul Gonzalez <rggonzal>
 */


#include "hexhashlib.h"


/**
 * int_str_comp -
 * A function that helps compare hexadecimal string hash values to integer
 * values.
 *
 * @param ui    integer format of hash
 * @param s     string format of hash
 * @param index index to start the comparison
 * @param size  number of byes in s starting from index to use for comparison
 * @return  0 if match, otherwise -1
 */
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


/**
 * hash_to_int -
 * A function that converts hexadecimal string hash values to integers
 *
 * @param hash  string format of hash
 * @param index index to start conversion from
 * @param size  number of bytes to convert
 * @return  the resulting integer value of hex string hash
 */
int hash_to_int(char *hash, int index, int size)
{
    char str[size + 1];
    uint32_t result;

    memcpy(str, hash + index, size);
    str[size] = '\0';

    result = a64l(str);

    return result;
}
