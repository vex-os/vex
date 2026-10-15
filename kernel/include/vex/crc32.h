#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

#define CRC32_TABLE_SIZE 256

struct crc32_table {
    uint32_t table[CRC32_TABLE_SIZE];
    uint32_t initial;
};

void crc32_generate_nri(struct crc32_table* restrict table, uint32_t polynomial, uint32_t initial);
void crc32_generate_rfi(struct crc32_table* restrict table, uint32_t polynomial, uint32_t initial);
uint32_t crc32_get_ext_nro(const struct crc32_table* restrict table, const void* restrict data, size_t sz, uint32_t ext) __nodiscard;
uint32_t crc32_get_ext_rfo(const struct crc32_table* restrict table, const void* restrict data, size_t sz, uint32_t ext) __nodiscard;
uint32_t crc32_get_nro(const struct crc32_table* restrict table, const void* restrict data, size_t sz) __nodiscard;
uint32_t crc32_get_rfo(const struct crc32_table* restrict table, const void* restrict data, size_t sz) __nodiscard;
