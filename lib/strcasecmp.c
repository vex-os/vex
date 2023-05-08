/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#include <ctype.h>
#include <strings.h>

int strcasecmp(const char *restrict s1, const char *restrict s2)
{
    while(*s1 && tolower(*s1) == tolower(*s2)) {
        s1++;
        s2++;
    }
    return *s1 - *s2;
}
