/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <string.h>

const char *strrchr(const char *restrict s, int c)
{
    const char *cp = NULL;
    do {
        if(*s == c)
            cp = s;
    } while(*s++);
    return cp;
}
