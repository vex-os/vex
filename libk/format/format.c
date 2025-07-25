// SPDX-License-Identifier: BSD-2-Clause
#include <format.h>

int format(format_func_t func, void* restrict arg, const char* restrict fmt, ...)
{
    int r;
    va_list ap;
    va_start(ap, fmt);
    r = vformat(func, arg, fmt, ap);
    va_end(ap);
    return r;
}
