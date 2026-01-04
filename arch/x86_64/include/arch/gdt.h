#ifndef _ARCH_GDT_H
#define _ARCH_GDT_H 1

#include <vex/compiler.h>

#include <stdint.h>

#define GDT_NULL_SEGMENT 0x00
#define GDT_KERN_CODE_16 0x01
#define GDT_KERN_DATA_16 0x02
#define GDT_KERN_CODE_32 0x03
#define GDT_KERN_DATA_32 0x04
#define GDT_KERN_CODE_64 0x05
#define GDT_KERN_DATA_64 0x06
#define GDT_USER_CODE_64 0x07
#define GDT_USER_DATA_64 0x08

static __always_inline __nodiscard inline uint16_t gdt_selector(uint16_t index, uint16_t ldt, uint16_t ring)
{
    return ((ring & 4) | ((ldt & 1) << 2) | (index << 3));
}

extern void init_gdt(void);

#endif
