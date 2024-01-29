/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2022, VX/sys Contributors */
#include <string.h>

const char *strrchr(const char *restrict s, int c)
{
    const char *cp = NULL;
    do {
        if(*s == (char)c)
            cp = s;
    } while(*s++);
    return cp;
}
