/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <wchar.h>

size_t wcslen(const wchar_t *restrict s)
{
    size_t i;
    for(i = 0; *s++; i++);
    return i;
}
