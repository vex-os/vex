/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#ifndef __SYS_CDEFS_H__
#define __SYS_CDEFS_H__

#if !defined(__GNUC__)
#define __attribute__(...)
#define __restrict
#define __inline
#endif

#if __STDC_VERSION__ < 199901L
#define restrict __restrict
#define inline __inline
#endif

#define if_likely(x) if(__builtin_expect((x), 1))
#define if_unlikely(x) if(__builtin_expect((x), 0))
#define UNREACHABLE() __builtin_unreachable()

#define __concat0(x, y) x##y
#define __string0(x) #x
#define __concat(x, y) __concat0(x, y)
#define __string(x) __string0(x)
#define __unique(x) __concat(x, __COUNTER__)
#define __string_va(...) #__VA_ARGS__

#define __align_ceil(x, align) (((x) + (align) - 1) & ~((align) - 1))
#define __align_floor(x, align) ((x) & ~((align) - 1))

#define __alias(x) __attribute__((alias(#x)))
#define __align(x) __attribute__((aligned(x)))
#define __format(x, y, z) __attribute__((format(x, y, z)))
#define __printflike(x, y) __attribute__((format(printf, x, y)))
#define __scanflike(x, y) __attribute__((format(scanf, x, y)))
#define __section(x) __attribute__((section(x)))

#define __always_inline __attribute__((always_inline))
#define __nodiscard __attribute__((warn_unused_result))
#define __noreturn __attribute__((noreturn))
#define __packed __attribute__((packed))
#define __weak __attribute__((weak))
#define __used __attribute__((used))

#if !defined(__unused)
#define __unused __attribute__((unused))
#endif

#endif /* __SYS_CDEFS_H__ */
