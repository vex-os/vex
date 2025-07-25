// SPDX-License-Identifier: BSD-2-Clause
#ifndef INCLUDE_ARCH_PORTS_H
#define INCLUDE_ARCH_PORTS_H
#include <kern/compiler.h>
#include <stdint.h>

static __always_inline inline uint8_t pmio_read8(uintptr_t offset)
{
    uint8_t value;
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"((uint16_t)(offset & 0xFFFF)) : "memory");
    return value;
}

static __always_inline inline uint16_t pmio_read16(uintptr_t offset)
{
    uint16_t value;
    asm volatile("inw %1, %0" : "=a"(value) : "Nd"((uint16_t)(offset & 0xFFFF)) : "memory");
    return value;
}

static __always_inline inline uint32_t pmio_read32(uintptr_t offset)
{
    uint32_t value;
    asm volatile("inl %1, %0" : "=a"(value) : "Nd"((uint16_t)(offset & 0xFFFF)) : "memory");
    return value;
}

static __always_inline inline void pmio_write8(uintptr_t offset, uint8_t value)
{
    asm volatile("outb %0, %1" ::"a"(value), "Nd"((uint16_t)(offset & 0xFFFF)) : "memory");
}

static __always_inline inline void pmio_write16(uintptr_t offset, uint16_t value)
{
    asm volatile("outw %0, %1" ::"a"(value), "Nd"((uint16_t)(offset & 0xFFFF)) : "memory");
}

static __always_inline inline void pmio_write32(uintptr_t offset, uint32_t value)
{
    asm volatile("outl %0, %1" ::"a"(value), "Nd"((uint16_t)(offset & 0xFFFF)) : "memory");
}

static __always_inline inline void pmio_wait(void)
{
    // FIXME: this may startle a random ISA device
    // and cause the machine to shit itself... Though
    // it may not happen at all on a 64-bit system!
    asm volatile("outb %0, $0x80" ::"a"(UINT8_C(0)) : "memory");
}

#endif // INCLUDE_ARCH_PORTS_H
