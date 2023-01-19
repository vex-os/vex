/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <kan/io.h>
#include <kan/symbol.h>
#include <string.h>

static uintptr_t get_pmio_register(const io_block_t *restrict io, uintptr_t offset)
{
    if(io->mode & IO_16_BIT)
        return io->base + (offset * sizeof(uint16_t));
    if(io->mode & IO_32_BIT)
        return io->base + (offset * sizeof(uint32_t));
    return io->base + offset;
}

static volatile void *get_mmio_register(const io_block_t *restrict io, uintptr_t offset)
{
    if(io->mode & IO_16_BIT)
        return &((volatile uint16_t *)io->base)[offset];
    if(io->mode & IO_32_BIT)
        return &((volatile uint32_t *)io->base)[offset];
    return &((volatile uint8_t *)io->base)[offset];
}

int io_read8(const io_block_t *restrict io, uintptr_t offset, uint8_t *restrict val)
{
    volatile uint8_t *mmio;

    if(offset < io->size) {
        if(io->mode & IO_PORT_MAPPED) {
            if(io->mode & IO_PORT_MAPPED_SLOW)
                pmio_throttle();
            return pmio_read8(get_pmio_register(io, offset), val);
        }

        if(io->mode & IO_MEMORY_MAPPED) {
            mmio = get_mmio_register(io, offset);
            val[0] = mmio[0];
            return EOK;
        }

        return EIO;
    }

    return ERANGE;
}
EXPORT_SYMBOL(io_read8);

int io_read16(const io_block_t *restrict io, uintptr_t offset, uint16_t *restrict val)
{
    volatile uint16_t *mmio;

    if(offset < io->size) {
        if(io->mode & IO_PORT_MAPPED) {
            if(io->mode & IO_PORT_MAPPED_SLOW)
                pmio_throttle();
            return pmio_read16(get_pmio_register(io, offset), val);
        }

        if(io->mode & IO_MEMORY_MAPPED) {
            mmio = get_mmio_register(io, offset);
            val[0] = mmio[0];
            return EOK;
        }

        return EIO;
    }

    return ERANGE;
}
EXPORT_SYMBOL(io_read16);

int io_read32(const io_block_t *restrict io, uintptr_t offset, uint32_t *restrict val)
{
    volatile uint32_t *mmio;

    if(offset < io->size) {
        if(io->mode & IO_PORT_MAPPED) {
            if(io->mode & IO_PORT_MAPPED_SLOW)
                pmio_throttle();
            return pmio_read32(get_pmio_register(io, offset), val);
        }

        if(io->mode & IO_MEMORY_MAPPED) {
            mmio = get_mmio_register(io, offset);
            val[0] = mmio[0];
            return EOK;
        }

        return EIO;
    }

    return ERANGE;
}
EXPORT_SYMBOL(io_read32);

int io_write8(const io_block_t *restrict io, uintptr_t offset, uint8_t val)
{
    volatile uint8_t *mmio;

    if(offset < io->size) {
        if(io->mode & IO_PORT_MAPPED) {
            if(io->mode & IO_PORT_MAPPED_SLOW)
                pmio_throttle();
            return pmio_write8(get_pmio_register(io, offset), val);
        }

        if(io->mode & IO_MEMORY_MAPPED) {
            mmio = get_mmio_register(io, offset);
            mmio[0] = val;
            return EOK;
        }

        return EIO;
    }

    return ERANGE;
}
EXPORT_SYMBOL(io_write8);

int io_write16(const io_block_t *restrict io, uintptr_t offset, uint16_t val)
{
    volatile uint16_t *mmio;

    if(offset < io->size) {
        if(io->mode & IO_PORT_MAPPED) {
            if(io->mode & IO_PORT_MAPPED_SLOW)
                pmio_throttle();
            return pmio_write16(get_pmio_register(io, offset), val);
        }

        if(io->mode & IO_MEMORY_MAPPED) {
            mmio = get_mmio_register(io, offset);
            mmio[0] = val;
            return EOK;
        }

        return EIO;
    }

    return ERANGE;
}
EXPORT_SYMBOL(io_write16);

int io_write32(const io_block_t *restrict io, uintptr_t offset, uint32_t val)
{
    volatile uint32_t *mmio;

    if(offset < io->size) {
        if(io->mode & IO_PORT_MAPPED) {
            if(io->mode & IO_PORT_MAPPED_SLOW)
                pmio_throttle();
            return pmio_write32(get_pmio_register(io, offset), val);
        }

        if(io->mode & IO_MEMORY_MAPPED) {
            mmio = get_mmio_register(io, offset);
            mmio[0] = val;
            return EOK;
        }

        return EIO;
    }

    return ERANGE;
}
EXPORT_SYMBOL(io_write32);
