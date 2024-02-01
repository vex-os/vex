/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2024, VX/sys Contributors */
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
