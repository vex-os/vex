/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _INCLUDE_ROM_VEX_8X16_H
#define _INCLUDE_ROM_VEX_8X16_H
#include <stdint.h>

#define VEX_8x16_WIDTH      0x08
#define VEX_8x16_HEIGHT     0x10
#define VEX_8x16_GLYPH_SZ   0x10
#define VEX_8x16_SCANLINE   0x01
#define VEX_8x16_MAX_CHAR   0xFF

extern const uint8_t vex_8x16[];

#endif /* _INCLUDE_ROM_VEX_8X16_H */
