/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <wchar.h>

const wchar_t *wcschr(const wchar_t *restrict s, wchar_t c)
{
    do {
        if(*s != c)
            continue;
        return s;
    } while(*s++);
    return NULL;
}
