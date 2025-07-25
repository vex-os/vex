// SPDX-License-Identifier: BSD-2-Clause
#include <string.h>

char* strcpy(char* restrict dst, const char* restrict src)
{
    char* save = dst;
    while((*dst++ = *src++))
        ;
    return save;
}
