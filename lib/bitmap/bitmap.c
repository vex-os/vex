/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#include <bitmap.h>

void bitmap_init(bitmap_t *restrict bitmap, size_t nbits)
{
    bitmap->bm_nbits = __align_ceil(nbits, 64);
    bitmap->bm_size = bitmap->bm_nbits / 8;
}

bool bitmap_read(const bitmap_t *restrict bitmap, size_t bit)
{
    if(bitmap->bm_data[bit / 64] & (1 << (bit % 64)))
        return true;
    return false;
}

void bitmap_clear(bitmap_t *restrict bitmap, size_t bit)
{
    bitmap->bm_data[bit / 64] &= ~(1 << (bit % 64));
}

void bitmap_set(bitmap_t *restrict bitmap, size_t bit)
{
    bitmap->bm_data[bit / 64] |= (1 << (bit % 64));
}

bool bitmap_read_range(const bitmap_t *restrict bitmap, size_t a, size_t b)
{
    size_t i;
    for(i = a; i <= b; i++) {
        if(i < bitmap->bm_nbits && bitmap_read(bitmap, i))
            continue;
        return false;
    }

    return true;
}

void bitmap_clear_range(bitmap_t *restrict bitmap, size_t a, size_t b)
{
    size_t i;
    for(i = a; i <= b && i < bitmap->bm_nbits; bitmap_clear(bitmap, i++));
}

void bitmap_set_range(bitmap_t *restrict bitmap, size_t a, size_t b)
{
    size_t i;
    for(i = a; i <= b && i < bitmap->bm_nbits; bitmap_set(bitmap, i++));
}
