/* SPDX-License-Identifier: BSD-2-Clause */
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
