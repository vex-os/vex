#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

#define CRC32_TABLE_SIZE 256

typedef struct crc32_table_s crc32_table_t;

struct crc32_table_s {
    uint32_t table[CRC32_TABLE_SIZE];
    uint32_t initial;
};

void crc32_table_init(crc32_table_t* restrict table, uint32_t polynomial, uint32_t initial);
void crc32_table_init_refin(crc32_table_t* restrict table, uint32_t polynomial, uint32_t initial);
uint32_t crc32_update(const crc32_table_t* restrict table, const void* restrict data, size_t size, uint32_t ext) __nodiscard;
uint32_t crc32_update_refout(const crc32_table_t* restrict table, const void* restrict data, size_t size, uint32_t ext) __nodiscard;
uint32_t crc32_compute(const crc32_table_t* restrict table, const void* restrict data, size_t size) __nodiscard;
uint32_t crc32_compute_refout(const crc32_table_t* restrict table, const void* restrict data, size_t size) __nodiscard;
