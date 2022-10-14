/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __LIBC_STDLIB_STRTOX_H__
#define __LIBC_STDLIB_STRTOX_H__
#include <stdint.h>
#include <sys/cdefs.h>

extern const char __libc_strtox_digits[];
const char *__libc_strtox_prelim(const char *restrict s, int *sign, int *base) __weak;
uintmax_t __libc_strtox_main(const char *restrict *restrict s, int base, uintmax_t error, uintmax_t limval, int limdigit, int *sign) __weak;

#endif /* __LIBC_STDLIB_STRTOX_H__ */
