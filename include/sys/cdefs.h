/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _INCLUDE_SYS_CDEFS_H
#define _INCLUDE_SYS_CDEFS_H

#if !defined(__GNUC__)
#define __attribute__(...)
#define __restrict
#define __inline
#endif

#if !defined(__has_builtin)
#define __has_builtin(...) (0)
#endif

#if __STDC_VERSION__ < 199901L
#define restrict __restrict
#define inline __inline
#endif

#if __has_builtin(__builtin_expect)
#define PREDICT_FALSE(cond) __builtin_expect((cond), 0L)
#define PREDICT_TRUE(cond)  __builtin_expect((cond), 1L)
#else
#define PREDICT_FALSE(cond) (cond)
#define PREDICT_TRUE(cond)  (cond)
#endif

#if __has_builtin(__builtin_unreachable)
#define UNREACHABLE() __builtin_unreachable()
#else
#define UNREACHABLE() do {} while(1)
#endif

#define __concat0(x, y) x##y
#define __concat(x, y) __concat0(x, y)
#define __unique(x) __concat(x, __COUNTER__)

#define __stringify0(x) #x
#define __stringify(x) __stringify0(x)
#define __string_va(...) #__VA_ARGS__

#define ALIGN_CEIL(x, align) (((x) + (align) - 1) & ~((align) - 1))
#define ALIGN_FLOOR(x, align) ((x) & ~((align) - 1))

#define __alias(func)       __attribute__((alias(#func)))
#define __align_as(type)    __attribute__((aligned(sizeof(type))))
#define __always_inline     __attribute__((always_inline))
#define __nodiscard         __attribute__((warn_unused_result))
#define __noreturn          __attribute__((noreturn))
#define __packed            __attribute__((packed))
#define __printflike(x, y)  __attribute__((format(printf, x, y)))
#define __section(name)     __attribute__((section(name)))
#define __used              __attribute__((used))

#if !defined(__unused)
#define __unused            __attribute__((unused))
#endif

#endif /* _INCLUDE_SYS_CDEFS_H */
