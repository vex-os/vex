/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, Kirill GPRB */
#ifndef _INCLUDE_BITMAP_H__
#define _INCLUDE_BITMAP_H__
#include <kan/compiler.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct bitmap_s {
    uint32_t *data;
    size_t nbits;
    size_t size;
} bitmap_t;

static __force_inline void init_bitmap(bitmap_t *restrict bitmap, size_t nbits)
{
    bitmap->nbits = __align_ceil(nbits, 32);
    bitmap->size = bitmap->nbits / 8;
}

static __force_inline bool read_bitmap(const bitmap_t *restrict bitmap, size_t bit)
{
    if(bitmap->data[bit / 32] & (1 << (bit % 32)))
        return true;
    return false;
}

static __force_inline void clear_bitmap(bitmap_t *restrict bitmap, size_t bit)
{
    bitmap->data[bit / 32] &= ~(1 << (bit % 32));
}

static __force_inline void set_bitmap(bitmap_t *restrict bitmap, size_t bit)
{
    bitmap->data[bit / 32] |= (1 << (bit % 32));
}

static __force_inline bool read_bitmap_range(const bitmap_t *restrict bitmap, size_t a, size_t b)
{
    size_t i;
    for(i = a; i <= b; i++) {
        if(i < bitmap->nbits && read_bitmap(bitmap, i))
            continue;
        return false;
    }

    return true;
}

static __force_inline void clear_bitmap_range(bitmap_t *restrict bitmap, size_t a, size_t b)
{
    size_t i;
    for(i = a; i <= b && i < bitmap->nbits; clear_bitmap(bitmap, i++));
}

static __force_inline void set_bitmap_range(bitmap_t *restrict bitmap, size_t a, size_t b)
{
    size_t i;
    for(i = a; i <= b && i < bitmap->nbits; set_bitmap(bitmap, i++));
}

#endif /* _INCLUDE_BITMAP_H__ */
