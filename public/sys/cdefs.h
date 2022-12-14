/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef _PUBLIC_SYS_CDEFS_H__
#define _PUBLIC_SYS_CDEFS_H__

#if __STDC_VERSION__ >= 199901L
#define __stdc_compliance 99
#elif defined(__STDC__)
#define __stdc_compliance 90
#endif

#if !defined(__GNUC__)
#define __attribute__(...)
#define __restrict
#define __inline
#endif

#if __stdc_compliance >= 99
#include <stdnoreturn.h>
#else
#define noreturn __attribute__((noreturn))
#define restrict __restrict
#define inline __inline
#endif

#define __alias(x)          __attribute__((alias(#x)))
#define __aligned(x)        __attribute__((aligned(x)))
#define __always_inline     __attribute__((always_inline)) inline
#define __cold              __attribute__((cold))
#define __deprecated        __attribute__((deprecated))
#define __format(x, y, z)   __attribute__((format(x, y, z)))
#define __hot               __attribute__((hot))
#define __packed            __attribute__((packed))
#define __printf(x, y)      __attribute__((format(printf, x, y)))
#define __pure              __attribute__((pure))
#define __returns_twice     __attribute__((returns_twice))
#define __section(x)        __attribute__((section(x)))
#define __used              __attribute__((used))
#define __nodiscard         __attribute__((warn_unused_result))
#define __weak              __attribute__((weak))

#endif /* _PUBLIC_SYS_CDEFS_H__ */
