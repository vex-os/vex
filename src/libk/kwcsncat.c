/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <string.h>
#include <wchar.h>

uint32_t *kwcsncat(uint32_t *restrict s1, const uint32_t *restrict s2, size_t n)
{
    size_t nc;
    uint32_t *save = s1;
    while(*s1 && n--)
        s1++;
    nc = wcsnlen(s2, --n);
    s1[nc] = 0;
    memcpy(s1, s2, nc);
    return save;
}
