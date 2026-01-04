#ifndef _DEVICE_FBCONS_FONT_H
#define _DEVICE_FBCONS_FONT_H 1

#define FONT_WIDTH    0x08
#define FONT_HEIGHT   0x10
#define FONT_STRIDE   0x10 // bytes per glyph
#define FONT_SCANLINE 0x01 // bytes per line
#define FONT_MAX_CP   0xFF

extern const char fbcons_font[4096];

#endif
