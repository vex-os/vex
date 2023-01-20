/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#include <kan/errno.h>
#include <kan/kmalloc.h>
#include <kan/pmem.h>
#include <kan/psf2.h>
#include <kan/symbol.h>
#include <stdlib.h>
#include <string.h>

static wchar_t fill_unicode(psf2_t *restrict psf, size_t size)
{
    size_t i;
    size_t j;
    size_t nwc;
    size_t glyph;
    size_t start;
    wchar_t maxwc;
    wchar_t wcs[16] = { 0 };
    const psf2_header_t *header = psf->header;
    const char *ppsf = (const char *)header;

    glyph = 0;
    start = header->header_size + (header->num_glyphs * header->glyph_size);
    maxwc = 0;

    for(i = start; ppsf[i] && (i < size) && (glyph < header->num_glyphs); i++) {
        if(ppsf[i] == 0xFF) {
            glyph++;
            continue;
        }

        nwc = mbstowcs(wcs, &ppsf[i], 16);
        if(nwc != SIZE_MAX && nwc) {
            for(j = 0; j < nwc; j++) {
                if(psf->unicode)
                    psf->unicode[wcs[j]] = glyph;
                if(wcs[j] < maxwc)
                    continue;
                maxwc = wcs[j];
            }
        }

        /* Seek until the separator */
        while(i < size && ppsf[i + 1] != 0xFF)
            i++;
        continue;
    }

    return maxwc;
}

int psf2_load(psf2_t *restrict psf, const void *restrict ptr, size_t size)
{
    psf->header = ptr;

    if(!psf2_is_valid(psf))
        return EINVAL;

    psf->max_codepoint = psf->header->num_glyphs;
    psf->unicode_psize = 0;
    psf->unicode = NULL;
    psf->bwidth = __align_ceil(psf->header->glyph_width, 8) / 8;

    if(psf->header->flags & PSF2_UNICODE) {
        psf->max_codepoint = fill_unicode(psf, size);
        psf->unicode_psize = get_page_count((psf->max_codepoint + 1) * sizeof(uint32_t));
        psf->unicode = pmalloc_hhdm(psf->unicode_psize);
        if(!psf->unicode)
            return ENOMEM;
        fill_unicode(psf, size);
    }

    return EOK;
}
EXPORT_SYMBOL(psf2_load);

void psf2_unload(psf2_t *restrict psf)
{
    if(psf->unicode)
        pmfree_hhdm(psf->unicode, psf->unicode_psize);
    psf->header = NULL;
    psf->unicode = NULL;
    psf->unicode_psize = 0;
}
EXPORT_SYMBOL(psf2_unload);

uint32_t psf2_get_glyph(const psf2_t *restrict psf, uint32_t wc)
{
    if(wc > psf->max_codepoint)
        return 0;
    if(psf->unicode)
        return psf->unicode[wc];
    return wc;
}
EXPORT_SYMBOL(psf2_get_glyph);
