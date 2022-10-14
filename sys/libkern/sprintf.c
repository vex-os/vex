/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#include <sprintf.h>
#include <stdint.h>

int sprintf(char *restrict s, const char *restrict fmt, ...)
{
    int r;
    va_list ap;
    va_start(ap, fmt);
    r = vsnprintf(s, SIZE_MAX, fmt, ap);
    va_end(ap);
    return r;
}
