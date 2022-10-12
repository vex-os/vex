/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c), 2022, Kaneru Contributors */
#include <string.h>

char *strncpy(char *restrict s1, const char *restrict s2, size_t n)
{
    char *save = s1;
    while(n && (*s1++ = *s2++))
        n--;
    while(n-- > 1)
        *s1++ = 0;
    return save;
}
