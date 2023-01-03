/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#ifndef _INCLUDE_UAPI_KAN_COMPILER_H__
#define _INCLUDE_UAPI_KAN_COMPILER_H__

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

#if __stdc_compliance < 99
#define restrict __restrict
#define inline __inline
#endif

#if !defined(__has_builtin)
#define __has_builtin(x) 0
#endif

#if __has_builtin(__builtin_unreachable)
#define unreachable() __builtin_unreachable()
#else
#define unreachable() ({for(;;);})
#endif

#define ___concat(x, y) x##y
#define ___string(x) #x
#define __concat(x, y) ___concat(x, y)
#define __string(x) ___string(x)
#define __string_va(...) #__VA_ARGS__

#define __align_ceil(x, align) (((x) + (align) - 1) & ~((align) - 1))
#define __align_floor(x, align) ((x) & ~((align) - 1))

#define __alias(x)          __attribute__((alias(#x)))
#define __aligned(x)        __attribute__((aligned(x)))
#define __cold              __attribute__((cold))
#define __deprecated        __attribute__((deprecated))
#define __force_inline      __attribute__((always_inline)) inline
#define __format(x, y, z)   __attribute__((format(x, y, z)))
#define __hot               __attribute__((hot))
#define __noreturn          __attribute__((noreturn))
#define __packed            __attribute__((packed))
#define __printf(x, y)      __attribute__((format(printf, x, y)))
#define __pure              __attribute__((pure))
#define __returns_twice     __attribute__((returns_twice))
#define __section(x)        __attribute__((section(x)))
#define __used              __attribute__((used))
#define __nodiscard         __attribute__((warn_unused_result))
#define __weak              __attribute__((weak))

#endif /* _INCLUDE_UAPI_KAN_COMPILER_H__ */
