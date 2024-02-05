/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2024, VX/sys Contributors */
#include <string.h>

const char *strpbrk(const char *restrict s1, const char *restrict s2)
{
    while(*s1) {
        if(strchr(s2, *s1))
            return s1;
        s1++;
    }

    return NULL;
}
