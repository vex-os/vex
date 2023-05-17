/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2022, KanOS Contributors */
#include <string.h>
#include <strings.h>

/* The standard function, strncat, uses the size
 * limiter as the size of source, not destination.
 * Because no one would EVER need to specify destination size!
 * Yes, this comment is copied from kstrncpy... Too bad! */
char *kstrncat(char *restrict s1, const char *restrict s2, size_t n)
{
    size_t nc;
    char *save = s1;
    while(*s1 && n--)
        s1++;
    nc = strnlen(s2, --n);
    s1[nc] = 0;
    memcpy(s1, s2, nc);
    return save;
}
