/* SPDX-License-Identifier: Zlib */
#ifndef INCLUDE_STDLIB_H
#define INCLUDE_STDLIB_H
#include <vex/compiler.h>
#include <stddef.h>
#include <stdint.h>

static ALWAYS_INLINE NODISCARD inline int abs(int value)
{
#if __has_builtin(__builtin_abs)
    return __builtin_abs(value);
#else
    if(value >= 0)
        return value;
    return -value;
#endif
}

static ALWAYS_INLINE NODISCARD inline long labs(long value)
{
#if __has_builtin(__builtin_labs)
    return __builtin_labs(value);
#else
    if(value >= 0L)
        return value;
    return -value;
#endif
}

static ALWAYS_INLINE NODISCARD inline long long llabs(long long value)
{
#if __has_builtin(__builtin_llabs)
    return __builtin_llabs(value);
#else
    if(value >= 0LL)
        return value;
    return -value;
#endif
}

#define atoi(nptr) strtol((nptr), NULL, 10)
#define atol(nptr) strtol((nptr), NULL, 10)
#define atoll(nptr) strtoll((nptr), NULL, 10)

long strtol(const char *restrict nptr, const char **restrict endptr, int base) NODISCARD;
long long strtoll(const char *restrict nptr, const char **restrict endptr, int base) NODISCARD;
unsigned long strtoul(const char *restrict nptr, const char **restrict endptr, int base) NODISCARD;
unsigned long long strtoull(const char *restrict nptr, const char **restrict endptr, int base) NODISCARD;

intmax_t strtoimax(const char *restrict nptr, const char **restrict endptr, int base) NODISCARD;
uintmax_t strtoumax(const char *restrict nptr, const char **restrict endptr, int base) NODISCARD;
size_t strtousize(const char *restrict nptr, const char **restrict endptr, int base) NODISCARD;

#endif /* INCLUDE_STDLIB_H */
