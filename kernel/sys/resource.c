/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#include <kaneru/errno.h>
#include <kaneru/resource.h>
#include <string.h>

static volatile void *get_aligned_mmio(unsigned int flags, uintptr_t base, uintptr_t off)
{
    if(flags & RESOURCE_MMIO_A_16)
        return &((volatile uint16_t *)base)[off];
    if(flags & RESOURCE_MMIO_A_32)
        return &((volatile uint32_t *)base)[off];
    return &((volatile uint8_t *)base)[off];
}

const struct resource *find_resource(const char *restrict name)
{
    const struct resource *res;
    for(res = &__resources_beg; res != &__resources_end; res++) {
        if(strncmp(res->name, name, sizeof(res->name)) != 0)
            continue;
        return res;
    }

    return NULL;
}

int resource_read8(const struct resource *restrict r, uintptr_t off, uint8_t *restrict val)
{
    volatile uint8_t *mmio_base;

    if(r->flags & RESOURCE_PMIO) {
        if(r->flags & RESOURCE_PMIO_SLOW)
            pmio_throttle();
        return pmio_read8(r->base + off, val);
    }

    if(r->flags & RESOURCE_MMIO) {
        mmio_base = get_aligned_mmio(r->flags, r->base, off);
        val[0] = mmio_base[0];
        return 0;
    }

    return -EIO;
}

int resource_read16(const struct resource *restrict r, uintptr_t off, uint16_t *restrict val)
{
    volatile uint16_t *mmio_base;

    if(r->flags & RESOURCE_PMIO) {
        if(r->flags & RESOURCE_PMIO_SLOW)
            pmio_throttle();
        return pmio_read16(r->base + off, val);
    }

    if(r->flags & RESOURCE_MMIO) {
        mmio_base = get_aligned_mmio(r->flags, r->base, off);
        val[0] = mmio_base[0];
        return 0;
    }

    return -EIO;
}

int resource_read32(const struct resource *restrict r, uintptr_t off, uint32_t *restrict val)
{
    volatile uint32_t *mmio_base;

    if(r->flags & RESOURCE_PMIO) {
        if(r->flags & RESOURCE_PMIO_SLOW)
            pmio_throttle();
        return pmio_read32(r->base + off, val);
    }

    if(r->flags & RESOURCE_MMIO) {
        mmio_base = get_aligned_mmio(r->flags, r->base, off);
        val[0] = mmio_base[0];
        return 0;
    }

    return -EIO;
}

int resource_write8(const struct resource *restrict r, uintptr_t off, uint8_t val)
{
    volatile uint8_t *mmio_base;

    if(r->flags & RESOURCE_PMIO) {
        if(r->flags & RESOURCE_PMIO_SLOW)
            pmio_throttle();
        return pmio_write8(r->base + off, val);
    }

    if(r->flags & RESOURCE_MMIO) {
        mmio_base = get_aligned_mmio(r->flags, r->base, off);
        mmio_base[0] = val;
        return 0;
    }

    return -EIO;
}

int resource_write16(const struct resource *restrict r, uintptr_t off, uint16_t val)
{
    volatile uint16_t *mmio_base;

    if(r->flags & RESOURCE_PMIO) {
        if(r->flags & RESOURCE_PMIO_SLOW)
            pmio_throttle();
        return pmio_write16(r->base + off, val);
    }

    if(r->flags & RESOURCE_MMIO) {
        mmio_base = get_aligned_mmio(r->flags, r->base, off);
        mmio_base[0] = val;
        return 0;
    }

    return -EIO;
}

int resource_write32(const struct resource *restrict r, uintptr_t off, uint32_t val)
{
    volatile uint32_t *mmio_base;

    if(r->flags & RESOURCE_PMIO) {
        if(r->flags & RESOURCE_PMIO_SLOW)
            pmio_throttle();
        return pmio_write32(r->base + off, val);
    }

    if(r->flags & RESOURCE_MMIO) {
        mmio_base = get_aligned_mmio(r->flags, r->base, off);
        mmio_base[0] = val;
        return 0;
    }

    return -EIO;
}
