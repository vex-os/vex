/* SPDX-License-Identifier: GPL-2.0-only */
#include <sprintf.h>
#include <stdint.h>

int sprintf(char *restrict str, const char *restrict fmt, ...)
{
    int r;
    va_list ap;
    va_start(ap, fmt);
    r = vsnprintf(str, SIZE_MAX, fmt, ap);
    va_end(ap);
    return r;
}
