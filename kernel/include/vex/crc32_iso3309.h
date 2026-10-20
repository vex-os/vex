#pragma once

#include <vex/crc32.h>

extern crc32_table_t crc32_iso3309_table;

static __always_inline __nodiscard uint32_t crc32_iso3309_compute(const void* restrict data, size_t length)
{
    return crc32_compute(&crc32_iso3309_table, data, length);
}

static __always_inline __nodiscard uint32_t crc32_iso3309_update(const void* restrict data, size_t length, uint32_t combine)
{
    return crc32_update(&crc32_iso3309_table, data, length, combine);
}
