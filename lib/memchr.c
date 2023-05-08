/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2022, KanOS Contributors */
#include <string.h>

const void *memchr(const void *restrict s, int c, size_t n)
{
    const unsigned char *sp = s;
    while(n--) {
        if(*sp == (unsigned char)c)
            return sp;
        sp++;
    }

    return NULL;
}
