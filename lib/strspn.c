/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2022, KanOS Contributors */
#include <string.h>

size_t strspn(const char *restrict s1, const char *restrict s2)
{
    size_t i = 0;
    while(*s1 && strchr(s2, *s1++))
        i++;
    return i;
}
