/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2022, KanOS Contributors */
#include <string.h>

int strcmp(const char *restrict s1, const char *restrict s2)
{
    while(*s1 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return *s1 - *s2;
}
