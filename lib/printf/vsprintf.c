/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#include <printf.h>
#include <stdint.h>

int vsprintf(char *restrict s, const char *restrict fmt, va_list ap)
{
    return vsnprintf(s, SIZE_MAX, fmt, ap);
}
