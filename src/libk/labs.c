/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <limits.h>
#include <stdlib.h>

long labs(long j)
{
    return (j >= 0) ? j : -j;
}
