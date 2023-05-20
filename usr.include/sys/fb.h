/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#ifndef __USR_INCLUDE_SYS_FB_H__
#define __USR_INCLUDE_SYS_FB_H__
#include <stdint.h>

typedef struct fb_vmask_s {
    uint16_t offset;
    uint32_t bits;
} fb_vmask_t;

typedef struct fb_info_s {
    char ident[64];
    uint64_t base_offset;
    uint64_t mmio_offset;
    uint32_t mmio_length;
    uint16_t vid_width;
    uint16_t vid_height;
    uint32_t vid_scanline;
    uint16_t vid_depth;
    fb_vmask_t vid_red;
    fb_vmask_t vid_green;
    fb_vmask_t vid_blue;
    fb_vmask_t vid_alpha;
} fb_info_t;

#endif /* __USR_INCLUDE_SYS_FB_H__ */
