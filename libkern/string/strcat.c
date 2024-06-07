/* SPDX-License-Identifier: GPL-2.0-only */
#include <string.h>

char *strcat(char *restrict dst, const char *restrict src)
{
    char *save = dst;
    if(*dst)
        while(*++dst);
    while((*dst++ = *src++));
    return save;
}
