#ifndef F63D6B62_92FB_4609_8F6A_7D3FE9C20BB1
#define F63D6B62_92FB_4609_8F6A_7D3FE9C20BB1
#pragma once

#include <core/cdefs.h>
#include <core/types.h>

static __always_inline __nodiscard int abs(int value)
{
#if __has_builtin(__builtin_abs)
    return __builtin_abs(value);
#else
    if(value >= 0) {
        return value;
    }
    else {
        return -value;
    }
#endif
}

static __always_inline __nodiscard long labs(long value)
{
#if __has_builtin(__builtin_labs)
    return __builtin_labs(value);
#else
    if(value >= 0L) {
        return value;
    }
    else {
        return -value;
    }
#endif
}

static __always_inline __nodiscard long long llabs(long long value)
{
#if __has_builtin(__builtin_llabs)
    return __builtin_llabs(value);
#else
    if(value >= 0LL) {
        return value;
    }
    else {
        return -value;
    }
#endif
}

#define atoi(nptr)  strtol((nptr), nullptr, 10)
#define atol(nptr)  strtol((nptr), nullptr, 10)
#define atoll(nptr) strtoll((nptr), nullptr, 10)

long strtol(const char* restrict nptr, const char** restrict endptr, int base) __nodiscard;
long long strtoll(const char* restrict nptr, const char** restrict endptr, int base) __nodiscard;
unsigned long strtoul(const char* restrict nptr, const char** restrict endptr, int base) __nodiscard;
unsigned long long strtoull(const char* restrict nptr, const char** restrict endptr, int base) __nodiscard;

intmax_t strtoimax(const char* restrict nptr, const char** restrict endptr, int base) __nodiscard;
uintmax_t strtoumax(const char* restrict nptr, const char** restrict endptr, int base) __nodiscard;
size_t strtousize(const char* restrict nptr, const char** restrict endptr, int base) __nodiscard;

#endif /* F63D6B62_92FB_4609_8F6A_7D3FE9C20BB1 */
