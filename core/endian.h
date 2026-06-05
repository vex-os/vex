#ifndef E6C477C8_D1FE_4C3A_8999_C7AA122B661E
#define E6C477C8_D1FE_4C3A_8999_C7AA122B661E
#pragma once

#include <core/cdefs.h>

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define be16toh(value) __builtin_bswap16(value)
#define be32toh(value) __builtin_bswap32(value)
#define be64toh(value) __builtin_bswap64(value)
#define le16toh(value) (value)
#define le32toh(value) (value)
#define le64toh(value) (value)
#define htobe16(value) __builtin_bswap16(value)
#define htobe32(value) __builtin_bswap32(value)
#define htobe64(value) __builtin_bswap64(value)
#define htole16(value) (value)
#define htole32(value) (value)
#define htole64(value) (value)
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define be16toh(value) (value)
#define be32toh(value) (value)
#define be64toh(value) (value)
#define le16toh(value) __builtin_bswap16(value)
#define le32toh(value) __builtin_bswap32(value)
#define le64toh(value) __builtin_bswap64(value)
#define htobe16(value) (value)
#define htobe32(value) (value)
#define htobe64(value) (value)
#define htole16(value) __builtin_bswap16(value)
#define htole32(value) __builtin_bswap32(value)
#define htole64(value) __builtin_bswap64(value)
#else
#error Unknown endianness
#endif

#endif /* E6C477C8_D1FE_4C3A_8999_C7AA122B661E */
