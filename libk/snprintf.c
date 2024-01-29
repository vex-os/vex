/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2023, VX/sys Contributors */
#include <stdio.h>

int snprintf(char *restrict s, size_t n, const char *restrict fmt, ...)
{
    int r;
    va_list ap;
    va_start(ap, fmt);
    r = vsnprintf(s, n, fmt, ap);
    va_end(ap);
    return r;
}
