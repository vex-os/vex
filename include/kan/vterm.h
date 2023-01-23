/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#ifndef _INCLUDE_KAN_VTERM_H__
#define _INCLUDE_KAN_VTERM_H__
#include <kan/compiler.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define VT_NUL          0x00
#define VT_BEL          0x07
#define VT_BS           0x08
#define VT_HT           0x09
#define VT_LF           0x0A
#define VT_VT           0x0B
#define VT_FF           0x0C
#define VT_CR           0x0D
#define VT_DEL          0x7F
#define VT_ESC          0x1B
#define VT_CSI          0x5B

#define VT_BOLD         0x0001
#define VT_FAINT        0x0002
#define VT_ITALIC       0x0004
#define VT_UNDERLINE    0x0008
#define VT_SLOW_BLINK   0x0010
#define VT_FAST_BLINK   0x0020
#define VT_REVERSE      0x0040
#define VT_CONCEAL      0x0080
#define VT_CROSSED_OUT  0x0100
#define VT_FG_24BIT     0x0200
#define VT_FG_BRIGHT    0x0400
#define VT_BG_24BIT     0x0800
#define VT_BG_BRIGHT    0x1000

#define VT_BLACK        0x01
#define VT_RED          0x01
#define VT_GREEN        0x02
#define VT_YELLOW       0x03
#define VT_BLUE         0x04
#define VT_MAGENTA      0x05
#define VT_CYAN         0x06
#define VT_WHITE        0x07
#define VT_ANSI_MASK    0x07

/* Default faint palette */
#define VT_D0_BLACK     0x000000
#define VT_D0_RED       0xAA0000
#define VT_D0_GREEN     0x00AA00
#define VT_D0_YELLOW    0xAA5500
#define VT_D0_BLUE      0x0000AA
#define VT_D0_MAGENTA   0xAA00AA
#define VT_D0_CYAN      0x00AAAA
#define VT_D0_WHITE     0xAAAAAA

/* Default normal palette */
#define VT_D1_BLACK     0x000000
#define VT_D1_RED       0xCD3131
#define VT_D1_GREEN     0x0DBC79
#define VT_D1_YELLOW    0xE5E510
#define VT_D1_BLUE      0x2472C8
#define VT_D1_MAGENTA   0xBC3FBC
#define VT_D1_CYAN      0x11A8CD
#define VT_D1_WHITE     0xE5E5E5

/* Default bright palette */
#define VT_D2_BLACK     0x666666
#define VT_D2_RED       0xF14C4C
#define VT_D2_GREEN     0x23D18B
#define VT_D2_YELLOW    0xF5F543
#define VT_D2_BLUE      0x3B8EE8
#define VT_D2_MAGENTA   0xD670D6
#define VT_D2_CYAN      0x29B8DB
#define VT_D2_WHITE     0xFFFFFF

/* Default attribute options. */
#define VT_DBG_COLOR    VT_D1_BLACK
#define VT_DFG_COLOR    VT_D1_WHITE

#define VT_STATE_ESC    0x00
#define VT_STATE_CSI    0x01
#define VT_STATE_ATTR   0x02
#define VT_STATE_END    0x03

#define VT_MAX_ARGS     0x20

typedef struct vt_attrib_s {
    uint16_t mode;
    uint32_t fg;
    uint32_t bg;
} vt_attrib_t;

typedef struct vt_cell_s {
    bool dirty;
    vt_attrib_t attr;
    wchar_t wc;
} vt_cell_t;

typedef struct vt_cursor_s {
    size_t x;
    size_t y;
} vt_cursor_t;

typedef struct vt_parser_s {
    uint8_t state;
    size_t argc;
    size_t args[VT_MAX_ARGS];
    char prefix;
} vt_parser_t;

typedef struct vt_palette_s {
    uint32_t faint[8];
    uint32_t normal[8];
    uint32_t bright[8];
} vt_palette_t;

typedef struct vt_s {
    size_t cwidth;
    size_t cheight;
    size_t clength;
    vt_cell_t *cells;
    vt_attrib_t attr;
    vt_cursor_t cursor;
    vt_cursor_t pcursor;
    vt_palette_t palette;
    vt_parser_t parser;
    uint32_t cursor_color;
    void(*term_bell)(struct vt_s *restrict vt);
    void(*term_pseq)(struct vt_s *restrict vt, char c);
    void(*draw_cell)(struct vt_s *restrict vt, const vt_cell_t *restrict c, size_t cx, size_t cy);
    void(*draw_cursor)(struct vt_s *restrict vt, const vt_cursor_t *restrict cursor);
    void(*draw_clear)(struct vt_s *restrict vt, uint32_t xrgb);
    void(*draw_end)(struct vt_s *restrict vt);
} vt_t;

int vt_init(vt_t *restrict vt, size_t cwidth, size_t cheight);
void vt_deinit(vt_t *restrict vt);
void vt_puts(vt_t *restrict vt, const char *restrict s);

static inline uint32_t vt_get_bg(const vt_t *restrict vt, const vt_attrib_t *restrict attr)
{
    const uint32_t *palette;
    if(!(attr->mode & VT_BG_24BIT)) {
        if(attr->mode & VT_BG_BRIGHT)
            palette = vt->palette.bright;
        else
            palette = vt->palette.normal;
        return palette[attr->bg & VT_ANSI_MASK];
    }

    return attr->bg;
}

static inline uint32_t vt_get_fg(const vt_t *restrict vt, const vt_attrib_t *restrict attr)
{
    const uint32_t *palette;
    if(!(attr->mode & VT_FG_24BIT)) {
        if(attr->mode & (VT_FG_BRIGHT | VT_BOLD))
            palette = vt->palette.bright;
        else if(attr->mode & VT_FAINT)
            palette = vt->palette.faint;
        else
            palette = vt->palette.normal;
        return palette[attr->fg & VT_ANSI_MASK];
    }

    return attr->fg;
}

#endif /* _INCLUDE_KAN_VTERM_H__ */
