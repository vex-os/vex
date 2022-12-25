/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#include <string.h>

char *strcat(char *restrict s1, const char *restrict s2)
{
    char *save = s1;
    if(*s1)
        while(*++s1);
    while((*s1++ = *s2++));
    return save;
}
