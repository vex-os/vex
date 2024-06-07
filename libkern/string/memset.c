/* SPDX-License-Identifier: GPL-2.0-only */
#include <string.h>

void *memset(void *restrict dst, int chr, size_t sz)
{
    unsigned char *sp = dst;
    while(sz--)
        *sp++ = (unsigned char)(chr);
    return dst;
}
