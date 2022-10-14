/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __INCLUDE_STDLIB_H__
#define __INCLUDE_STDLIB_H__
#include <stddef.h>
#include <sys/cdefs.h>

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
