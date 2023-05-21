/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#include <blob/kfont.h>
#include <machine/boot.h>
#include <stdbool.h>
#include <string.h>
#include <sys/init.h>
#include <sys/klog.h>
#include <sys/panic.h>
#include <sys/pmm.h>

#define CHR_BS  0x08
#define CHR_HT  0x09
#define CHR_LF  0x0A
#define CHR_VT  0x0B
#define CHR_FF  0x0C
#define CHR_CR  0x0D
#define CHR_WS  0x20
#define CHR_DEL 0x7F

typedef struct cell_s {
    bool dirty;
    int value;
} cell_t;

static size_t framebuffer_width = 0;
static size_t framebuffer_height = 0;
static size_t framebuffer_length = 0;
static volatile uint32_t *framebuffer = NULL;

static size_t screen_width = 0;
static size_t screen_height = 0;
static size_t screen_length = 0;
static cell_t *screen_cells = NULL;

static size_t cursor_pxpos = 0;
static size_t cursor_pypos = 0;
static size_t cursor_xpos = 0;
static size_t cursor_ypos = 0;

static void klog_fb_draw_cell(size_t cx, size_t cy, int value)
{
    size_t i;
    size_t gx;
    size_t gy;
    uint32_t color;
    volatile uint32_t *fbp = framebuffer;
    const uint8_t *gptr;

    i = cy * KFONT_HEIGHT;
    if(i > framebuffer_height)
        return;
    fbp += i * framebuffer_width;

    i = cx * KFONT_WIDTH;
    if(i > framebuffer_width)
        return;
    fbp += i;

    if(value < 0 || value > KFONT_MAX_CHAR)
        value = 0;
    gptr = &kfont[value * KFONT_STRIDE];

    for(gy = 0; gy < KFONT_HEIGHT; ++gy) {
        for(gx = 0; gx < KFONT_WIDTH; ++gx) {
            if(gptr[gx >> 3] & (0x80 >> (gx & 7)))
                color = UINT32_C(0xFFFFFFFF);
            else
                color = UINT32_C(0x00000000);
            fbp[gx] = color;
        }

        fbp += framebuffer_width;
        gptr += KFONT_SCANLINE;
    }
}

static void klog_fb_draw_cursor(size_t cx, size_t cy)
{
    size_t i;
    size_t gx;
    size_t gy;
    volatile uint32_t *fbp = framebuffer;

    i = cy * KFONT_HEIGHT;
    if(i > framebuffer_height)
        return;
    fbp += i * framebuffer_width;

    i = cx * KFONT_WIDTH;
    if(i > framebuffer_width)
        return;
    fbp += i;

    for(gy = 0; gy < KFONT_HEIGHT; ++gy) {
        for(gx = 0; gx < KFONT_WIDTH; ++gx) 
            fbp[gx] = UINT32_C(0xFFFFFFFF);
        fbp += framebuffer_width;
    }
}

static void klog_fb_newline(void)
{
    size_t i;
    size_t subscreen;
    cell_t *cell;

    if(++cursor_ypos >= screen_height) {
        cursor_ypos = screen_height - 1;

        subscreen = screen_length - screen_width;

        for(i = 0; i < subscreen; ++i) {
            cell = &screen_cells[i];
            cell[0] = cell[screen_width];
            cell->dirty = true;
        }

        cell = &screen_cells[subscreen];
        for(i = 0; i < screen_width; ++i) {
            cell[i].dirty = true;
            cell[i].value = CHR_WS;
        }
    }
}

