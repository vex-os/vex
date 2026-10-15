#include <vex/bitmap.h>

// Bits are stored MSB-first: bit N of a chunk lives at
// BITMAP_BIT(BITMAP_CHUNK_BITS - N - 1), see bitmap_isset

// Mask selecting chunk positions [bitmod, BITMAP_CHUNK_BITS)
static __always_inline __nodiscard bitmap_type mask_from(size_t bitmod)
{
    return BITMAP_MASK_SET >> bitmod;
}

// Mask selecting chunk positions [0, bitmod]
static __always_inline __nodiscard bitmap_type mask_upto(size_t bitmod)
{
    return BITMAP_MASK_SET << (BITMAP_CHUNK_BITS - 1 - bitmod);
}

int bitmap_range_isset(const bitmap_type* restrict bitmap, size_t bit_a, size_t bit_b)
{
    size_t idx_a = bit_a / BITMAP_CHUNK_BITS;
    size_t idx_b = bit_b / BITMAP_CHUNK_BITS;

    bitmap_type mask_a = mask_from(bit_a % BITMAP_CHUNK_BITS);
    bitmap_type mask_b = mask_upto(bit_b % BITMAP_CHUNK_BITS);

    if(idx_a == idx_b) {
        mask_a &= mask_b;
        mask_b = mask_a;
    }

    if((bitmap[idx_a] & mask_a) != mask_a)
        return 0;

    for(size_t i = idx_a + 1; i < idx_b; ++i) {
        if(bitmap[i] != BITMAP_MASK_SET) {
            return 0;
        }
    }

    if((bitmap[idx_b] & mask_b) != mask_b)
        return 0;
    return 1;
}

int bitmap_range_isclear(const bitmap_type* restrict bitmap, size_t bit_a, size_t bit_b)
{
    size_t idx_a = bit_a / BITMAP_CHUNK_BITS;
    size_t idx_b = bit_b / BITMAP_CHUNK_BITS;

    bitmap_type mask_a = mask_from(bit_a % BITMAP_CHUNK_BITS);
    bitmap_type mask_b = mask_upto(bit_b % BITMAP_CHUNK_BITS);

    if(idx_a == idx_b) {
        mask_a &= mask_b;
        mask_b = mask_a;
    }

    if(bitmap[idx_a] & mask_a)
        return 0;

    for(size_t i = idx_a + 1; i < idx_b; ++i) {
        if(bitmap[i] != BITMAP_MASK_CLEAR) {
            return 0;
        }
    }

    if(bitmap[idx_b] & mask_b)
        return 0;
    return 1;
}

void bitmap_range_clear(bitmap_type* restrict bitmap, size_t bit_a, size_t bit_b)
{
    size_t idx_a = bit_a / BITMAP_CHUNK_BITS;
    size_t idx_b = bit_b / BITMAP_CHUNK_BITS;

    bitmap_type mask_a = mask_from(bit_a % BITMAP_CHUNK_BITS);
    bitmap_type mask_b = mask_upto(bit_b % BITMAP_CHUNK_BITS);

    if(idx_a == idx_b) {
        bitmap[idx_a] &= ~(mask_a & mask_b);
        return;
    }

    bitmap[idx_a] &= ~mask_a;

    for(size_t i = idx_a + 1; i < idx_b; ++i) {
        bitmap[i] = BITMAP_MASK_CLEAR;
    }

    bitmap[idx_b] &= ~mask_b;
}

void bitmap_range_set(bitmap_type* restrict bitmap, size_t bit_a, size_t bit_b)
{
    size_t idx_a = bit_a / BITMAP_CHUNK_BITS;
    size_t idx_b = bit_b / BITMAP_CHUNK_BITS;

    bitmap_type mask_a = mask_from(bit_a % BITMAP_CHUNK_BITS);
    bitmap_type mask_b = mask_upto(bit_b % BITMAP_CHUNK_BITS);

    if(idx_a == idx_b) {
        bitmap[idx_a] |= mask_a & mask_b;
        return;
    }

    bitmap[idx_a] |= mask_a;

    for(size_t i = idx_a + 1; i < idx_b; ++i) {
        bitmap[i] = BITMAP_MASK_SET;
    }

    bitmap[idx_b] |= mask_b;
}
