/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#ifndef __INCLUDE_INTTYPES_H__
#define __INCLUDE_INTTYPES_H__
#include <kan/cdefs.h>
#include <stdint.h>

intmax_t strtoimax(const char *restrict nptr, const char **restrict endptr, int base);
uintmax_t strtoumax(const char *restrict nptr, const char **restrict endptr, int base);

#endif /* __INCLUDE_INTTYPES_H__ */
