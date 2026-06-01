#ifndef B2DB992B_AE8D_4321_9537_2CA2CF4F59F5
#define B2DB992B_AE8D_4321_9537_2CA2CF4F59F5
#pragma once

#include <core/pmio.h>

static __always_inline inline uint8_t pmread8(uintptr_t offset)
{
    uint8_t value;
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"((uint16_t)(offset & 0xFFFF)) : "memory");
    return value;
}

static __always_inline inline uint16_t pmread16(uintptr_t offset)
{
    uint16_t value;
    asm volatile("inw %1, %0" : "=a"(value) : "Nd"((uint16_t)(offset & 0xFFFF)) : "memory");
    return value;
}

static __always_inline inline uint32_t pmread32(uintptr_t offset)
{
    uint32_t value;
    asm volatile("inl %1, %0" : "=a"(value) : "Nd"((uint16_t)(offset & 0xFFFF)) : "memory");
    return value;
}

static __always_inline inline void pmwrite8(uintptr_t offset, uint8_t value)
{
    asm volatile("outb %0, %1" ::"a"(value), "Nd"((uint16_t)(offset & 0xFFFF)) : "memory");
}

static __always_inline inline void pmwrite16(uintptr_t offset, uint16_t value)
{
    asm volatile("outw %0, %1" ::"a"(value), "Nd"((uint16_t)(offset & 0xFFFF)) : "memory");
}

static __always_inline inline void pmwrite32(uintptr_t offset, uint32_t value)
{
    asm volatile("outl %0, %1" ::"a"(value), "Nd"((uint16_t)(offset & 0xFFFF)) : "memory");
}

static __always_inline inline void pmread8_string(uintptr_t offset, uint8_t* restrict data, size_t count)
{
    asm volatile("rep insb" : "+D"(data), "+c"(count) : "d"((uint16_t)(offset & 0xFFFF)) : "memory");
}

static __always_inline inline void pmread16_string(uintptr_t offset, uint16_t* restrict data, size_t count)
{
    asm volatile("rep insw" : "+D"(data), "+c"(count) : "d"((uint16_t)(offset & 0xFFFF)) : "memory");
}

static __always_inline inline void pmread32_string(uintptr_t offset, uint32_t* restrict data, size_t count)
{
    asm volatile("rep insl" : "+D"(data), "+c"(count) : "d"((uint16_t)(offset & 0xFFFF)) : "memory");
}

static __always_inline inline void pmwrite8_string(uintptr_t offset, const uint8_t* restrict data, size_t count)
{
    asm volatile("rep outsb" : "+S"(data), "+c"(count) : "d"((uint16_t)(offset & 0xFFFF)) : "memory");
}

static __always_inline inline void pmwrite16_string(uintptr_t offset, const uint16_t* restrict data, size_t count)
{
    asm volatile("rep outsw" : "+S"(data), "+c"(count) : "d"((uint16_t)(offset & 0xFFFF)) : "memory");
}

static __always_inline inline void pmwrite32_string(uintptr_t offset, const uint32_t* restrict data, size_t count)
{
    asm volatile("rep outsl" : "+S"(data), "+c"(count) : "d"((uint16_t)(offset & 0xFFFF)) : "memory");
}

#endif /* B2DB992B_AE8D_4321_9537_2CA2CF4F59F5 */
