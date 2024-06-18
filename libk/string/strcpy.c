/* SPDX-License-Identifier: Zlib */
#include <string.h>

char *strcpy(char *restrict dst, const char *restrict src)
{
    char *save = dst;
    while((*dst++ = *src++));
    return save;
}
