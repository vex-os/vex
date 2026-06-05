#ifndef C0E501A6_2C96_4C65_8667_EC228CA1B283
#define C0E501A6_2C96_4C65_8667_EC228CA1B283
#pragma once

#include <core/cdefs.h>
#include <core/types.h>

#define GDT_NULL_SEGMENT 0x00
#define GDT_KERN_CODE_16 0x01
#define GDT_KERN_DATA_16 0x02
#define GDT_KERN_CODE_32 0x03
#define GDT_KERN_DATA_32 0x04
#define GDT_KERN_CODE_64 0x05
#define GDT_KERN_DATA_64 0x06
#define GDT_USER_CODE_64 0x07
#define GDT_USER_DATA_64 0x08
#define GDT_TSS          0x09

struct amd64_gdt;

void gdt_install_tss(struct amd64_gdt* restrict gdt, uintptr_t tss_base, size_t tss_size);

static __always_inline __nodiscard uint16_t gdt_selector(uint16_t index, uint16_t ldt, uint16_t ring)
{
    return ((ring & 4) | ((ldt & 1) << 2) | (index << 3));
}

#endif /* C0E501A6_2C96_4C65_8667_EC228CA1B283 */
