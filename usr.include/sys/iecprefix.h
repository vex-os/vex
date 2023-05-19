/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#ifndef __USR_INCLUDE_SYS_IECPREFIX_H__
#define __USR_INCLUDE_SYS_IECPREFIX_H__
#include <stdint.h>

#define KiB(x) (((uintmax_t)(x)) << UINTMAX_C(10))
#define MiB(x) (((uintmax_t)(x)) << UINTMAX_C(20))
#define GiB(x) (((uintmax_t)(x)) << UINTMAX_C(30))
#define TiB(x) (((uintmax_t)(x)) << UINTMAX_C(40))
#define PiB(x) (((uintmax_t)(x)) << UINTMAX_C(50))

#define KiB_R(x) (((uintmax_t)(x)) >> UINTMAX_C(10))
#define MiB_R(x) (((uintmax_t)(x)) >> UINTMAX_C(20))
#define GiB_R(x) (((uintmax_t)(x)) >> UINTMAX_C(30))
#define TiB_R(x) (((uintmax_t)(x)) >> UINTMAX_C(40))
#define PiB_R(x) (((uintmax_t)(x)) >> UINTMAX_C(50))

#endif /* __USR_INCLUDE_SYS_IECPREFIX_H__ */
