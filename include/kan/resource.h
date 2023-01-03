/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#ifndef _INCLUDE_KAN_RESOURCE_H__
#define _INCLUDE_KAN_RESOURCE_H__
#include <kan/compiler.h>
#include <uapi/kan/resource.h>

#if defined(__X86_64__)
#include <x86/pmio.h>
#define pmio_read8(x, y) x86_pmio_read8((x), (y))
#define pmio_read16(x, y) x86_pmio_read16((x), (y))
#define pmio_read32(x, y) x86_pmio_read32((x), (y))
#define pmio_write8(x, y) x86_pmio_write8((x), (y))
#define pmio_write16(x, y) x86_pmio_write16((x), (y))
#define pmio_write32(x, y) x86_pmio_write32((x), (y))
#define pmio_throttle() x86_pmio_throttle()
#else
#define pmio_read8(x, y) ((void)(x),(void)(y),(int)(EIO))
#define pmio_read16(x, y) ((void)(x),(void)(y),(int)(EIO))
#define pmio_read32(x, y) ((void)(x),(void)(y),(int)(EIO))
#define pmio_write8(x, y) ((void)(x),(void)(y),(int)(EIO))
#define pmio_write16(x, y) ((void)(x),(void)(y),(int)(EIO))
#define pmio_write32(x, y) ((void)(x),(void)(y),(int)(EIO))
#define pmio_throttle() ((void)(x),(void)(y),(int)(EIO))
#endif

int register_resource(resource_t *restrict r);
const resource_t *find_resource(const char *name);
int resource_read8(const resource_t *restrict r, uintptr_t offset, uint8_t *restrict val);
int resource_read16(const resource_t *restrict r, uintptr_t offset, uint16_t *restrict val);
int resource_read32(const resource_t *restrict r, uintptr_t offset, uint32_t *restrict val);
int resource_write8(const resource_t *restrict r, uintptr_t offset, uint8_t val);
int resource_write16(const resource_t *restrict r, uintptr_t offset, uint16_t val);
int resource_write32(const resource_t *restrict r, uintptr_t offset, uint32_t val);

#endif /* _INCLUDE_KAN_RESOURCE_H__ */
