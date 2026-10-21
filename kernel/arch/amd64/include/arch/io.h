#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

static __always_inline uint8_t port_read8(uintptr_t offset)
{
    uint8_t value;
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"((uint16_t)(offset & 0xFFFF)) : "memory");
    return value;
}

static __always_inline uint16_t port_read16(uintptr_t offset)
{
    uint16_t value;
    asm volatile("inw %1, %0" : "=a"(value) : "Nd"((uint16_t)(offset & 0xFFFF)) : "memory");
    return value;
}

static __always_inline uint32_t port_read32(uintptr_t offset)
{
    uint32_t value;
    asm volatile("inl %1, %0" : "=a"(value) : "Nd"((uint16_t)(offset & 0xFFFF)) : "memory");
    return value;
}

static __always_inline void port_write8(uintptr_t offset, uint8_t value)
{
    asm volatile("outb %0, %1" ::"a"(value), "Nd"((uint16_t)(offset & 0xFFFF)) : "memory");
}

static __always_inline void port_write16(uintptr_t offset, uint16_t value)
{
    asm volatile("outw %0, %1" ::"a"(value), "Nd"((uint16_t)(offset & 0xFFFF)) : "memory");
}

static __always_inline void port_write32(uintptr_t offset, uint32_t value)
{
    asm volatile("outl %0, %1" ::"a"(value), "Nd"((uint16_t)(offset & 0xFFFF)) : "memory");
}

static __always_inline void port_read8s(uintptr_t offset, uint8_t* restrict data, size_t count)
{
    asm volatile("rep insb" : "+D"(data), "+c"(count) : "d"((uint16_t)(offset & 0xFFFF)) : "memory");
}

static __always_inline void port_read16s(uintptr_t offset, uint16_t* restrict data, size_t count)
{
    asm volatile("rep insw" : "+D"(data), "+c"(count) : "d"((uint16_t)(offset & 0xFFFF)) : "memory");
}

static __always_inline void port_read32s(uintptr_t offset, uint32_t* restrict data, size_t count)
{
    asm volatile("rep insl" : "+D"(data), "+c"(count) : "d"((uint16_t)(offset & 0xFFFF)) : "memory");
}

static __always_inline void port_write8s(uintptr_t offset, const uint8_t* restrict data, size_t count)
{
    asm volatile("rep outsb" : "+S"(data), "+c"(count) : "d"((uint16_t)(offset & 0xFFFF)) : "memory");
}

static __always_inline void port_write16s(uintptr_t offset, const uint16_t* restrict data, size_t count)
{
    asm volatile("rep outsw" : "+S"(data), "+c"(count) : "d"((uint16_t)(offset & 0xFFFF)) : "memory");
}

static __always_inline void port_write32s(uintptr_t offset, const uint32_t* restrict data, size_t count)
{
    asm volatile("rep outsl" : "+S"(data), "+c"(count) : "d"((uint16_t)(offset & 0xFFFF)) : "memory");
}
