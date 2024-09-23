/* SPDX-License-Identifier: BSD-2-Clause */
#include <string.h>

char *strncat(char *restrict dst, const char *restrict src, size_t sz)
{
    char *save = dst;
    while(*dst)
        dst++;
    while(sz && (*dst++ = *src++))
        sz--;
    if(sz == 0)
        *dst = 0;
    return save;
}
