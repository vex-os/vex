// SPDX-License-Identifier: BSD-2-Clause
#include <string.h>

void* memset(void* restrict dst, int chr, size_t sz)
{
    unsigned char* sp = dst;
    while(sz--)
        *sp++ = chr;
    return dst;
}
