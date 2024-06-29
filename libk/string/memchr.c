// SPDX-License-Identifier: Zlib
#include <string.h>

const void *memchr(const void *restrict buf, int chr, size_t sz)
{
    const unsigned char *sp = buf;

    while(sz--) {
        if(*sp == chr)
            return sp;
        sp++;
    }

    return NULL;
}
