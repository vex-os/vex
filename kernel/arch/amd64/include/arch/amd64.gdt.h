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

struct amd64_gdt_descriptor {
    uint16_t limit_0;
    uint16_t base_0;
    uint8_t base_1;
    uint8_t flags_0;
    uint8_t limit_1 : 4;
    uint8_t flags_1 : 4;
    uint8_t base_2;
} __packed;

struct amd64_tss_descriptor {
    uint16_t limit_0;
    uint16_t base_0;
    uint8_t base_1;
    uint8_t flags;
    uint8_t limit_1 : 4;
    uint8_t flags_1 : 4;
    uint8_t base_2;
    uint32_t base_3;
    uint32_t reserved;
} __packed;

// Laid out in GDT_* index order
struct amd64_gdt {
    struct amd64_gdt_descriptor null;
    struct amd64_gdt_descriptor kern_code_16;
    struct amd64_gdt_descriptor kern_data_16;
    struct amd64_gdt_descriptor kern_code_32;
    struct amd64_gdt_descriptor kern_data_32;
    struct amd64_gdt_descriptor kern_code_64;
    struct amd64_gdt_descriptor kern_data_64;
    struct amd64_gdt_descriptor user_data_64;
    struct amd64_gdt_descriptor user_code_64;
    struct amd64_tss_descriptor tss;
} __packed;

#define gdt_selector(index, ldt, ring) ((uint16_t)(((ring) & 3) | (((ldt) & 1) << 2) | ((index) << 3)))

static_assert(0x43 == gdt_selector(GDT_USER_CODE_64, 0, 3));
static_assert(0x3B == gdt_selector(GDT_USER_DATA_64, 0, 3));
