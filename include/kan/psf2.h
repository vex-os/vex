/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#ifndef _INCLUDE_KAN_PSF2_H__
#define _INCLUDE_KAN_PSF2_H__
#include <uapi/kan/psf2.h>

int load_psf2(psf2_font_t *restrict psf, const void *ptr, size_t size);
void unload_psf2(psf2_font_t *restrict psf);
const uint8_t *get_psf2_glyph(const psf2_font_t *restrict psf, wchar_t wc);

#endif /* _INCLUDE_KAN_PSF2_H__ */
