/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <string.h>

char *strcpy(char *restrict s1, const char *restrict s2)
{
    char *save = s1;
    while((*s1++ = *s2++));
    return save;
}
