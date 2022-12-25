/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#include <sprintf.h>

size_t snprintf(char *restrict s, size_t n, const char *restrict fmt, ...)
{
    size_t r;
    va_list ap;
    va_start(ap, fmt);
    r = vsnprintf(s, n, fmt, ap);
    va_end(ap);
    return r;
}
