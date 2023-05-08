/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#include <printf.h>

int fcprintf(fcprintf_func_t func, void *restrict arg, const char *restrict fmt, ...)
{
    int r;
    va_list ap;
    va_start(ap, fmt);
    r = fcvprintf(func, arg, fmt, ap);
    va_end(ap);
    return r;
}
