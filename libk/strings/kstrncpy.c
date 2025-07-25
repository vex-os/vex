// SPDX-License-Identifier: BSD-2-Clause
#include <strings.h>

char* kstrncpy(char* restrict dst, const char* restrict src, size_t sz)
{
    char* save = dst;
    while(*src && sz--)
        *dst++ = *src++;
    if(sz)
        *dst = 0;
    return save;
}
