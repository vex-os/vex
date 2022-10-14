/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __SYS_LIBK_STRTOX_H__
#define __SYS_LIBK_STRTOX_H__
#include <stdint.h>
#include <sys/cdefs.h>

const char *_strtox_prelim(const char *restrict s, int *sign, int *base) __weak;
uintmax_t _strtox_main(const char *restrict *restrict s, int base, uintmax_t error, uintmax_t limval, int limdigit, int *sign) __weak;

#endif /* __SYS_LIBK_STRTOX_H__ */
