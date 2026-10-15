#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

#define GDT_NULL_SEGMENT 0x00
#define GDT_KERN_CODE_16 0x01
#define GDT_KERN_DATA_16 0x02
#define GDT_KERN_CODE_32 0x03
#define GDT_KERN_DATA_32 0x04
#define GDT_KERN_CODE_64 0x05
#define GDT_KERN_DATA_64 0x06
#define GDT_USER_DATA_64 0x07
#define GDT_USER_CODE_64 0x08
#define GDT_TSS          0x09

#define gdt_selector(index, ldt, ring) ((uint16_t)(((ring) & 3) | (((ldt) & 1) << 2) | ((index) << 3)))

static_assert(0x43 == gdt_selector(GDT_USER_CODE_64, 0, 3));
static_assert(0x3B == gdt_selector(GDT_USER_DATA_64, 0, 3));
