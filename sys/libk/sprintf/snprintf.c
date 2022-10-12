/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c), 2022, Kaneru Contributors */
#include <sprintf.h>

int snprintf(char *restrict s, size_t n, const char *restrict fmt, ...)
{
    size_t r;
    va_list va;
    va_start(va, fmt);
    r = vsnprintf(s, n, fmt, va);
    va_end(va);
    return (int)r;
}
