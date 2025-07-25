// SPDX-License-Identifier: BSD-2-Clause
#include <string.h>

int memcmp(const void* restrict ba, const void* restrict bb, size_t sz)
{
    const unsigned char* ap = ba;
    const unsigned char* bp = bb;
    while(sz--) {
        if(*ap != *bp)
            return *ap - *bp;
        ap++;
        bp++;
    }

    return 0;
}
