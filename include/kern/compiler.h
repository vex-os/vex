// SPDX-License-Identifier: Zlib
#ifndef INCLUDE_KERN_COMPILER_H
#define INCLUDE_KERN_COMPILER_H

#if !defined(__GNUC__)
#define __attribute__(...)
#define __restrict
#define __inline
#endif

#if !defined(__has_attribute)
#define __has_attribute(...) (0)
#endif

#if !defined(__has_builtin)
#define __has_builtin(...) (0)
#endif

#if __STDC_VERSION__ < 199901L
#define restrict __restrict
#define inline __inline
#endif

#if __has_builtin(__builtin_expect)
#define predict_false(cond) __builtin_expect((cond), 0L)
#define predict_true(cond) __builtin_expect((cond), 1L)
#else
#define predict_false(cond) (cond)
#define predict_true(cond) (cond)
#endif

#if __has_builtin(__builtin_unreachable)
#define unreachable() __builtin_unreachable()
#else
#define unreachable() do {} while(1)
#endif

#define UNUSED_ARG(arg) ((void)(arg))

#define CONCAT0(x, y) x##y
#define CONCAT(x, y) CONCAT0(x, y)
#define UNIQUE(x) CONCAT(x, __COUNTER__)

#define STRINGIFY0(x) #x
#define STRINGIFY(x) STRINGIFY0(x)
#define STRING_VA(...) #__VA_ARGS__

#define align_ceil(x, align) (((x) + (align) - 1) & ~((align) - 1))
#define align_floor(x, align) ((x) & ~((align) - 1))

#define __alias(func)       __attribute__((alias(#func)))
#define __align_as(type)    __attribute__((aligned(sizeof(type))))
#define __always_inline     __attribute__((always_inline))
#define __nodiscard         __attribute__((warn_unused_result))
#define __noreturn          __attribute__((noreturn))
#define __packed            __attribute__((packed))
#define __printflike(x, y)  __attribute__((format(printf, x, y)))
#define __scanflike(x, y)   __attribute__((format(scanf, x, y)))
#define __section(name)     __attribute__((section(name)))
#define __used              __attribute__((used))

#endif // INCLUDE_KERN_COMPILER_H
