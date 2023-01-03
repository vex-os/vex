/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, Kirill GPRB */
#ifndef _INCLUDE_BITMAP_H__
#define _INCLUDE_BITMAP_H__
#include <kan/compiler.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct bitmap_s {
    uint8_t *data;
    size_t num_bits;
    size_t num_bytes;
} bitmap_t;

static __force_inline void bitmap_init(bitmap_t *restrict bitmap, size_t num_bits)
{
    bitmap->num_bits = __align_ceil(num_bits, 8);
    bitmap->num_bytes = bitmap->num_bits / 8;
}

static __force_inline bool bitmap_check(const bitmap_t *restrict bitmap, size_t bit)
{
    if(bitmap->data[bit / 8] & (1 << (bit % 8)))
        return true;
    return false;
}

static __force_inline void bitmap_clear(bitmap_t *restrict bitmap, size_t bit)
{
    bitmap->data[bit / 8] &= ~(1 << (bit % 8));
}

static __force_inline void bitmap_set(bitmap_t *restrict bitmap, size_t bit)
{
    bitmap->data[bit / 8] |= (1 << (bit % 8));
}

static __force_inline bool bitmap_check_range(const bitmap_t *restrict bitmap, size_t a, size_t b)
{
    size_t i;
    for(i = a; i <= b; i++) {
        if(bitmap_check(bitmap, i))
            continue;
        return false;
    }

    return true;
}

static __force_inline void bitmap_clear_range(bitmap_t *restrict bitmap, size_t a, size_t b)
{
    size_t i;
    for(i = a; i <= b; bitmap_clear(bitmap, i++));
}

static __force_inline void bitmap_set_range(bitmap_t *restrict bitmap, size_t a, size_t b)
{
    size_t i;
    for(i = a; i <= b; bitmap_set(bitmap, i++));
}

#endif /* _INCLUDE_BITMAP_H__ */
