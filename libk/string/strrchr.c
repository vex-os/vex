// SPDX-License-Identifier: Zlib
#include <string.h>

const char *strrchr(const char *restrict str, int chr)
{
    const char *cp = NULL;
    do {
        if(*str == chr)
            cp = str;
    } while(*str++);
    return cp;
}
