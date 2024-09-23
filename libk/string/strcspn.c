/* SPDX-License-Identifier: BSD-2-Clause */
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
