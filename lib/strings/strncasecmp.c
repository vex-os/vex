/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#include <ctype.h>
#include <strings.h>

int strncasecmp(const char *restrict s1, const char *restrict s2, size_t n)
{
    while(*s1 && tolower(*s1) == tolower(*s2) && n--) {
        s1++;
        s2++;
    }
    return *s1 - *s2;
}
