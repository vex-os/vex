/* SPDX-License-Identifier: Zlib */
#include <string.h>

char *strcat(char *restrict dst, const char *restrict src)
{
    char *save = dst;
    if(*dst)
        while(*++dst);
    while((*dst++ = *src++));
    return save;
}
