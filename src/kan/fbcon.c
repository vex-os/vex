/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#include <blob/kfont.h>
#include <kan/boot.h>
#include <kan/console.h>
#include <kan/debug.h>
#include <kan/errno.h>
#include <kan/initcall.h>
#include <kan/kprintf.h>
#include <kan/pmem.h>
#include <kan/psf2.h>
#include <kan/vterm.h>
#include <stdlib.h>
#include <string.h>

static uint32_t mask_r = 0;
static uint32_t mask_g = 0;
static uint32_t mask_b = 0;
static struct limine_framebuffer fb = { 0 };
static psf2_font_t psf = { 0 };
static vt_t vterm = { 0 };

static __force_inline uint32_t make_mask(uint8_t n)
{
    uint32_t i;
    for(i = 0; n; i |= (1 << ((n--) - 1)));
    return i;
}

static __force_inline uint32_t remap_color(uint32_t xrgb)
{
    uint32_t result = 0;
    result |= ((xrgb >> 16) & mask_r) << fb.red_mask_shift;
    result |= ((xrgb >>  8) & mask_g) << fb.green_mask_shift;
    result |= ((xrgb >>  0) & mask_b) << fb.blue_mask_shift;
    return result;
}

static void draw_cell(vt_t *restrict vt, const vt_cell_t *restrict c, size_t cx, size_t cy)
{
    size_t i;
    size_t gx;
    size_t gy;
    bool visible;
    uint32_t temp;
    uint32_t rbg = vt_get_bg(&vterm, &c->attr);
    uint32_t rfg = vt_get_fg(&vterm, &c->attr);
    uint32_t *fbp = fb.address;
    const uint8_t *glyph;

    i = cy * psf.header->glyph_height;
    if(i > fb.height)
        return;
    fbp += i * fb.width;

    i = cx * psf.header->glyph_width;
    if(i > fb.width)
        return;
    fbp += i;

    rbg = remap_color(rbg);
    rfg = remap_color(rfg);

    if(c->attr.mode & VT_REVERSE) {
        temp = rbg;
        rbg = rfg;
        rfg = temp;
    }

    glyph = get_psf2_glyph(&psf, c->wc);
    visible = !(c->attr.mode & VT_CONCEAL);
    for(gy = 0; gy < psf.header->glyph_height; gy++) {
        for(gx = 0; gx < psf.header->glyph_width; gx++) {
            if(visible && (glyph[gx >> 3] & (0x80 >> (gx & 7))))
                fbp[gx] = rfg;
            else
                fbp[gx] = rbg;
        }

        fbp += fb.width;
        glyph += psf.bwidth;
    }
}

static void draw_cursor(vt_t *restrict vt, const vt_cursor_t *restrict cursor)
{
    size_t i;
    size_t gx;
    size_t gy;
    uint32_t *fbp = fb.address;

    i = cursor->y * psf.header->glyph_height;
    if(i > fb.height)
        return;
    fbp += i * fb.width;

    i = cursor->x * psf.header->glyph_width;
    if(i > fb.width)
        return;
    fbp += i;

    for(gy = 0; gy < psf.header->glyph_height; gy++) {
        for(gx = 0; gx < psf.header->glyph_width; gx++)
            fbp[gx] = 0xFFFFFFFF;
        fbp += fb.width;
    }
}

static void draw_clear(vt_t *restrict vt, uint32_t xrgb)
{
    size_t i;
    size_t fblen = fb.width * fb.height;
    uint32_t rcolor = remap_color(xrgb);
    uint32_t *fbp = fb.address;
    for(i = 0; i < fblen; fbp[i++] = rcolor);
}

static void fbcon_puts(console_t *restrict con, const char *restrict s)
{
    vt_puts(&vterm, s);
}

static console_t fbcon = {
    .name = "fbcon",
    .puts_fn = &fbcon_puts,
    .next = NULL,
};

static int init_fbcon(void)
{
    int r;
    size_t i;
    size_t cwidth;
    size_t cheight;
    const char *ext;
    const char *base;
    const struct limine_file *file = NULL;
    const struct limine_module_response *modules = get_modules();
    const struct limine_framebuffer_response *framebuffers = get_framebuffers();

    /* FIXME: can we not depend on 32-bit framebuffers? */
    if(framebuffers && framebuffers->framebuffer_count) {
        for(i = 0; i < framebuffers->framebuffer_count; i++) {
            if(framebuffers->framebuffers[i]->bpp == 32) {
                memcpy(&fb, framebuffers->framebuffers[i], sizeof(struct limine_framebuffer));
                break;
            }
        }
    }

    if(!fb.address)
        return ENODEV;
    pr_inform("fbcon: fb.address=%p", (void *)fb.address);
    pr_inform("fbcon: fb.size=%zux%zu", (size_t)fb.width, (size_t)fb.height);
    pr_inform("fbcon: fb.pitch=%zu", (size_t)fb.pitch);

    if(modules && modules->module_count) {
        for(i = 0; i < modules->module_count; i++) {
            file = modules->modules[i];
            ext = strchr(file->path, '.');
            base = strrchr(file->path, '/');
            base = base ? (base + 1) : file->path;

            if(!ext || (strcmp(ext, ".psf") && strcmp(ext, ".psfu")))
                continue;
            r = load_psf2(&psf, file->address, file->size);

            if(r == EOK) {
                pr_inform("fbcon: loaded font %s, %zu KiB", base, file->size / 1024);
                break;
            }

            unload_psf2(&psf);
        }
    }

    if(!psf.header) {
        r = load_psf2(&psf, &__kfont, __kfont_size[0]);
        panic_if(r == EINVAL, "fbcon: invalid built-in font");
        panic_if(r == ENOMEM, "fbcon: insufficient memory");
        panic_if(r != EOK, "fbcon: failed to load built-in font: %s", strerror(r));

        base = strrchr(__kfont_path, '/');
        base = base ? (base + 1) : __kfont_path;
        pr_inform("fbcon: loaded built-in font %s, %zu KiB", base, __kfont_size[0] / 1024);
    }

    pr_inform("fbcon: psf.num_glyphs=%zu", (size_t)psf.header->num_glyphs);
    pr_inform("fbcon: psf.glyph_width=%zu", (size_t)psf.header->glyph_width);
    pr_inform("fbcon: psf.glyph_height=%zu", (size_t)psf.header->glyph_height);
    pr_inform("fbcon: psf.max_codepoint=%zX", (size_t)psf.max_codepoint);
    pr_inform("fbcon: psf.unicode=%p", (void *)psf.unicode);

    /* We use these to remap the 0xXXRRGGBB values we use
     * across fbcon and vterm to values the framebuffer actually expects. */
    mask_r = make_mask(fb.red_mask_size);
    mask_g = make_mask(fb.green_mask_size);
    mask_b = make_mask(fb.blue_mask_size);

    /* Setup VT callbacks */
    vterm.draw_cell = &draw_cell;
    vterm.draw_cursor = &draw_cursor;
    vterm.draw_clear = &draw_clear;

    cwidth = fb.width / psf.header->glyph_width;
    cheight = fb.height / psf.header->glyph_height;

    r = vt_init(&vterm, cwidth, cheight);
    panic_if(r == ENOMEM, "fbcon: insufficient memory");
    panic_if(r != EOK, "fbcon: failed to initialize vterm: %s", strerror(r));

    unregister_boot_console();
    return register_console(&fbcon);
}
initcall_tier_0(fbcon, init_fbcon);
initcall_depend(fbcon, boot_console);
initcall_depend(fbcon, boot_info);
initcall_depend(fbcon, pmem);
