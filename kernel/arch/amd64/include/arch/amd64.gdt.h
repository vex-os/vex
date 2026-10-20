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

typedef struct amd64_gdt_descriptor_s amd64_gdt_descriptor_t;
typedef struct amd64_tss_descriptor_s amd64_tss_descriptor_t;
typedef struct amd64_gdt_s amd64_gdt_t;

struct amd64_gdt_descriptor_s {
    uint16_t limit_0;
    uint16_t base_0;
    uint8_t base_1;
    uint8_t flags_0;
    uint8_t limit_1 : 4;
    uint8_t flags_1 : 4;
    uint8_t base_2;
} __packed;

struct amd64_tss_descriptor_s {
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

struct amd64_gdt_s {
    amd64_gdt_descriptor_t null;
    amd64_gdt_descriptor_t kern_code_16;
    amd64_gdt_descriptor_t kern_data_16;
    amd64_gdt_descriptor_t kern_code_32;
    amd64_gdt_descriptor_t kern_data_32;
    amd64_gdt_descriptor_t kern_code_64;
    amd64_gdt_descriptor_t kern_data_64;
    amd64_gdt_descriptor_t user_data_64;
    amd64_gdt_descriptor_t user_code_64;
    amd64_tss_descriptor_t tss;
} __packed;

#define gdt_offset(index) ((index) * sizeof(amd64_gdt_descriptor_t))

static_assert(gdt_offset(GDT_NULL_SEGMENT) == offsetof(amd64_gdt_t, null));
static_assert(gdt_offset(GDT_KERN_CODE_16) == offsetof(amd64_gdt_t, kern_code_16));
static_assert(gdt_offset(GDT_KERN_DATA_16) == offsetof(amd64_gdt_t, kern_data_16));
static_assert(gdt_offset(GDT_KERN_CODE_32) == offsetof(amd64_gdt_t, kern_code_32));
static_assert(gdt_offset(GDT_KERN_DATA_32) == offsetof(amd64_gdt_t, kern_data_32));
static_assert(gdt_offset(GDT_KERN_CODE_64) == offsetof(amd64_gdt_t, kern_code_64));
static_assert(gdt_offset(GDT_KERN_DATA_64) == offsetof(amd64_gdt_t, kern_data_64));
static_assert(gdt_offset(GDT_USER_DATA_64) == offsetof(amd64_gdt_t, user_data_64));
static_assert(gdt_offset(GDT_USER_CODE_64) == offsetof(amd64_gdt_t, user_code_64));
static_assert(gdt_offset(GDT_TSS) == offsetof(amd64_gdt_t, tss));

#define gdt_selector(index, ldt, ring) ((uint16_t)(((ring) & 3) | (((ldt) & 1) << 2) | ((index) << 3)))

static_assert(0x43 == gdt_selector(GDT_USER_CODE_64, 0, 3));
static_assert(0x3B == gdt_selector(GDT_USER_DATA_64, 0, 3));
