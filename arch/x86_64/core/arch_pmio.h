#ifndef B9B6C109_D39A_40E0_B52E_B374A8730AC2
#define B9B6C109_D39A_40E0_B52E_B374A8730AC2

#include <core/cdefs.h>

#include <stdint.h>

static __always_inline inline uint8_t arch_pmread8(uintptr_t offset)
{
    uint8_t value;
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"((uint16_t)(offset & 0xFFFF)) : "memory");
    return value;
}

static __always_inline inline uint16_t arch_pmread16(uintptr_t offset)
{
    uint16_t value;
    asm volatile("inw %1, %0" : "=a"(value) : "Nd"((uint16_t)(offset & 0xFFFF)) : "memory");
    return value;
}

static __always_inline inline uint32_t arch_pmread32(uintptr_t offset)
{
    uint32_t value;
    asm volatile("inl %1, %0" : "=a"(value) : "Nd"((uint16_t)(offset & 0xFFFF)) : "memory");
    return value;
}

static __always_inline inline void arch_pmwrite8(uintptr_t offset, uint8_t value)
{
    asm volatile("outb %0, %1" ::"a"(value), "Nd"((uint16_t)(offset & 0xFFFF)) : "memory");
}

static __always_inline inline void arch_pmwrite16(uintptr_t offset, uint16_t value)
{
    asm volatile("outw %0, %1" ::"a"(value), "Nd"((uint16_t)(offset & 0xFFFF)) : "memory");
}

static __always_inline inline void arch_pmwrite32(uintptr_t offset, uint32_t value)
{
    asm volatile("outl %0, %1" ::"a"(value), "Nd"((uint16_t)(offset & 0xFFFF)) : "memory");
}

static __always_inline inline void arch_pmwait(void)
{
    // FIXME: this may startle a random ISA device
    // and cause the machine to shit itself... Though
    // it shoudln't really happen at all on a 64-bit system!
    asm volatile("outb %0, $0x80" ::"a"(UINT8_C(0)) : "memory");
}

#endif /* B9B6C109_D39A_40E0_B52E_B374A8730AC2 */
