/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#ifndef __USR_INCLUDE_SYS_FBDEF_H__
#define __USR_INCLUDE_SYS_FBDEF_H__
#include <stdint.h>

typedef struct fb_rgbmask_s {
    uint32_t bits_r;
    uint32_t bits_g;
    uint32_t bits_b;
    uint32_t shift_r;
    uint32_t shift_g;
    uint32_t shift_b;
} fb_rgbmask_t;

typedef struct fb_info_s {
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint32_t pitch;
    fb_rgbmask_t mask;
    uint64_t offset;
} fb_info_t;

#endif /* __USR_INCLUDE_SYS_FBDEF_H__ */
