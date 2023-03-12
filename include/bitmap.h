/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#ifndef __INCLUDE_BITMAP_H__
#define __INCLUDE_BITMAP_H__
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/cdefs.h>

typedef struct bitmap_s {
    uint64_t *data;
    size_t nbits;
    size_t size;
} bitmap_t;

void bitmap_init(bitmap_t *restrict bitmap, size_t nbits);
bool bitmap_read(const bitmap_t *restrict bitmap, size_t bit);
void bitmap_clear(bitmap_t *restrict bitmap, size_t bit);
void bitmap_set(bitmap_t *restrict bitmap, size_t bit);
void bitmap_clear_range(bitmap_t *restrict bitmap, size_t a, size_t b);
void bitmap_set_range(bitmap_t *restrict bitmap, size_t a, size_t b);

#endif /* __INCLUDE_BITMAP_H__ */
