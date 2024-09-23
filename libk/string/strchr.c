/* SPDX-License-Identifier: BSD-2-Clause */
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
