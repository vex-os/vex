/* SPDX-License-Identifier: GPL-2.0-only */
#include <string.h>

const void *memchr(const void *restrict buf, int chr, size_t sz)
{
    const unsigned char *sp = buf;

    while(sz--) {
        if(*sp == (unsigned char)(chr))
            return sp;
        sp++;
    }

    return NULL;
}
