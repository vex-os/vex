/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __PUBLIC_KANERU_CDEFS_H__
#define __PUBLIC_KANERU_CDEFS_H__

#if !defined(__STDC__) && !defined(__cplusplus)
#error An ANSI-compliant C/C++ compiler is required!
#endif

#if !defined(__GNUC__)
#warning No GNU C extensions! Expect weird behavior!
#define __BASE_FILE__ __FILE__
#define __attribute__(...)
#define __restrict
#define __inline
#endif

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ < 199901L)
#define const
#define inline __inline
#define restrict __restrict
#elif defined(__cplusplus)
#define restrict
#endif

#if !defined(__has_attribute)
#define __has_attribute(x) 0
#endif

#if !defined(__has_extension)
#define __has_extension(x) 0
#endif

#if !defined(__has_feature)
#define __has_feature(x) 0
#endif

#if !defined(__has_include)
#define __has_include(x) 0
#endif

#if !defined(__has_builtin)
#define __has_builtin(x) 0
#endif

#define __concat_1(x, y) x##y
#define __string_1(x) #x
#define __string_va(...) #__VA_ARGS__
#define __concat(x, y) __concat_1(x, y)
#define __string(x) __string_1(x)

#define __likely(x) __builtin_expect(!!(x), 1)
#define __unlikely(x) __builtin_expect(!!(x), 0)

#define __align_up(x, align) (((x) + (align) - 1) & ~((align) - 1))
#define __align_down(x, align) ((x) & ~((align) - 1))

#define __alias(x)              __attribute__((alias(#x)))
#define __aligned(x)            __attribute__((aligned(x)))
#define __cold                  __attribute__((cold))
#define __deprecated            __attribute__((deprecated))
#define __format(x, y, z)       __attribute__((format(x, y, z)))
#define __hot                   __attribute__((hot))
#define __maybe_unused          __attribute__((unused))
#define __noreturn              __attribute__((noreturn))
#define __optimize(x)           __attribute__((optimize(x)))
#define __packed                __attribute__((packed))
#define __pure                  __attribute__((pure))
#define __returns_twice         __attribute__((returns_twice))
#define __section(x)            __attribute__((section(x)))
#define __used                  __attribute__((used))
#define __warn_unused_result    __attribute__((warn_unused_result))
#define __weak                  __attribute__((weak))
#define __weak_alias(x)         __attribute__((weakref(#x)))

#endif /* __PUBLIC_KANERU_CDEFS_H__ */
