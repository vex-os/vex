/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <string.h>

int strncmp(const char *restrict s1, const char *restrict s2, size_t n)
{
    while(*s1 && *s1 == *s2 && n--) {
        s1++;
        s2++;
    }
    return *s1 - *s2;
}
