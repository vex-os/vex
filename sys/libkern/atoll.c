/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#include <stddef.h>
#include <stdlib.h>

long long int atoll(const char *restrict s)
{
    return strtoll(s, NULL, 10);
}
