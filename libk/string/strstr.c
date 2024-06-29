// SPDX-License-Identifier: Zlib
#include <string.h>

const char *strstr(const char *restrict str, const char *restrict substr)
{
    const char *ap = str;
    const char *bp;

    while(*str) {
        bp = substr;

        while (*bp && (*ap == *bp)) {
            ++ap;
            ++bp;
        }

        if (!*bp)
            return str;
        ap = ++str;
    }

    return NULL;
}
