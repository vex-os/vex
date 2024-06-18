/* SPDX-License-Identifier: Zlib */
#ifndef INCLUDE_ISERIX_COMPILER_H
#define INCLUDE_ISERIX_COMPILER_H

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
#define PREDICT_FALSE(cond) __builtin_expect((cond), 0L)
#define PREDICT_TRUE(cond) __builtin_expect((cond), 1L)
#else
#define PREDICT_FALSE(cond) (cond)
#define PREDICT_TRUE(cond) (cond)
#endif

#if __has_builtin(__builtin_unreachable)
#define UNREACHABLE() __builtin_unreachable()
#else
#define UNREACHABLE() do {} while(1)
#endif

#define CONCAT0(x, y) x##y
#define CONCAT(x, y) CONCAT0(x, y)
#define UNIQUE(x) CONCAT(x, __COUNTER__)

#define STRINGIFY0(x) #x
#define STRINGIFY(x) STRINGIFY0(x)
#define STRING_VA(...) #__VA_ARGS__

#define ALIGN_CEIL(x, align) (((x) + (align) - 1) & ~((align) - 1))
#define ALIGN_FLOOR(x, align) ((x) & ~((align) - 1))

#if __has_attribute(alias)
#define ALIAS(func) __attribute__((alias(#func)))
#else
#define ALIAS(...)
#endif

#if __has_attribute(aligned)
#define ALIGN_AS(type) __attribute__((aligned(sizeof(type))))
#else
#define ALIGN_AS(...)
#endif

#if __has_attribute(always_inline)
#define ALWAYS_INLINE __attribute__((always_inline))
#else
#define ALWAYS_INLINE
#endif

#if __has_attribute(warn_unused_result)
#define NODISCARD __attribute__((warn_unused_result))
#else
#define NODISCARD
#endif

#if __has_attribute(noreturn)
#define NORETURN __attribute__((noreturn))
#else
#define NORETURN
#endif

#if __has_attribute(packed)
#define PACKED __attribute__((packed))
#else
#define PACKED
#endif

#if __has_attribute(format)
#define PRINTFLIKE(x, y) __attribute__((format(printf, x, y)))
#define SCANFLIKE(x, y) __attribute__((format(scanf, x, y)))
#else
#define PRINTFLIKE(...)
#define SCANFLIKE(...)
#endif

#if __has_attribute(section)
#define SECTION(name) __attribute__((section(name)))
#else
#define SECTION(...)
#endif

#if __has_attribute(unused)
#define UNUSED __attribute__((unused))
#else
#define UNUSED
#endif

#if __has_attribute(used)
#define USED __attribute__((used))
#else
#define USED
#endif

#endif /* INCLUDE_ISERIX_COMPILER_H */
