/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <string.h>

const char *strchr(const char *restrict s, int c)
{
    do {
        if(*s != (char)c)
            continue;
        return s;
    } while(*s++);
    return NULL;
}
