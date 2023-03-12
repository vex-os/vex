/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <string.h>

void *memcpy(void *restrict s1, const void *restrict s2, size_t n)
{
    unsigned char *dp = s1;
    const unsigned char *sp = s2;
    while(n--)
        *dp++ = *sp++;
    return s1;
}
