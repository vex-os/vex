#include <vex/consdrv.h>
#include <vex/fbcons.h>
#include <vex/kprintf.h>
#include <vex/limine.h>
#include <vex/page.h>
#include <vex/panic.h>
#include <vex/pmm.h>
#include <vex/string.h>

#include <stdbool.h>

#include "fbcons_font.h"

#define CHR_BS  0x08
#define CHR_HT  0x09
#define CHR_LF  0x0A
#define CHR_VT  0x0B
#define CHR_FF  0x0C
#define CHR_CR  0x0D
#define CHR_WS  0x20
#define CHR_DEL 0x7F

struct vt_cell {
    char value;
    bool dirty;
};

// PATCHME: this is not a good place to house the framebuffer request
static volatile __limine_request struct limine_framebuffer_request fb_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0,
};

static size_t fb_width = 0U;
static size_t fb_height = 0U;
static size_t fb_depth = 0U;
static size_t fb_length = 0U;
static volatile uint8_t* fb_8 = NULL;
static volatile uint16_t* fb_16 = NULL;
static volatile uint32_t* fb_32 = NULL;

static size_t vt_width = 0U;
static size_t vt_height = 0U;
static size_t vt_length = 0U;
static struct vt_cell* vt_cells = NULL;

static size_t cursor_xpos_prev = 0U;
static size_t cursor_ypos_prev = 0U;
static size_t cursor_xpos = 0U;
static size_t cursor_ypos = 0U;

static __always_inline inline void put_pixel(size_t offset, bool white)
{
    switch(fb_depth) {
        case 8:
            fb_8[offset] = white ? UINT8_C(0xFF) : UINT8_C(0x00);
            break;

        case 16:
            fb_16[offset] = white ? UINT16_C(0xFFFF) : UINT16_C(0x0000);
            break;

        case 32:
            fb_32[offset] = white ? UINT32_C(0xFFFFFFFF) : UINT32_C(0x00000000);
            break;

        default:
            panic("fbcons: unsupported framebuffer depth");
            unreachable();
    }
}

static void render_cell(size_t sx, size_t sy, int value)
{
    size_t i;
    size_t gx, gy;
    size_t pixel = 0U;
    const unsigned char* gptr;

    i = sy * FONT_HEIGHT;
    if(i > fb_height)
        return;
    pixel += i * fb_width;

    i = sx * FONT_WIDTH;
    if(i > fb_width)
        return;
    pixel += i;

    if(value < 0 || value > FONT_MAX_CP)
        value = 0;
    gptr = &fbcons_font[value * FONT_STRIDE];

    for(gy = 0U; gy < FONT_HEIGHT; gy += 1U) {
        for(gx = 0U; gx < FONT_WIDTH; gx += 1U)
            put_pixel(pixel + gx, (gptr[gx >> 3] & (0x80 >> (gx & 0x07))));
        gptr += FONT_SCANLINE;
        pixel += fb_width;
    }
}

static void render_cursor(size_t sx, size_t sy)
{
    size_t i;
    size_t gx, gy;
    size_t pixel = 0U;

    i = sy * FONT_HEIGHT;
    if(i > fb_height)
        return;
    pixel += i * fb_width;

    i = sx * FONT_WIDTH;
    if(i > fb_width)
        return;
    pixel += i;

    for(gy = 0U; gy < FONT_HEIGHT; gy += 1U) {
        for(gx = 0U; gx < FONT_WIDTH; gx += 1U)
            put_pixel(pixel + gx, true);
        pixel += fb_width;
    }
}

static void do_newline(void)
{
    size_t i;
    size_t subscreen;
    struct vt_cell* cell;

    cursor_ypos += 1U;

    if(cursor_ypos >= vt_height) {
        cursor_ypos = vt_height - 1U;
        subscreen = vt_length - vt_width;

        for(i = 0U; i < subscreen; ++i) {
            cell = &vt_cells[i];
            cell[0] = cell[vt_width];
            cell[0].dirty = true;
        }

        cell = &vt_cells[subscreen];

        for(i = 0U; i < vt_width; ++i) {
            cell[i].value = CHR_WS;
            cell[i].dirty = true;
        }
    }
}