static void klog_fb_redraw(void)
{
    size_t i;
    size_t j;
    cell_t *cell;

    for(i = 0; i < screen_width; ++i) {
        for(j = 0; j < screen_height; ++j) {
            cell = &screen_cells[j * screen_width + i];

            if(cell->dirty) {
                klog_fb_draw_cell(i, j, cell->value);
                cell->dirty = false;
            }
        }
    }

    /* Draw the cell supposedly under the old cursor */
    cell = &screen_cells[cursor_pypos * screen_width + cursor_pxpos];
    klog_fb_draw_cell(cursor_pxpos, cursor_pypos, cell->value);

    /* Draw the new cursor */
    klog_fb_draw_cursor(cursor_xpos, cursor_ypos);

    cursor_pxpos = cursor_xpos;
    cursor_pypos = cursor_ypos;
}

static void klog_fb_clear(void)
{
    size_t i;
    for(i = 0; i < framebuffer_length; framebuffer[i++] = UINT32_C(0x00000000));
    memset(screen_cells, 0, sizeof(cell_t) * screen_length);
}

static void klog_fb_write(klog_sink_t *restrict sink, const void *s, size_t n)
{
    size_t i;
    cell_t *cell = NULL;
    const char *sp = s;

    for(i = 0; i < n; ++i) {
        switch(sp[i]) {
            case CHR_BS:
            case CHR_DEL:
                if(cursor_xpos > 0)
                    cursor_xpos--;
                continue;
            case CHR_HT:
                cursor_xpos += 4 - (cursor_xpos % 4);
                if(cursor_xpos >= screen_width)
                    cursor_xpos = screen_width - 1;
                continue;
            case CHR_LF:
                cursor_xpos = 0;
                klog_fb_newline();
                continue;
            case CHR_VT:
                klog_fb_newline();
                continue;
            case CHR_FF:
                klog_fb_clear();
                cursor_xpos = 0;
                cursor_ypos = 0;
                return;
            case CHR_CR:
                cursor_xpos = 0;
                continue;
        }

        cell = &screen_cells[cursor_ypos * screen_width + cursor_xpos];
        cell->dirty = true;
        cell->value = sp[i];

        if(++cursor_xpos >= screen_width) {
            klog_fb_newline();
            cursor_xpos = 0;
        }
    }

    klog_fb_redraw();
}

static void klog_fb_unblank(klog_sink_t *restrict sink)
{
#if LATER
    klog_fb_clear();
    cursor_xpos = 0;
    cursor_ypos = 0;
#endif
}

static klog_sink_t klog_fb = {
    .next = NULL,
    .write = &klog_fb_write,
    .unblank = &klog_fb_unblank,
    .ident = "klog_fb",
    .data = NULL,
};

static void init_klog_fb(void)
{
    size_t i;
    const fb_info_t *fb;

    for(i = 0; i < MAX_FRAMEBUFFERS; ++i) {
        if(framebuffers[i].vid_depth != 32)
            continue;
        fb = &framebuffers[i];

        framebuffer_width = fb->vid_width;
        framebuffer_height = fb->vid_height;
        framebuffer_length = framebuffer_width * framebuffer_height;
        framebuffer = (volatile uint32_t *)(fb->base_offset + hhdm_offset);

        screen_width = framebuffer_width / KFONT_WIDTH;
        screen_height = framebuffer_height / KFONT_HEIGHT;
        screen_length = screen_width * screen_height;
        screen_cells = pmm_alloc_hhdm(get_page_count(sizeof(cell_t) * screen_length), PMM_ZONE_NORMAL);
        kassertf(screen_cells, "klog_fb: insufficient memory");

        cursor_pxpos = 0;
        cursor_pypos = 0;
        cursor_xpos = 0;
        cursor_ypos = 0;

        klog_fb_clear();

        register_klog_sink(&klog_fb);

        klog(LOG_INFO, "klog_fb: font blob: %d bytes, %dx%d [%zux%zu]", KFONT_BLOBSIZE, KFONT_WIDTH, KFONT_HEIGHT, screen_width, screen_height);
        klog(LOG_INFO, "klog_fb: framebuffer: %s, %zux%zu, %p", fb->ident, framebuffer_width, framebuffer_height, (void *)framebuffer);

        return;
    }
}
subsys_initcall(klog_fb, init_klog_fb);
