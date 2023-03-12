/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <stddef.h>
#include <stdlib.h>

long atol(const char *restrict s)
{
    return strtol(s, NULL, 10);
}
