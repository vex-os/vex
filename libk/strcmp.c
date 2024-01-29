/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2022, VX/sys Contributors */
#include <string.h>

int strcmp(const char *restrict s1, const char *restrict s2)
{
    while(*s1 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return *s1 - *s2;
}
