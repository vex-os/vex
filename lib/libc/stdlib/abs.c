/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#include <limits.h>
#include <stdlib.h>

int abs(int j)
{
    int mask = j >> (sizeof(int) * CHAR_BIT - 1);
    return (j + mask) ^ mask;
}
