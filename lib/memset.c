/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2024, VX/sys Contributors */
#include <string.h>

void *memset(void *restrict s, int c, size_t n)
{
    unsigned char *sp = s;
    while(n--)
        *sp++ = (unsigned char)c;
    return s;
}
