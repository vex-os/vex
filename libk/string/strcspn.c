/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2024, VX/sys Contributors */
#include <string.h>

size_t strcspn(const char *restrict s1, const char *restrict s2)
{
    size_t i = 0;
    while(*s1) {
        if(strchr(s2, *s1))
            return i;
        s1++;
        i++;
    }

    return i;
}