static void do_redraw(void)
{
    size_t i, j;
    struct vt_cell* cell;

    for(i = 0U; i < vt_width; ++i) {
        for(j = 0U; j < vt_height; ++j) {
            cell = &vt_cells[j * vt_width + i];

            if(cell->dirty) {
                render_cell(i, j, cell->value);

                cell->dirty = false;
            }
        }
    }

    cell = &vt_cells[cursor_ypos_prev * vt_width + cursor_xpos_prev];
    render_cell(cursor_xpos_prev, cursor_ypos_prev, cell->value);

    render_cursor(cursor_xpos, cursor_ypos);

    cursor_xpos_prev = cursor_xpos;
    cursor_ypos_prev = cursor_ypos;
}

static void do_clear(void)
{
    size_t i;

    for(i = 0U; i < fb_length; ++i) {
        put_pixel(i, false);
    }

    memset(vt_cells, 0, vt_length * sizeof(struct vt_cell));
}

static void fbcons_putchar(int character)
{
    struct vt_cell* cell;

    switch(character) {
        case CHR_BS:
        case CHR_DEL:
            if(cursor_xpos > 0U)
                cursor_xpos -= 1U;
            do_redraw();
            return;

        case CHR_HT:
            cursor_xpos += 4U - (cursor_xpos % 4U);
            if(cursor_xpos >= vt_width)
                cursor_xpos = vt_width - 1U;
            do_redraw();
            return;

        case CHR_LF:
            cursor_xpos = 0U;
            do_newline();
            do_redraw();
            return;

        case CHR_VT:
            do_newline();
            do_redraw();
            return;

        case CHR_FF:
            do_clear();
            cursor_xpos_prev = 0U;
            cursor_ypos_prev = 0U;
            cursor_xpos = 0U;
            cursor_ypos = 0U;
            do_redraw();
            return;

        case CHR_CR:
            cursor_xpos = 0U;
            do_redraw();
            return;
    }

    cell = &vt_cells[cursor_ypos * vt_width + cursor_xpos];
    cell->value = character;
    cell->dirty = true;

    cursor_xpos += 1U;

    if(cursor_xpos >= vt_width) {
        cursor_xpos = 0U;
        do_newline();
    }

    do_redraw();
}

static void fbcons_write(struct consdrv* restrict driver, const void* restrict data, size_t sz)
{
    size_t i;
    const char* psz = data;

    for(i = 0U; i < sz; fbcons_putchar(psz[i++])) {
        // empty
    }
}

static void fbcons_unblank(struct consdrv* restrict driver)
{
    // TODO: implement
}

static const struct consdrv_ops fbcons_ops = {
    .init = NULL,
    .write = fbcons_write,
    .unblank = fbcons_unblank,
};

static struct consdrv fbcons_driver = {
    .flags = CONSF_RECALL,
    .identity = "fbcons",
    .ops = &fbcons_ops,
};

void init_fbcons(void)
{
    size_t i;
    struct limine_framebuffer* fb;

    if(fb_request.response) {
        for(i = 0U; i < fb_request.response->framebuffer_count; ++i) {
            fb = fb_request.response->framebuffers[i];

            if(fb->bpp == 8 || fb->bpp == 16 || fb->bpp == 32) {
                fb_width = fb->width;
                fb_height = fb->height;
                fb_depth = fb->bpp;

                fb_length = fb_width * fb_height;

                fb_8 = fb->address;
                fb_16 = fb->address;
                fb_32 = fb->address;

                vt_width = fb_width / FONT_WIDTH;
                vt_height = fb_height / FONT_HEIGHT;
                vt_length = vt_width * vt_height;

                vt_cells = pmm_alloc_dma_hhdm(page_count(vt_length * sizeof(struct vt_cell)));

                if(vt_cells == NULL) {
                    panic("fbcons: out of memory: vt_cells");
                    unreachable();
                }

                cursor_xpos = 0U;
                cursor_ypos = 0U;
                cursor_xpos_prev = 0U;
                cursor_ypos_prev = 0U;

                do_clear();

                consdrv_add(&fbcons_driver);

                return;
            }
        }
    }
}
