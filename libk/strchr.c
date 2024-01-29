/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2022, VX/sys Contributors */
#include <string.h>

const char *strchr(const char *restrict s, int c)
{
    do {
        if(*s != (char)c)
            continue;
        return s;
    } while(*s++);
    return NULL;
}
