/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __INCLUDE_STDLIB_H__
#define __INCLUDE_STDLIB_H__
#include <sys/cdefs.h>

#ifndef __LIBC_SIZE_T_DEFINED__
#define __LIBC_SIZE_T_DEFINED__
typedef __SIZE_TYPE__ size_t;
#endif

#ifndef __LIBC_WCHAR_T_DEFINED__
#define __LIBC_WCHAR_T_DEFINED__
#define wchar_t __libc_wchar_t
typedef __WCHAR_TYPE__ wchar_t;
#endif

int atoi(const char *restrict s) __weak;
long int atol(const char *restrict s) __weak;
long long int atoll(const char *restrict s) __weak;

long int strtol(const char *restrict s, const char *restrict *restrict endptr, int base) __weak;
long long int strtoll(const char *restrict s, const char *restrict *restrict endptr, int base) __weak;
unsigned long int strtoul(const char *restrict s, const char *restrict *restrict endptr, int base) __weak;
unsigned long long int strtoull(const char *restrict s, const char *restrict *restrict endptr, int base) __weak;
size_t strtouz(const char *restrict s, const char *restrict *restrict endptr, int base) __weak;

int abs(int j) __weak;
long int labs(long int j) __weak;
long long int llabs(long long int j) __weak;

#endif /* __INCLUDE_STDLIB_H__ */
