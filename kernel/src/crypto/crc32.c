#include <vex/crc32.h>

void crc32_generate_nri(struct crc32_table* restrict table, uint32_t polynomial, uint32_t initial)
{
    for(uint32_t i = 0; i < CRC32_TABLE_SIZE; ++i) {
        uint32_t value = i << 24;

        for(uint32_t j = 0; j < 8; ++j) {
            if(value & UINT32_C(0x80000000)) {
                value = polynomial ^ (value << 1);
            }
            else {
                value <<= 1;
            }
        }

        table->table[i] = value;
    }

    table->initial = initial;
}

void crc32_generate_rfi(struct crc32_table* restrict table, uint32_t polynomial, uint32_t initial)
{
    for(uint32_t i = 0; i < CRC32_TABLE_SIZE; ++i) {
        uint32_t value = i;

        for(uint32_t j = 0; j < 8; ++j) {
            if(value & 1) {
                value = polynomial ^ (value >> 1);
            }
            else {
                value >>= 1;
            }
        }

        table->table[i] = value;
    }

    table->initial = initial;
}

uint32_t crc32_get_ext_nro(const struct crc32_table* restrict table, const void* restrict data, size_t sz, uint32_t ext)
{
    const uint8_t* data_ptr = data;
    uint32_t crc = ~ext;

    for(size_t i = 0; i < sz; ++i)
        crc = table->table[(crc ^ data_ptr[i]) & 0xFF] ^ (crc >> 8);

    return crc ^ UINT32_C(0xFFFFFFFF);
}

uint32_t crc32_get_ext_rfo(const struct crc32_table* restrict table, const void* restrict data, size_t sz, uint32_t ext)
{
    uint32_t value = crc32_get_ext_nro(table, data, sz, ext);
    value = ((value & UINT32_C(0xAAAAAAAA)) >> 1) | ((value & UINT32_C(0x55555555)) << 1);
    value = ((value & UINT32_C(0xCCCCCCCC)) >> 2) | ((value & UINT32_C(0x33333333)) << 2);
    value = ((value & UINT32_C(0xF0F0F0F0)) >> 4) | ((value & UINT32_C(0x0F0F0F0F)) << 4);
    return __builtin_bswap32(value);
}

uint32_t crc32_get_nro(const struct crc32_table* restrict table, const void* restrict data, size_t sz)
{
    return crc32_get_ext_nro(table, data, sz, ~table->initial);
}

uint32_t crc32_get_rfo(const struct crc32_table* restrict table, const void* restrict data, size_t sz)
{
    return crc32_get_ext_rfo(table, data, sz, ~table->initial);
}
