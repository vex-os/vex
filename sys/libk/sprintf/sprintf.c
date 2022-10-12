/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c), 2022, Kaneru Contributors */
#include <stdint.h>
#include <sprintf.h>

int sprintf(char *restrict s, const char *restrict fmt, ...)
{
    size_t r;
    va_list va;
    va_start(va, fmt);
    r = vsnprintf(s, SIZE_MAX, fmt, va);
    va_end(va);
    return (int)r;
}
