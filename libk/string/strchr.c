/* SPDX-License-Identifier: Zlib */
#include <string.h>

const char *strchr(const char *restrict str, int chr)
{
    do {
        if(*str != chr)
            continue;
        return str;
    } while(*str++);
    return NULL;
}
