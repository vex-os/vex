/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2023, VX/sys Contributors */
#include <stdint.h>
#include <stdio.h>

int vsprintf(char *restrict s, const char *restrict fmt, va_list ap)
{
    return vsnprintf(s, SIZE_MAX, fmt, ap);
}
