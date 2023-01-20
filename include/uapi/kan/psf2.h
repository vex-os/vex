/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#ifndef _INCLUDE_UAPI_KAN_PSF2_H__
#define _INCLUDE_UAPI_KAN_PSF2_H__
#include <kan/compiler.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PSF2_MAGIC_0 0x72
#define PSF2_MAGIC_1 0xB5
#define PSF2_MAGIC_2 0x4A
#define PSF2_MAGIC_3 0x86
#define PSF2_VERSION 0x00000000
#define PSF2_UNICODE 0x00000001
typedef struct psf2_header_s {
    uint8_t magic[4];
    uint32_t version;
    uint32_t header_size;
    uint32_t flags;
    uint32_t num_glyphs;
    uint32_t glyph_size;
    uint32_t glyph_height;
    uint32_t glyph_width;
} __packed psf2_header_t;

typedef struct psf2_s {
    const psf2_header_t *header;
    uint32_t max_codepoint;
    size_t unicode_psize;
    uint32_t *unicode;
    size_t bwidth;
} psf2_t;

static __force_inline bool psf2_is_valid(const psf2_t *psf2)
{
    if(!psf2 || !psf2->header)
        return false;
    if(psf2->header->magic[0] != PSF2_MAGIC_0)
        return false;
    if(psf2->header->magic[1] != PSF2_MAGIC_1)
        return false;
    if(psf2->header->magic[2] != PSF2_MAGIC_2)
        return false;
    if(psf2->header->magic[3] != PSF2_MAGIC_3)
        return false;
    if(psf2->header->version != PSF2_VERSION)
        return false;
    return true;
}

#endif /* _INCLUDE_UAPI_KAN_PSF2_H__ */
