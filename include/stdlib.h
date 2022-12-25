/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#ifndef _INCLUDE_STDLIB_H__
#define _INCLUDE_STDLIB_H__
#include <kan/compiler.h>
#include <stddef.h>
#include <stdint.h>

typedef struct strtoxx_ctx_s {
    const char *s;
    uintmax_t error;
    uintmax_t limval;
    int limdigit;
    int sign;
    int base;
} strtoxx_ctx_t;

void strtoxx_init(const char *restrict s, strtoxx_ctx_t *restrict ctx);
uintmax_t strtoxx_main(strtoxx_ctx_t *restrict ctx);

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

int abs(int x);
long labs(long x);
long long llabs(long long x);

#endif /* _INCLUDE_STDLIB_H__ */
