#include <vex/bitmap.h>

static __always_inline __nodiscard inline bitmap_t make_bitmask(size_t bitmod)
{
    if(bitmod == 0)
        return BITMAP_MASK_SET;
    return ((1 << (BITMAP_CHUNK_BITS - bitmod)) - 1);
}

int bitmap_range_isset(const bitmap_t* restrict bitmap, size_t bit_a, size_t bit_b)
{
    size_t i;
    size_t idx_a = align_floor(bit_a, BITMAP_CHUNK_BITS) / BITMAP_CHUNK_BITS;
    size_t idx_b = align_floor(bit_b, BITMAP_CHUNK_BITS) / BITMAP_CHUNK_BITS;

    bitmap_t mask_a = make_bitmask(bit_a % BITMAP_CHUNK_BITS);
    bitmap_t mask_b = ~make_bitmask((bit_a + 1) % BITMAP_CHUNK_BITS);
    bitmap_t mask_ab = mask_a & mask_b;

    if(idx_a == idx_b) {
        if((bitmap[idx_a] & mask_ab) == mask_ab)
            return 1;
        return 0;
    }

    for(i = idx_a; i <= idx_b; ++i) {
        if(i == idx_a) {
            if((bitmap[i] & mask_a) == mask_a)
                continue;
            return 0;
        }

        if(i == idx_b) {
            if((bitmap[i] & mask_b) == mask_b)
                continue;
            return 0;
        }

        if(bitmap[i] == BITMAP_MASK_SET)
            continue;
        return 0;
    }

    return 1;
}

int bitmap_range_isclear(const bitmap_t* restrict bitmap, size_t bit_a, size_t bit_b)
{
    size_t i;
    size_t idx_a = align_floor(bit_a, BITMAP_CHUNK_BITS) / BITMAP_CHUNK_BITS;
    size_t idx_b = align_floor(bit_b, BITMAP_CHUNK_BITS) / BITMAP_CHUNK_BITS;

    bitmap_t mask_a = make_bitmask(bit_a % BITMAP_CHUNK_BITS);
    bitmap_t mask_b = ~make_bitmask((bit_a + 1) % BITMAP_CHUNK_BITS);
    bitmap_t mask_ab = mask_a & mask_b;

    if(idx_a == idx_b) {
        if((bitmap[idx_a] & ~mask_ab) == bitmap[idx_a])
            return 1;
        return 0;
    }

    for(i = idx_a; i <= idx_b; ++i) {
        if(i == idx_a) {
            if((bitmap[i] & ~mask_a) == bitmap[i])
                continue;
            return 0;
        }

        if(i == idx_b) {
            if((bitmap[i] & ~mask_b) == bitmap[i])
                continue;
            return 0;
        }

        if(bitmap[i] == BITMAP_MASK_CLEAR)
            continue;
        return 0;
    }

    return 1;
}

void bitmap_range_clear(bitmap_t* restrict bitmap, size_t bit_a, size_t bit_b)
{
    size_t i;
    size_t idx_a = align_floor(bit_a, BITMAP_CHUNK_BITS) / BITMAP_CHUNK_BITS;
    size_t idx_b = align_floor(bit_b, BITMAP_CHUNK_BITS) / BITMAP_CHUNK_BITS;

    bitmap_t mask_a = make_bitmask(bit_a % BITMAP_CHUNK_BITS);
    bitmap_t mask_b = ~make_bitmask((bit_a + 1) % BITMAP_CHUNK_BITS);
    bitmap_t mask_ab = mask_a & mask_b;

    if(idx_a == idx_b) {
        bitmap[idx_a] &= ~mask_ab;
        return;
    }

    for(i = idx_a; i <= idx_b; ++i) {
        if(i == idx_a) {
            bitmap[i] &= ~mask_a;
            continue;
        }

        if(i == idx_b) {
            bitmap[i] &= ~mask_b;
            continue;
        }

        bitmap[i] = BITMAP_MASK_CLEAR;
    }
}

void bitmap_range_set(bitmap_t* restrict bitmap, size_t bit_a, size_t bit_b)
{
    size_t i;
    size_t idx_a = align_floor(bit_a, BITMAP_CHUNK_BITS) / BITMAP_CHUNK_BITS;
    size_t idx_b = align_floor(bit_b, BITMAP_CHUNK_BITS) / BITMAP_CHUNK_BITS;

    bitmap_t mask_a = make_bitmask(bit_a % BITMAP_CHUNK_BITS);
    bitmap_t mask_b = ~make_bitmask((bit_a + 1) % BITMAP_CHUNK_BITS);
    bitmap_t mask_ab = mask_a & mask_b;

    if(idx_a == idx_b) {
        bitmap[idx_a] |= mask_ab;
        return;
    }

    for(i = idx_a; i <= idx_b; ++i) {
        if(i == idx_a) {
            bitmap[i] |= mask_a;
            continue;
        }

        if(i == idx_b) {
            bitmap[i] |= mask_b;
            continue;
        }

        bitmap[i] = BITMAP_MASK_SET;
    }
}
