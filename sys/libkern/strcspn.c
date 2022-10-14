/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#include <string.h>

size_t strcspn(const char *s1, const char *s2)
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
