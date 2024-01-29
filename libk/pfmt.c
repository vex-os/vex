/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2023, VX/sys Contributors */
#include <pfmt.h>

int pfmt(pfmt_func_t func, void *restrict arg, const char *restrict fmt, ...)
{
    int r;
    va_list ap;
    va_start(ap, fmt);
    r = vpfmt(func, arg, fmt, ap);
    va_end(ap);
    return r;
}
