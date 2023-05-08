/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2022, KanOS Contributors */
#include <string.h>

char *strerror(int errnum)
{
    static char buffer[128] = { 0 };
    strerror_r(errnum, buffer, sizeof(buffer));
    return buffer;
}
