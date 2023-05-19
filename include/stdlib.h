/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#ifndef __INCLUDE_STDLIB_H__
#define __INCLUDE_STDLIB_H__
#include <stddef.h>
#include <sys/cdefs.h>

int abs(int x);
long labs(long x);
long long llabs(long long x);

int atoi(const char *restrict s);
long atol(const char *restrict s);
long long atoll(const char *restrict s);

long strtol(const char *restrict nptr, const char **restrict endptr, int base);
long long strtoll(const char *restrict nptr, const char **restrict endptr, int base);
unsigned long strtoul(const char *restrict nptr, const char **restrict endptr, int base);
unsigned long long strtoull(const char *restrict nptr, const char **restrict endptr, int base);
size_t strtousize(const char *restrict nptr, const char **restrict endptr, int base);

#endif /* __INCLUDE_STDLIB_H__ */
