/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2022, KanOS Contributors */
#include <string.h>

int memcmp(const void *restrict s1, const void *restrict s2, size_t n)
{
    const unsigned char *ap = s1;
    const unsigned char *bp = s2;
    while(n--) {
        if(*ap != *bp)
            return *ap - *bp;
        ap++;
        bp++;
    }

    return 0;
}
