/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#include <blob/kfont.h>
#include <kan/bootinfo.h>
#include <kan/console.h>
#include <kan/debug.h>
#include <kan/errno.h>
#include <kan/initcall.h>
#include <kan/kprintf.h>
#include <kan/pmem.h>
#include <kan/psf2.h>
#include <stdlib.h>
#include <string.h>

static size_t cursor = 0;
static size_t cwidth = 0;
static size_t cheight = 0;
static uint32_t mask_r = 0;
static uint32_t mask_g = 0;
static uint32_t mask_b = 0;
static struct limine_framebuffer fb = { 0 };
static psf2_t psf = { 0 };

static __force_inline uint32_t make_mask(uint8_t n)
{
    uint32_t i;
    for(i = 0; n; i |= (1 << ((n--) - 1)));
    return i;
}

static __force_inline __used uint32_t remap_color(uint32_t xrgb)
{
    uint32_t result = 0;
    result |= ((xrgb >> 16) & mask_r) << fb.red_mask_shift;
    result |= ((xrgb >>  8) & mask_g) << fb.green_mask_shift;
    result |= ((xrgb >>  0) & mask_b) << fb.blue_mask_shift;
    return result;
}

static void draw_char(wchar_t wc, uint32_t rbg, uint32_t rfg, size_t cx, size_t cy)
{
    size_t i;
    size_t gx;
    size_t gy;
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

    glyph = (const uint8_t *)psf.header;
    glyph += psf.header->header_size;
    glyph += psf2_get_glyph(&psf, wc) * psf.header->glyph_size;

    for(gy = 0; gy < psf.header->glyph_height; gy++) {
        for(gx = 0; gx < psf.header->glyph_width; gx++) {
            if(glyph[gx >> 3] & (0x80 >> (gx & 7)))
                fbp[gx] = rfg;
            else
                fbp[gx] = rbg;
        }

        fbp += fb.width;
        glyph += psf.bwidth;
    }
}

static int init_fbcon(void)
{
    int r;
    size_t i;
    const char *ext;
    const struct limine_file *file = NULL;
    const struct limine_module_response *modules = NULL;
    const struct limine_framebuffer_response *framebuffers = NULL;

    fb.address = NULL;
    psf.header = NULL;
    psf.unicode = NULL;

    framebuffers = get_framebuffers();
    if(framebuffers && framebuffers->framebuffer_count) {
        for(i = 0; i < framebuffers->framebuffer_count; i++) {
            if(framebuffers->framebuffers[i]->bpp == 32) {
                memcpy(&fb, framebuffers->framebuffers[i], sizeof(struct limine_framebuffer));
                break;
            }
        }
    }

    if(fb.address) {
        pr_inform("fbcon: fb.address=%p", (void *)fb.address);
        pr_inform("fbcon: fb.size=%zux%zu", (size_t)fb.width, (size_t)fb.height);
        pr_inform("fbcon: fb.pitch=%zu", (size_t)fb.pitch);
        pr_inform("fbcon: fb.red_mask_size=%02zX", (size_t)fb.red_mask_size);
        pr_inform("fbcon: fb.red_mask_shift=%zu", (size_t)fb.red_mask_shift);
    }
    else return ENODEV;

    modules = get_modules();
    if(modules) {
        for(i = 0; i < modules->module_count; i++) {
            file = modules->modules[i];
            ext = strchr(file->path, '.');
            if(ext && (!strcmp(ext, ".psf") || !strcmp(ext, ".psfu"))) {
                pr_inform("fbcon: loading %s", file->path);
                r = psf2_load(&psf, file->address, file->size);
                if(r != EOK) {
                    psf2_unload(&psf);
                    continue;
                }

                break;
            }
        }
    }

    if(!psf.header) {
        pr_inform("fbcon: loading built-in font at %p", (void *)(&__kfont));
        r = psf2_load(&psf, &__kfont, __kfont_size[0]);
        panic_if(r == EINVAL, "fbcon: invalid built-in font");
        panic_if(r == ENOMEM, "fbcon: insufficient memory");
    }

    pr_inform("fbcon: psf.bwidth=%zu", psf.bwidth);
    pr_inform("fbcon: psf.glyph_width=%zu", (size_t)psf.header->glyph_width);
    pr_inform("fbcon: psf.glyph_height=%zu", (size_t)psf.header->glyph_height);
    pr_inform("fbcon: psf.max_codepoint=%08zX", (size_t)psf.max_codepoint);
    pr_inform("fbcon: psf.unicode_psize=%zu", psf.unicode_psize);

    cursor = 0;
    cwidth = fb.width / psf.header->glyph_width;
    cheight = fb.height / psf.header->glyph_height;
    mask_r = make_mask(fb.red_mask_size);
    mask_g = make_mask(fb.green_mask_size);
    mask_b = make_mask(fb.blue_mask_size);

    return EOK;
}
initcall_tier_0(fbcon, init_fbcon);
initcall_depend(fbcon, bootinfo);
initcall_depend(fbcon, pmem);

static int test_fbcon(void)
{
    size_t i, j;
    const wchar_t *wcs = L"KanOS теперь понимает Юникод!";
    for(j = 0; j < 10; j++) {
        for(i = 0; wcs[i]; i++)
            draw_char(wcs[i], 0x00000000, 0xFFFFFFFF, i + 64, j + 1);
    }
    return EOK;
}
initcall_tier_2(test_fbcon, test_fbcon);
