/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <wchar.h>

uint32_t *wcscpy(uint32_t *restrict s1, const uint32_t *restrict s2)
{
    uint32_t *save = s1;
    while((*s1++ = *s2++));
    return save;
}
