/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#include <limits.h>
#include <stdlib.h>

long int labs(long int j)
{
    long int mask = j >> (sizeof(long int) * CHAR_BIT - 1);
    return (j + mask) ^ mask;
}
