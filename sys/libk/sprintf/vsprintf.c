/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c), 2022, Kaneru Contributors */
#include <stdint.h>
#include <sprintf.h>

int vsprintf(char *restrict s, const char *restrict fmt, va_list va)
{
    return vsnprintf(s, SIZE_MAX, fmt, va);
}
