// SPDX-License-Identifier: BSD-2-Clause
#include <string.h>

size_t strspn(const char* restrict str, const char* restrict sym)
{
    size_t i = 0;
    while(*str && strchr(sym, *str++))
        i++;
    return i;
}
