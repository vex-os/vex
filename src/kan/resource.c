/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <kan/resource.h>
#include <kan/symbol.h>
#include <string.h>

static resource_t *resources = NULL;

int register_resource(resource_t *restrict r)
{
    if(!find_resource(r->name)) {
        r->next = resources;
        resources = r;
        return EOK;
    }

    return EBUSY;
}
EXPORT_SYMBOL(register_resource);

const resource_t *find_resource(const char *name)
{
    const resource_t *res;
    for(res = resources; res; res = res->next) {
        if(strncmp(res->name, name, sizeof(res->name)) != 0)
            continue;
        return res;
    }

    return NULL;
}
EXPORT_SYMBOL(find_resource);

static volatile void *get_aligned_mmio(unsigned int flags, uintptr_t base, uintptr_t offset)
{
    if(flags & RESOURCE_MMIO_A_16)
        return &((volatile uint16_t *)base)[offset];
    if(flags & RESOURCE_MMIO_A_32)
        return &((volatile uint32_t *)base)[offset];
    return &((volatile uint8_t *)base)[offset];
}

int resource_read8(const resource_t *restrict r, uintptr_t offset, uint8_t *restrict val)
{
    volatile uint8_t *mmio_base;

    if(r->flags & RESOURCE_PMIO) {
        if(r->flags & RESOURCE_PMIO_SLOW)
            pmio_throttle();
        return pmio_read8(r->base + offset, val);
    }

    if(r->flags & RESOURCE_MMIO) {
        mmio_base = get_aligned_mmio(r->flags, r->base, offset);
        val[0] = mmio_base[0];
        return EOK;
    }

    return EIO;
}
EXPORT_SYMBOL(resource_read8);

int resource_read16(const resource_t *restrict r, uintptr_t offset, uint16_t *restrict val)
{
    volatile uint16_t *mmio_base;

    if(r->flags & RESOURCE_PMIO) {
        if(r->flags & RESOURCE_PMIO_SLOW)
            pmio_throttle();
        return pmio_read16(r->base + offset, val);
    }

    if(r->flags & RESOURCE_MMIO) {
        mmio_base = get_aligned_mmio(r->flags, r->base, offset);
        val[0] = mmio_base[0];
        return EOK;
    }

    return EIO;
}
EXPORT_SYMBOL(resource_read16);

int resource_read32(const resource_t *restrict r, uintptr_t offset, uint32_t *restrict val)
{
    volatile uint32_t *mmio_base;

    if(r->flags & RESOURCE_PMIO) {
        if(r->flags & RESOURCE_PMIO_SLOW)
            pmio_throttle();
        return pmio_read32(r->base + offset, val);
    }

    if(r->flags & RESOURCE_MMIO) {
        mmio_base = get_aligned_mmio(r->flags, r->base, offset);
        val[0] = mmio_base[0];
        return EOK;
    }

    return EIO;
}
EXPORT_SYMBOL(resource_read32);

int resource_write8(const resource_t *restrict r, uintptr_t offset, uint8_t val)
{
    volatile uint8_t *mmio_base;

    if(r->flags & RESOURCE_PMIO) {
        if(r->flags & RESOURCE_PMIO_SLOW)
            pmio_throttle();
        return pmio_write8(r->base + offset, val);
    }

    if(r->flags & RESOURCE_MMIO) {
        mmio_base = get_aligned_mmio(r->flags, r->base, offset);
        mmio_base[0] = val;
        return EOK;
    }

    return EIO;
}
EXPORT_SYMBOL(resource_write8);

int resource_write16(const resource_t *restrict r, uintptr_t offset, uint16_t val)
{
    volatile uint16_t *mmio_base;

    if(r->flags & RESOURCE_PMIO) {
        if(r->flags & RESOURCE_PMIO_SLOW)
            pmio_throttle();
        return pmio_write16(r->base + offset, val);
    }

    if(r->flags & RESOURCE_MMIO) {
        mmio_base = get_aligned_mmio(r->flags, r->base, offset);
        mmio_base[0] = val;
        return EOK;
    }

    return EIO;
}
EXPORT_SYMBOL(resource_write16);

int resource_write32(const resource_t *restrict r, uintptr_t offset, uint32_t val)
{
    volatile uint32_t *mmio_base;

    if(r->flags & RESOURCE_PMIO) {
        if(r->flags & RESOURCE_PMIO_SLOW)
            pmio_throttle();
        return pmio_write32(r->base + offset, val);
    }

    if(r->flags & RESOURCE_MMIO) {
        mmio_base = get_aligned_mmio(r->flags, r->base, offset);
        mmio_base[0] = val;
        return EOK;
    }

    return EIO;
}
EXPORT_SYMBOL(resource_write32);
