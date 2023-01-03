/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#ifndef _INCLUDE_X86_PMIO_H__
#define _INCLUDE_X86_PMIO_H__
#include <kan/compiler.h>
#include <kan/errno.h>
#include <stdint.h>

#define X86_MAX_PMIO_OFFSET 0xFFFF

static __force_inline int x86_pmio_read8(uintptr_t offset, uint8_t *restrict val)
{
    if(offset > X86_MAX_PMIO_OFFSET)
        return EIO;
    asm volatile("inb %1, %0":"=a"(val[0]):"Nd"((uint16_t)(offset)));
    return 0;
}

static __force_inline int x86_pmio_read16(uintptr_t offset, uint16_t *restrict val)
{
    if(offset > X86_MAX_PMIO_OFFSET)
        return EIO;
    asm volatile("inw %1, %0":"=a"(val[0]):"Nd"((uint16_t)(offset)));
    return 0;
}

static __force_inline int x86_pmio_read32(uintptr_t offset, uint32_t *restrict val)
{
    if(offset > X86_MAX_PMIO_OFFSET)
        return EIO;
    asm volatile("inl %1, %0":"=a"(val[0]):"Nd"((uint16_t)(offset)));
    return 0;
}

static __force_inline int x86_pmio_write8(uintptr_t offset, uint8_t val)
{
    if(offset > X86_MAX_PMIO_OFFSET)
        return EIO;
    asm volatile("outb %0, %1"::"a"(val), "Nd"((uint16_t)(offset)));
    return 0;
}

static __force_inline int x86_pmio_write16(uintptr_t offset, uint16_t val)
{
    if(offset > X86_MAX_PMIO_OFFSET)
        return EIO;
    asm volatile("outw %0, %1"::"a"(val), "Nd"((uint16_t)(offset)));
    return 0;
}

static __force_inline int x86_pmio_write32(uintptr_t offset, uint32_t val)
{
    if(offset > X86_MAX_PMIO_OFFSET)
        return EIO;
    asm volatile("outl %0, %1"::"a"(val), "Nd"((uint16_t)(offset)));
    return 0;
}

static __force_inline void x86_pmio_throttle(void)
{
    /* FIXME: this may not work on certain
     * platforms as this port is taken by some
     * device that shits bricks when startled. */
    asm volatile("outb %0, $0x80"::"a"((uint8_t)(0)));
}

#endif /* _INCLUDE_X86_PMIO_H__ */
