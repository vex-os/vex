/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __LIBC_CTYPE_ASCII_CTYPE_ASCII_H__
#define __LIBC_CTYPE_ASCII_CTYPE_ASCII_H__
#include <ctype.h>

#define CTYPE_SIZE 0x101 /* 256 plus EOF */
extern const struct __libc_ctype __ctype_ascii[CTYPE_SIZE];

#endif /* __LIBC_CTYPE_ASCII_CTYPE_ASCII_H__ */
