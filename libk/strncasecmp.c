/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2023, VX/sys Contributors */
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
