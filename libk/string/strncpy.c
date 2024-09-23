/* SPDX-License-Identifier: BSD-2-Clause */
#include <string.h>

char *strncpy(char *restrict dst, const char *restrict src, size_t sz)
{
    char *save = dst;
    while(sz && (*dst++ = *src++))
        sz--;
    while(sz-- > 1)
        *dst++ = 0;
    return save;
}
