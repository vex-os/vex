/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <wchar.h>

const wchar_t *wcspbrk(const wchar_t *restrict s1, const wchar_t *restrict s2)
{
    while(*s1) {
        if(wcschr(s2, *s1))
            return s1;
        s1++;
    }

    return NULL;
}
