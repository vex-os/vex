/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef _INCLUDE_STDLIB_H__
#define _INCLUDE_STDLIB_H__
#include <stddef.h>
#include <stdint.h>
#include <sys/cdefs.h>

typedef struct strtoxx_ctx_s {
    const char *s;
    uintmax_t error;
    uintmax_t limval;
    int limdigit;
    int sign;
    int base;
} strtoxx_ctx_t;

void strtoxx_init(const char *restrict s, strtoxx_ctx_t *restrict ctx) __weak;
uintmax_t strtoxx_main(strtoxx_ctx_t *restrict ctx) __weak;

int atoi(const char *restrict s) __weak;
long atol(const char *restrict s) __weak;
long long atoll(const char *restrict s) __weak;

long strtol(const char *restrict s, const char **restrict endptr, int base) __weak;
long long strtoll(const char *restrict s, const char **restrict endptr, int base) __weak;
unsigned long strtoul(const char *restrict s, const char **restrict endptr, int base) __weak;
unsigned long long strtoull(const char *restrict s, const char **restrict endptr, int base) __weak;

intmax_t strtoimax(const char *restrict s, const char **restrict endptr, int base) __weak;
uintmax_t strtoumax(const char *restrict s, const char **restrict endptr, int base) __weak;

size_t strtouz(const char *restrict s, const char **restrict endptr, int base) __weak;

int abs(int x) __weak;
long labs(long x) __weak;
long long llabs(long long x) __weak;

#endif /* _INCLUDE_STDLIB_H__ */
