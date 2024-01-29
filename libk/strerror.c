/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2022, VX/sys Contributors */
#include <string.h>

char *strerror(int errnum)
{
    static char buffer[128] = { 0 };
    strerror_r(errnum, buffer, sizeof(buffer));
    return buffer;
}
