/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <sprintf.h>
#include <stdint.h>

size_t vsprintf(char *restrict s, const char *restrict fmt, va_list ap)
{
    return vsnprintf(s, SIZE_MAX, fmt, ap);
}
