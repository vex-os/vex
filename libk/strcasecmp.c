/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2023, VX/sys Contributors */
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
