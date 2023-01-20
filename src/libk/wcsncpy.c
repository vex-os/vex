/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <wchar.h>

uint32_t *wcsncpy(uint32_t *restrict s1, const uint32_t *restrict s2, size_t n)
{
    uint32_t *save = s1;
    while(n && (*s1++ = *s2++))
        n--;
    while(n-- > 1)
        *s1++ = 0;
    return save;
}