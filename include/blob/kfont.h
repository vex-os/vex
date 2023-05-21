/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#ifndef __INCLUDE_BLOB_KFONT_H__
#define __INCLUDE_BLOB_KFONT_H__
#include <stdint.h>

#define KFONT_WIDTH 0x08
#define KFONT_HEIGHT 0x10
#define KFONT_STRIDE 0x10
#define KFONT_SCANLINE 0x01
#define KFONT_MAX_CHAR 0xFF
#define KFONT_BLOBSIZE 0x1000
extern const uint8_t kfont[KFONT_BLOBSIZE];

#endif /* __INCLUDE_BLOB_KFONT_H__ */
