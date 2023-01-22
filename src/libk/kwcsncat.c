/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <string.h>
#include <wchar.h>

wchar_t *kwcsncat(wchar_t *restrict s1, const wchar_t *restrict s2, size_t n)
{
    size_t nc;
    wchar_t *save = s1;
    while(*s1 && n--)
        s1++;
    nc = wcsnlen(s2, --n);
    s1[nc] = 0;
    memcpy(s1, s2, nc);
    return save;
}
