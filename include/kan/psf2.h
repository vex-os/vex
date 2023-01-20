/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#ifndef _INCLUDE_KAN_PSF2_H__
#define _INCLUDE_KAN_PSF2_H__
#include <uapi/kan/psf2.h>

int psf2_load(psf2_t *restrict psf, const void *restrict ptr, size_t size);
void psf2_unload(psf2_t *restrict psf);
uint32_t psf2_get_glyph(const psf2_t *restrict psf, uint32_t wc);

#endif /* _INCLUDE_KAN_PSF2_H__ */
