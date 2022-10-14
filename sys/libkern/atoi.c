/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#include <stddef.h>
#include <stdlib.h>

int atoi(const char *restrict s)
{
    return (int)strtol(s, NULL, 10);
}
