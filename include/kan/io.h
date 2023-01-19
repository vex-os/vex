/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#ifndef _INCLUDE_KAN_IO_H__
#define _INCLUDE_KAN_IO_H__
#include <kan/compiler.h>
#include <stddef.h>
#include <stdint.h>

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

#define IO_16_BIT           0x0001
#define IO_32_BIT           0x0002
#define IO_PORT_MAPPED      0x0004
#define IO_PORT_MAPPED_SLOW 0x0008
#define IO_MEMORY_MAPPED    0x0010

typedef struct io_block_s {
    unsigned short mode;
    uintptr_t base;
    size_t size;
} io_block_t;

int io_read8(const io_block_t *restrict io, uintptr_t offset, uint8_t *restrict val);
int io_read16(const io_block_t *restrict io, uintptr_t offset, uint16_t *restrict val);
int io_read32(const io_block_t *restrict io, uintptr_t offset, uint32_t *restrict val);
int io_write8(const io_block_t *restrict io, uintptr_t offset, uint8_t val);
int io_write16(const io_block_t *restrict io, uintptr_t offset, uint16_t val);
int io_write32(const io_block_t *restrict io, uintptr_t offset, uint32_t val);

#endif /* _INCLUDE_KAN_IO_H__ */
