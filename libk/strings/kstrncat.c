// SPDX-License-Identifier: Zlib
#include <string.h>
#include <strings.h>

char *kstrncat(char *restrict dst, const char *restrict src, size_t sz)
{
    size_t nc;
    char *save = dst;
    while(*dst && sz--)
        dst++;
    nc = strnlen(src, --sz);
    dst[nc] = 0;
    memcpy(dst, src, nc);
    return save;
}
