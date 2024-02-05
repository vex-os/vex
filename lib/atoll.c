/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2024, VX/sys Contributors */
#include <stddef.h>
#include <stdlib.h>

long long atoll(const char *restrict s)
{
    return strtoll(s, NULL, 10);
}
