/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#ifndef __INCLUDE_STDLIB_H__
#define __INCLUDE_STDLIB_H__
#include <stddef.h>
#include <stdint.h>
#include <sys/cdefs.h>

int abs(int x);
long labs(long x);
long long llabs(long long x);

int atoi(const char *restrict s);
long atol(const char *restrict s);
long long atoll(const char *restrict s);

long strtol(const char *restrict s, const char **restrict endptr, int base);
long long strtoll(const char *restrict s, const char **restrict endptr, int base);
unsigned long strtoul(const char *restrict s, const char **restrict endptr, int base);
unsigned long long strtoull(const char *restrict s, const char **restrict endptr, int base);
intmax_t strtoimax(const char *restrict s, const char **restrict endptr, int base);
uintmax_t strtoumax(const char *restrict s, const char **restrict endptr, int base);
size_t strtouz(const char *restrict s, const char **restrict endptr, int base);

size_t mbtowc(wchar_t *restrict wcs, const char *restrict s, size_t n);
size_t mbstowcs(wchar_t *restrict wcs, const char *restrict s, size_t n);
#define mblen(s, n) mbtowc(NULL, (s), (n))

#endif /* __INCLUDE_STDLIB_H__ */
