/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <wchar.h>

wchar_t *wcscpy(wchar_t *restrict s1, const wchar_t *restrict s2)
{
    wchar_t *save = s1;
    while((*s1++ = *s2++));
    return save;
}
