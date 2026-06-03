#ifndef E6C477C8_D1FE_4C3A_8999_C7AA122B661E
#define E6C477C8_D1FE_4C3A_8999_C7AA122B661E
#pragma once

#include <core/cdefs.h>

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define BE16_to_host(value) __builtin_bswap16(value)
#define BE32_to_host(value) __builtin_bswap32(value)
#define BE64_to_host(value) __builtin_bswap64(value)
#define LE16_to_host(value) (value)
#define LE32_to_host(value) (value)
#define LE64_to_host(value) (value)
#define host_to_BE16(value) __builtin_bswap16(value)
#define host_to_BE32(value) __builtin_bswap32(value)
#define host_to_BE64(value) __builtin_bswap64(value)
#define host_to_LE16(value) (value)
#define host_to_LE32(value) (value)
#define host_to_LE64(value) (value)
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define BE16_to_host(value) (value)
#define BE32_to_host(value) (value)
#define BE64_to_host(value) (value)
#define LE16_to_host(value) __builtin_bswap16(value)
#define LE32_to_host(value) __builtin_bswap32(value)
#define LE64_to_host(value) __builtin_bswap64(value)
#define host_to_BE16(value) (value)
#define host_to_BE32(value) (value)
#define host_to_BE64(value) (value)
#define host_to_LE16(value) __builtin_bswap16(value)
#define host_to_LE32(value) __builtin_bswap32(value)
#define host_to_LE64(value) __builtin_bswap64(value)
#else
#error Unknown endianness
#endif

#endif /* E6C477C8_D1FE_4C3A_8999_C7AA122B661E */
