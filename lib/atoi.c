/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2024, VX/sys Contributors */
#include <stddef.h>
#include <stdlib.h>

int atoi(const char *restrict s)
{
    return (int)strtol(s, NULL, 10);
}
