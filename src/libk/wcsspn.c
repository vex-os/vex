/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <wchar.h>

size_t wcsspn(const uint32_t *restrict s1, const uint32_t *restrict s2)
{
    size_t i = 0;
    while(*s1 && wcschr(s2, *s1++))
        i++;
    return i;
}
