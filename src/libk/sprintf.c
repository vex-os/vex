/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <sprintf.h>
#include <stdint.h>

size_t sprintf(char *restrict s, const char *restrict fmt, ...)
{
    size_t r;
    va_list ap;
    va_start(ap, fmt);
    r = vsnprintf(s, SIZE_MAX, fmt, ap);
    va_end(ap);
    return r;
}
