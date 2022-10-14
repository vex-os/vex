/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#include <stdint.h>
#include <string.h>
#include <sys/kern/initcall.h>
#include <sys/kern/klog.h>
#include <sys/x86_64/segment.h>

#define SEG_READWRITE   (1 << 1)
#define SEG_CONFORMING  (1 << 2)
#define SEG_EXECUTABLE  (1 << 3)
#define SEG_NONSYSTEM   (1 << 4)
#define SEG_RING_0      (0 << 5)
#define SEG_RING_3      (3 << 5)
#define SEG_PRESENT     (1 << 7)
#define SEG_4KIB_UNITS  (1 << 3)
#define SEG_32BIT       (1 << 2)
#define SEG_64BIT       (1 << 1)

struct gdt_entry {
    uint16_t limit_0;
    uint16_t base_0;
    uint8_t base_1;
    uint8_t flags_0;
    uint8_t limit_1 : 4;
    uint8_t flags_1 : 4;
    uint8_t base_2;
} __packed;

struct gdt_pointer {
    uint16_t limit;
    uintptr_t base;
} __packed;

static struct gdt_entry gdt[16] = { 0 };
static struct gdt_pointer gdt_ptr = { 0 };

void set_entry_16(uint8_t id, uint32_t base, uint16_t limit, uint8_t flags)
{
    struct gdt_entry entry = { 0 };
    entry.limit_0 = limit;
    entry.base_0 = base & 0xFFFF;
    entry.base_1 = (base >> 16) & 0xFF;
    entry.base_2 = (base >> 24) & 0xFF;
    entry.flags_0 = flags | SEG_PRESENT;
    memcpy(gdt + id, &entry, sizeof(entry));
}

void set_entry_32(uint8_t id, uint32_t base, uint32_t limit, uint8_t flags)
{
    struct gdt_entry entry = { 0 };
    entry.limit_0 = limit & 0xFFFF;
    entry.limit_1 = (limit >> 16) & 0x0F;
    entry.base_0 = base & 0xFFFF;
    entry.base_1 = (base >> 16) & 0xFF;
    entry.base_2 = (base >> 24) & 0xFF;
    entry.flags_0 = flags | SEG_PRESENT;
    entry.flags_1 = SEG_4KIB_UNITS | SEG_32BIT;
    memcpy(gdt + id, &entry, sizeof(entry));
}

void set_entry_64(uint8_t id, uint8_t flags)
{
    struct gdt_entry entry = { 0 };
    entry.flags_0 = flags | SEG_PRESENT;
    entry.flags_1 = SEG_64BIT;
    memcpy(gdt + id, &entry, sizeof(entry));
}

static void init_segment(void)
{
    uint8_t code_flags = SEG_READWRITE | SEG_NONSYSTEM | SEG_EXECUTABLE;
    uint8_t data_flags = SEG_READWRITE | SEG_NONSYSTEM;

    memset(gdt, 0, sizeof(gdt));

    set_entry_16(SEG_INDEX_KERN_CODE_16, 0x00000000, 0xFFFF, code_flags | SEG_RING_0);
    set_entry_16(SEG_INDEX_KERN_DATA_16, 0x00000000, 0xFFFF, data_flags | SEG_RING_0);
    set_entry_32(SEG_INDEX_KERN_CODE_32, 0x00000000, 0xFFFFF, code_flags | SEG_RING_0);
    set_entry_32(SEG_INDEX_KERN_DATA_32, 0x00000000, 0xFFFFF, data_flags | SEG_RING_0);
    set_entry_64(SEG_INDEX_KERN_CODE_64, code_flags | SEG_RING_0);
    set_entry_64(SEG_INDEX_KERN_DATA_64, data_flags | SEG_RING_0);
    set_entry_64(SEG_INDEX_USER_CODE_64, code_flags | SEG_RING_3);
    set_entry_64(SEG_INDEX_USER_DATA_64, data_flags | SEG_RING_3);

    gdt_ptr.limit = (uint16_t)(sizeof(gdt) - 1);
    gdt_ptr.base = (uintptr_t)(&gdt[0]);

    asm volatile("lgdtq %0"::"m"(gdt_ptr));
    asm volatile(
        "movw %1,   %%ax\n"
        "movw %%ax, %%ds\n"
        "movw %%ax, %%es\n"
        "movw %%ax, %%fs\n"
        "movw %%ax, %%gs\n"
        "movw %%ax, %%ss\n"
        "pushq %0       \n"
        "pushq $1f      \n"
        "retfq          \n"
        "1:             \n"
        :
        : "i"(SEG_SELECTOR(SEG_INDEX_KERN_CODE_64, 0, 0)), "i"(SEG_SELECTOR(SEG_INDEX_KERN_DATA_64, 0, 0))
    );

    klog(KL_MACHINE, "gdt limit=%hu, base=%p", gdt_ptr.limit, (void *)gdt_ptr.base);
}
initcall_tier_0(segment, init_segment);
