/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <wchar.h>

const uint32_t *wcsrchr(const uint32_t *restrict s, uint32_t c)
{
    const uint32_t *cp = NULL;
    do {
        if(*s == c)
            cp = s;
    } while(*s++);
    return cp;
}
