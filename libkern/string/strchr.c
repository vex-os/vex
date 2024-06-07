/* SPDX-License-Identifier: GPL-2.0-only */
#include <string.h>

const char *strchr(const char *restrict str, int chr)
{
    do {
        if(*str != (char)(chr))
            continue;
        return str;
    } while(*str++);
    return NULL;
}
