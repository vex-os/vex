/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2024, VX/sys Contributors */
#include <stddef.h>
#include <stdlib.h>

long atol(const char *restrict s)
{
    return strtol(s, NULL, 10);
}
