/* SPDX-License-Identifier: GPL-2.0-only */
#include <string.h>

const char *strpbrk(const char *restrict str, const char *restrict sym)
{
    while(*str) {
        if(strchr(sym, *str))
            return str;
        str++;
    }

    return NULL;
}
