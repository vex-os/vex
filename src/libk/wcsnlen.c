/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <wchar.h>

size_t wcsnlen(const uint32_t *restrict s, size_t n)
{
    size_t i;
    for(i = 0; *s++ && i < n; i++);
    return i;
}
