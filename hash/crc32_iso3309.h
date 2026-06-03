#ifndef CD90B826_6CE7_46EC_A74F_D4FEC331F71E
#define CD90B826_6CE7_46EC_A74F_D4FEC331F71E
#pragma once

#include <hash/crc32.h>

extern struct crc32_table crc32_iso3309_table;

static __always_inline __nodiscard inline uint32_t crc32_iso3309_get(const void* restrict data, size_t length)
{
    return crc32_get_nro(&crc32_iso3309_table, data, length);
}

static __always_inline __nodiscard inline uint32_t crc32_iso3309_get_ext(const void* restrict data, size_t length, uint32_t combine)
{
    return crc32_get_ext_nro(&crc32_iso3309_table, data, length, combine);
}

#endif /* CD90B826_6CE7_46EC_A74F_D4FEC331F71E */
