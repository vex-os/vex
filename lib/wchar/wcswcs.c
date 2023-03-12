/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <wchar.h>

const wchar_t *wcswcs(const wchar_t *restrict s1, const wchar_t *restrict s2)
{
    const wchar_t *ap = s1;
    const wchar_t *bp;

    while(*s1) {
        bp = s2;
        while (*bp && (*ap == *bp)) {
            ++ap;
            ++bp;
        }
        if (!*bp)
            return s1;
        ++s1;
        ap = s1;
    }

    return NULL;
}
