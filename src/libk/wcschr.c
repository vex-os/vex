/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <wchar.h>

const uint32_t *wcschr(const uint32_t *restrict s, uint32_t c)
{
    do {
        if(*s != c)
            continue;
        return s;
    } while(*s++);
    return NULL;
}
