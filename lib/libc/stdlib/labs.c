/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#include <limits.h>
#include <stdlib.h>

long int labs(long int j)
{
    return (j >= 0) ? j : -j;
}
