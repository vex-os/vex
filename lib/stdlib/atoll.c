/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <stddef.h>
#include <stdlib.h>

long long atoll(const char *restrict s)
{
    return strtoll(s, NULL, 10);
}
