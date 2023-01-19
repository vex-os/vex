/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <kan/io.h>
#include <kan/symbol.h>
#include <string.h>

int io_read(const io_block_t *restrict io, uintptr_t offset, uintmax_t *restrict out)
{
    if(offset < io->size) {
        if(io->mode & IO_MEMORY_MAPPED) {
            if(io->mode & IO_16_BIT)
                out[0] = ((volatile uint16_t *)io->base)[offset];
            else if(io->mode & IO_32_BIT)
                out[0] = ((volatile uint32_t *)io->base)[offset];
            else
                out[0] = ((volatile uint8_t *)io->base)[offset];
            return EOK;
        }

        if(io->mode & IO_PORT_MAPPED) {
            if(io->mode & IO_16_BIT)
                return pmio_read16(io->base + (offset * sizeof(uint16_t)), out);
            else if(io->mode & IO_32_BIT)
                return pmio_read32(io->base + (offset * sizeof(uint32_t)), out);
            return pmio_read8(io->base + offset, out);
        }

        return EINVAL;
    }

    return ERANGE;
}
EXPORT_SYMBOL(io_read);

int io_write(const io_block_t *restrict io, uintptr_t offset, uintmax_t value)
{
    if(offset < io->size) {
        if(io->mode & IO_MEMORY_MAPPED) {
            if(io->mode & IO_16_BIT)
                ((volatile uint16_t *)io->base)[offset] = value;
            else if(io->mode & IO_32_BIT)
                ((volatile uint32_t *)io->base)[offset] = value;
            else
                ((volatile uint8_t *)io->base)[offset] = value;
            return EOK;
        }

        if(io->mode & IO_PORT_MAPPED) {
            if(io->mode & IO_16_BIT)
                return pmio_write16(io->base + (offset * sizeof(uint16_t)), value);
            else if(io->mode & IO_32_BIT)
                return pmio_write32(io->base + (offset * sizeof(uint32_t)), value);
            return pmio_write8(io->base + offset, value);
        }

        return EINVAL;
    }

    return ERANGE;
}
EXPORT_SYMBOL(io_write);
