/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#include <kan/errno.h>
#include <kan/pmem.h>
#include <kan/psf2.h>
#include <stdlib.h>
#include <string.h>
#include <kan/kprintf.h>

static wchar_t fill_unicode(psf2_font_t *restrict psf, size_t size)
{
    size_t i;
    size_t nmb;
    size_t glyph;
    size_t usize;
    wchar_t maxwc;
    wchar_t wc;

    glyph = 0;
    usize = size - psf->header->header_size - (psf->header->num_glyphs * psf->header->glyph_size);
    maxwc = 0;

    for(i = 0; psf->utable[i] && (i < usize) && (glyph < psf->header->num_glyphs); i++) {
        if(psf->utable[i] == 0xFF) {
            glyph++;
            continue;
        }

        if(psf->utable[i] == 0xFE) {
            while((i < usize) && (psf->utable[i + 1] != 0xFF))
                i++;
            continue;
        }

        nmb = 4;
        if(i + nmb > size)
            nmb = size - i - 1;

        nmb = mbtowc(&wc, &psf->utable[i], nmb);
        if(nmb != SIZE_MAX) {
            if(psf->unicode) {
                psf->unicode[wc] = glyph;
            }
            if(wc > maxwc)
                maxwc = wc;
            i += nmb - 1;
        }
    }

    return maxwc;
}

int load_psf2(psf2_font_t *restrict psf, const void *ptr, size_t size)
{
    psf->header = ptr;

    if(psf->header->magic[0] != PSF2_MAGIC_0)
        return EINVAL;
    if(psf->header->magic[1] != PSF2_MAGIC_1)
        return EINVAL;
    if(psf->header->magic[2] != PSF2_MAGIC_2)
        return EINVAL;
    if(psf->header->magic[3] != PSF2_MAGIC_3)
        return EINVAL;
    if(psf->header->version != PSF2_VERSION)
        return EINVAL;

    psf->glyphs = ptr;
    psf->glyphs += psf->header->header_size;
    psf->utable = NULL;
    psf->unicode = NULL;
    psf->max_codepoint = (wchar_t)(psf->header->num_glyphs - 1);
    psf->unicode_psize = 0;
    psf->bwidth = __align_ceil(psf->header->glyph_width, 8) / 8;

    if(psf->header->flags & PSF2_UNICODE) {
        psf->utable = psf->glyphs;
        psf->utable += psf->header->num_glyphs * psf->header->glyph_size;
        psf->max_codepoint = fill_unicode(psf, size);
        psf->unicode_psize = get_page_count((psf->max_codepoint + 1) * sizeof(size_t));
        psf->unicode = pmalloc_hhdm(psf->unicode_psize);
        if(!psf->unicode)
            return ENOMEM;
        memset(psf->unicode, 0, psf->unicode_psize * PAGE_SIZE);
        fill_unicode(psf, size);
    }

    return EOK;
}

void unload_psf2(psf2_font_t *restrict psf)
{
    if(psf->unicode)
        pmfree_hhdm(psf->unicode, psf->unicode_psize);
    psf->header = NULL;
    psf->glyphs = NULL;
    psf->utable = NULL;
    psf->unicode = NULL;
    psf->max_codepoint = 0;
    psf->unicode_psize = 0;
    psf->bwidth = 0;
}

const uint8_t *get_psf2_glyph(const psf2_font_t *restrict psf, wchar_t wc)
{
    if(psf->unicode) {
        if(wc > psf->max_codepoint)
            return &psf->glyphs[psf->unicode[0]];
        return &psf->glyphs[psf->unicode[wc] * psf->header->glyph_size];
    }

    if(wc > psf->max_codepoint)
        return &psf->glyphs[0];
    return &psf->glyphs[wc * psf->header->glyph_size];
}
