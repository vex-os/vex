/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __INCLUDE_KANERU_RESOURCE_H__
#define __INCLUDE_KANERU_RESOURCE_H__
#include <kaneru/cdefs.h>
#include <stdint.h>

#if defined(__X86_64__)
#include <x86_64/pmio.h>
#define pmio_read8 x86_pmio_read8
#define pmio_read16 x86_pmio_read16
#define pmio_read32 x86_pmio_read32
#define pmio_write8 x86_pmio_write8
#define pmio_write16 x86_pmio_write16
#define pmio_write32 x86_pmio_write32
#define pmio_throttle x86_pmio_throttle
#endif

#define RESOURCE_MMIO       (1 << 0)
#define RESOURCE_MMIO_A_16  (1 << 1)
#define RESOURCE_MMIO_A_32  (1 << 2)
#define RESOURCE_PMIO       (1 << 3)
#define RESOURCE_PMIO_SLOW  (1 << 4)

struct resource {
    char name[64];
    uintptr_t base;
    unsigned int flags;
};

#define new_resource \
    static struct resource __section(".rodata.resources") __used

/* defined in kernel/sys.<arch>.lds */
extern struct resource __resources_beg;
extern struct resource __resources_end;

const struct resource *find_resource(const char *restrict name);
int resource_read8(const struct resource *restrict r, uintptr_t off, uint8_t *restrict val);
int resource_read16(const struct resource *restrict r, uintptr_t off, uint16_t *restrict val);
int resource_read32(const struct resource *restrict r, uintptr_t off, uint32_t *restrict val);
int resource_write8(const struct resource *restrict r, uintptr_t off, uint8_t val);
int resource_write16(const struct resource *restrict r, uintptr_t off, uint16_t val);
int resource_write32(const struct resource *restrict r, uintptr_t off, uint32_t val);

#endif /* __INCLUDE_KANERU_RESOURCE_H__ */
