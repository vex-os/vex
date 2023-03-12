/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <string.h>

char *strncat(char *restrict s1, const char *restrict s2, size_t n)
{
    char *save = s1;
    while(*s1)
        s1++;
    while(n && (*s1++ = *s2++))
        n--;
    if(n == 0)
        *s1 = 0;
    return save;
}
