/* SPDX-License-Identifier: GPL-2.0-only */
#include <string.h>

char *strcpy(char *restrict dst, const char *restrict src)
{
    char *save = dst;
    while((*dst++ = *src++));
    return save;
}
