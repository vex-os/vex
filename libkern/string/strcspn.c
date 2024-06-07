/* SPDX-License-Identifier: GPL-2.0-only */
#include <string.h>

size_t strcspn(const char *restrict str, const char *restrict sym)
{
    size_t i = 0;
    while(*str) {
        if(strchr(sym, *str))
            return i;
        str++;
        i++;
    }

    return i;
}
