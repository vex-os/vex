/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2022, KanOS Contributors */
#include <string.h>

size_t strnlen(const char *restrict s, size_t n)
{
    size_t i;
    for(i = 0; *s++ && i < n; i++);
    return i;
}
