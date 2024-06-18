/* SPDX-License-Identifier: Zlib */
#include <sprintf.h>
#include <stdint.h>

int vsprintf(char *restrict str, const char *restrict fmt, va_list ap)
{
    return vsnprintf(str, SIZE_MAX, fmt, ap);
}
