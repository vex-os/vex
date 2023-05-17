/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#ifndef __ARCH_X86_64_INCLUDE_ARCH_PMIO_H__
#define __ARCH_X86_64_INCLUDE_ARCH_PMIO_H__
#include <kan/cdefs.h>
#include <stdint.h>

static __always_inline inline uint8_t pmio_read8(uintptr_t offset)
{
    uint8_t value;
    asm volatile("inb %1, %0":"=a"(value):"Nd"(offset & 0xFFFF):"memory");
    return value;
}

static __always_inline inline uint16_t pmio_read16(uintptr_t offset)
{
    uint16_t value;
    asm volatile("inw %1, %0":"=a"(value):"Nd"(offset & 0xFFFF):"memory");
    return value;
}

static __always_inline inline uint32_t pmio_read32(uintptr_t offset)
{
    uint32_t value;
    asm volatile("inl %1, %0":"=a"(value):"Nd"(offset & 0xFFFF):"memory");
    return value;
}

static __always_inline inline void pmio_write8(uintptr_t offset, uint8_t value)
{
    asm volatile("outb %0, %1"::"a"(value), "Nd"(offset & 0xFFFF):"memory");
}

static __always_inline inline void pmio_write16(uintptr_t offset, uint16_t value)
{
    asm volatile("outw %0, %1"::"a"(value), "Nd"(offset & 0xFFFF):"memory");
}

static __always_inline inline void pmio_write32(uintptr_t offset, uint32_t value)
{
    asm volatile("outl %0, %1"::"a"(value), "Nd"(offset & 0xFFFF):"memory");
}

static __always_inline inline void pmio_wait(void)
{
    /* The IO port 0x80 is used as a POST
     * code on an ISA-based PC-compatible but
     * certain devices can encroach on it, so...
     * FIXME: a more sophisticated pmio_wait() */
    asm volatile("outb %0, $0x80"::"a"(UINT8_C(0)):"memory");
}

#endif /* __ARCH_X86_64_INCLUDE_ARCH_PMIO_H__ */
