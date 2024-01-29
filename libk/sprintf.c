/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2023, VX/sys Contributors */
#include <stdint.h>
#include <stdio.h>

int sprintf(char *restrict s, const char *restrict fmt, ...)
{
    int r;
    va_list ap;
    va_start(ap, fmt);
    r = vsnprintf(s, SIZE_MAX, fmt, ap);
    va_end(ap);
    return r;
}
