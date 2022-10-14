/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#include <limits.h>
#include <stdlib.h>

long long int llabs(long long int j)
{
    long long int mask = j >> (sizeof(long long int) * CHAR_BIT - 1);
    return (j + mask) ^ mask;
}
