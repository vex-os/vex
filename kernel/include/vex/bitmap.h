#pragma once

#include <vex/compiler.h>
#include <vex/limits.h>
#include <vex/types.h>

#define BITMAP_BIT(bit) (UINT32_C(1) << ((uint32_t)(bit)))

#define BITMAP_CHUNK_BITS 32
#define BITMAP_MASK_CLEAR UINT32_C(0x00000000)
#define BITMAP_MASK_SET   UINT32_C(0xFFFFFFFF)

typedef uint32_t bitmap_t;

/// Whether every bit from bit_a through bit_b, both included, is set
int bitmap_range_isset(const bitmap_t* restrict bitmap, size_t bit_a, size_t bit_b);

/// Whether every bit from bit_a through bit_b, both included, is clear
int bitmap_range_isclear(const bitmap_t* restrict bitmap, size_t bit_a, size_t bit_b);

/// Clears the bits from bit_a through bit_b, both included
void bitmap_range_clear(bitmap_t* restrict bitmap, size_t bit_a, size_t bit_b);

/// Sets the bits from bit_a through bit_b, both included
void bitmap_range_set(bitmap_t* restrict bitmap, size_t bit_a, size_t bit_b);

static __always_inline __nodiscard size_t bitmap_bytecount(size_t bits)
{
    bits = align_ceil(bits, BITMAP_CHUNK_BITS);
    return bits / CHAR_BIT;
}

static __always_inline __nodiscard size_t bitmap_chunkcount(size_t bits)
{
    bits = align_ceil(bits, BITMAP_CHUNK_BITS);
    return bits / CHAR_BIT / sizeof(bitmap_t);
}

static __always_inline __nodiscard int bitmap_isset(const bitmap_t* restrict bitmap, size_t bit)
{
    size_t bdiv = bit / BITMAP_CHUNK_BITS;
    size_t bmod = bit % BITMAP_CHUNK_BITS;
    return bitmap[bdiv] & BITMAP_BIT(BITMAP_CHUNK_BITS - bmod - 1);
}

static __always_inline void bitmap_clear(bitmap_t* restrict bitmap, size_t bit)
{
    size_t bdiv = bit / BITMAP_CHUNK_BITS;
    size_t bmod = bit % BITMAP_CHUNK_BITS;
    bitmap[bdiv] &= ~BITMAP_BIT(BITMAP_CHUNK_BITS - bmod - 1);
}

static __always_inline void bitmap_set(bitmap_t* restrict bitmap, size_t bit)
{
    size_t bdiv = bit / BITMAP_CHUNK_BITS;
    size_t bmod = bit % BITMAP_CHUNK_BITS;
    bitmap[bdiv] |= BITMAP_BIT(BITMAP_CHUNK_BITS - bmod - 1);
}
