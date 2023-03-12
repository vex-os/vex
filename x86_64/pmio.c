/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#include <kan/errno.h>
#include <x86_64/pmio.h>

int pmio_read8(uintptr_t offset, uint8_t *restrict value)
{
    if(offset <= PMIO_MAX_OFFSET) {
        asm volatile("inb %1, %0":"=a"(value[0]):"Nd"(offset));
        return 0;
    }

    return EIO;
}

int pmio_read16(uintptr_t offset, uint16_t *restrict value)
{
    if(offset <= PMIO_MAX_OFFSET) {
        asm volatile("inw %1, %0":"=a"(value[0]):"Nd"(offset));
        return 0;
    }

    return EIO;
}

int pmio_read32(uintptr_t offset, uint32_t *restrict value)
{
    if(offset <= PMIO_MAX_OFFSET) {
        asm volatile("inl %1, %0":"=a"(value[0]):"Nd"(offset));
        return 0;
    }

    return EIO;
}

int pmio_write8(uintptr_t offset, uint8_t value)
{
    if(offset <= PMIO_MAX_OFFSET) {
        asm volatile("outb %0, %1"::"a"(value), "Nd"(offset));
        return 0;
    }

    return EIO;
}

int pmio_write16(uintptr_t offset, uint16_t value)
{
    if(offset <= PMIO_MAX_OFFSET) {
        asm volatile("outw %0, %1"::"a"(value), "Nd"(offset));
        return 0;
    }

    return EIO;
}

int pmio_write32(uintptr_t offset, uint32_t value)
{
    if(offset <= PMIO_MAX_OFFSET) {
        asm volatile("outl %0, %1"::"a"(value), "Nd"(offset));
        return 0;
    }

    return EIO;
}

void pmio_wait(void)
{
    // FIXME: linux has a lot more here...
    asm volatile("outb %0, $0x80"::"a"(0));
}