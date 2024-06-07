/* SPDX-License-Identifier: GPL-2.0-only */
#include <string.h>

const char *strrchr(const char *restrict str, int chr)
{
    const char *cp = NULL;
    do {
        if(*str == (char)(chr))
            cp = str;
    } while(*str++);
    return cp;
}
