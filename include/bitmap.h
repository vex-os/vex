/* SPDX-License-Identifier: Zlib */
#ifndef INCLUDE_BITMAP_H
#define INCLUDE_BITMAP_H
#include <vex/compiler.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#define BITMAP_BIT(bit) (UINT32_C(1) << ((uint32_t)(bit)))

#define BITMAP_CHUNK_BITS 32
#define BITMAP_MASK_CLEAR UINT32_C(0x00000000)
#define BITMAP_MASK_SET UINT32_C(0xFFFFFFFF)

typedef uint32_t bitmap_t;

int bitmap_range_isset(const bitmap_t *restrict bitmap, size_t bit_a, size_t bit_b);
int bitmap_range_isclear(const bitmap_t *restrict bitmap, size_t bit_a, size_t bit_b);
void bitmap_range_clear(bitmap_t *restrict bitmap, size_t bit_a, size_t bit_b);
void bitmap_range_set(bitmap_t *restrict bitmap, size_t bit_a, size_t bit_b);

static ALWAYS_INLINE NODISCARD inline size_t bitmap_bytecount(size_t bits)
{
    bits = ALIGN_CEIL(bits, BITMAP_CHUNK_BITS);
    return bits / CHAR_BIT;
}

static ALWAYS_INLINE NODISCARD inline size_t bitmap_chunkcount(size_t bits)
{
    bits = ALIGN_CEIL(bits, BITMAP_CHUNK_BITS);
    return bits / CHAR_BIT / sizeof(bitmap_t);
}

static ALWAYS_INLINE NODISCARD inline int bitmap_isset(const bitmap_t *restrict bitmap, size_t bit)
{
    size_t bdiv = bit / BITMAP_CHUNK_BITS;
    size_t bmod = bit % BITMAP_CHUNK_BITS;
    return bitmap[bdiv] & BITMAP_BIT(BITMAP_CHUNK_BITS - bmod - 1);
}

static ALWAYS_INLINE inline void bitmap_clear(bitmap_t *restrict bitmap, size_t bit)
{
    size_t bdiv = bit / BITMAP_CHUNK_BITS;
    size_t bmod = bit % BITMAP_CHUNK_BITS;
    bitmap[bdiv] |= BITMAP_BIT(BITMAP_CHUNK_BITS - bmod - 1);
}

static ALWAYS_INLINE inline void bitmap_set(bitmap_t *restrict bitmap, size_t bit)
{
    size_t bdiv = bit / BITMAP_CHUNK_BITS;
    size_t bmod = bit % BITMAP_CHUNK_BITS;
    bitmap[bdiv] &= ~BITMAP_BIT(BITMAP_CHUNK_BITS - bmod - 1);
}

#endif /* INCLUDE_BITMAP_H */
