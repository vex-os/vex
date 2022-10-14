/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#include <string.h>

char *strncpy_k(char *restrict s1, const char *restrict s2, size_t n)
{
    char *save = s1;
    while(*s2 && n--)
        *s1++ = *s2++;
    if(n)
        *s1 = 0;
    return save;
}
