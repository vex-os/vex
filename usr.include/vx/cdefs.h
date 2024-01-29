/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2023, VX/sys Contributors */
#ifndef USR_INCLUDE_VX_CDEFS_H
#define USR_INCLUDE_VX_CDEFS_H

#if !defined(__GNUC__)
#define __attribute__(...)
#define __restrict
#define __inline
#endif

#if __STDC_VERSION__ < 199901L
#define restrict __restrict
#define inline __inline
#endif

#define __predict_true(x) __builtin_expect((x), 1)
#define __predict_false(x) __builtin_expect((x), 0)

#define __unreachable() __builtin_unreachable()

#define __concat0(x, y) x##y
#define __concat(x, y) __concat0(x, y)
#define __unique(x) __concat(x, __COUNTER__)

#define __string0(x) #x
#define __string(x) __string0(x)
#define __string_va(...) #__VA_ARGS__

#define __align_ceil(x, align) (((x) + (align) - 1) & ~((align) - 1))
#define __align_floor(x, align) ((x) & ~((align) - 1))

#define __alias(x)          __attribute__((alias(#x)))
#define __align(x)          __attribute__((aligned(x)))
#define __always_inline     __attribute__((always_inline))
#define __format(x, y, z)   __attribute__((format(x, y, z)))
#define __nodiscard         __attribute__((warn_unused_result))
#define __noreturn          __attribute__((noreturn))
#define __packed            __attribute__((packed))
#define __printflike(x, y)  __attribute__((format(printf, x, y)))
#define __scanflike(x, y)   __attribute__((format(scanf, x, y)))
#define __section(x)        __attribute__((section(x)))
#define __used              __attribute__((used))
#define __weak              __attribute__((weak))

#if !defined(__unused)
#define __unused __attribute__((unused))
#endif

#endif /* USR_INCLUDE_VX_CDEFS_H */
