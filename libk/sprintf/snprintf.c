/* SPDX-License-Identifier: Zlib */
#include <sprintf.h>

int snprintf(char *restrict str, size_t sz, const char *restrict fmt, ...)
{
    int r;
    va_list ap;
    va_start(ap, fmt);
    r = vsnprintf(str, sz, fmt, ap);
    va_end(ap);
    return r;
}
