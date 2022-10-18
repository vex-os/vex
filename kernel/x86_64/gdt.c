/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#include <kaneru/initcall.h>
#include <kaneru/kprintf.h>
#include <stdint.h>
#include <string.h>
#include <x86_64/gdt.h>

#define GDT_READWRITE   (1 << 1)
#define GDT_CONFORMING  (1 << 2)
#define GDT_EXECUTABLE  (1 << 3)
#define GDT_NONSYSTEM   (1 << 4)
#define GDT_RING_0      (0 << 5)
#define GDT_RING_3      (3 << 5)
#define GDT_PRESENT     (1 << 7)
#define GDT_4KIB_UNITS  (1 << 3)
#define GDT_32BIT       (1 << 2)
#define GDT_64BIT       (1 << 1)

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

static void set_entry_16(uint8_t id, uint32_t base, uint16_t limit, uint8_t flags)
{
    struct gdt_entry entry = { 0 };
    entry.limit_0 = limit;
    entry.base_0 = base & 0xFFFF;
    entry.base_1 = (base >> 16) & 0xFF;
    entry.base_2 = (base >> 24) & 0xFF;
    entry.flags_0 = flags | GDT_PRESENT;
    memcpy(gdt + id, &entry, sizeof(entry));
}

static void set_entry_32(uint8_t id, uint32_t base, uint32_t limit, uint8_t flags)
{
    struct gdt_entry entry = { 0 };
    entry.limit_0 = limit & 0xFFFF;
    entry.limit_1 = (limit >> 16) & 0x0F;
    entry.base_0 = base & 0xFFFF;
    entry.base_1 = (base >> 16) & 0xFF;
    entry.base_2 = (base >> 24) & 0xFF;
    entry.flags_0 = flags | GDT_PRESENT;
    entry.flags_1 = GDT_4KIB_UNITS | GDT_32BIT;
    memcpy(gdt + id, &entry, sizeof(entry));
}

static void set_entry_64(uint8_t id, uint8_t flags)
{
    struct gdt_entry entry = { 0 };
    entry.flags_0 = flags | GDT_PRESENT;
    entry.flags_1 = GDT_64BIT;
    memcpy(gdt + id, &entry, sizeof(entry));
}

static int init_gdt(void)
{
    uint8_t code_flags = GDT_READWRITE | GDT_NONSYSTEM | GDT_EXECUTABLE;
    uint8_t data_flags = GDT_READWRITE | GDT_NONSYSTEM;

    memset(gdt, 0, sizeof(gdt));

    set_entry_16(GDT_INDEX_KERN_CODE_16, 0x00000000, 0xFFFF, code_flags | GDT_RING_0);
    set_entry_16(GDT_INDEX_KERN_DATA_16, 0x00000000, 0xFFFF, data_flags | GDT_RING_0);
    set_entry_32(GDT_INDEX_KERN_CODE_32, 0x00000000, 0xFFFFF, code_flags | GDT_RING_0);
    set_entry_32(GDT_INDEX_KERN_DATA_32, 0x00000000, 0xFFFFF, data_flags | GDT_RING_0);
    set_entry_64(GDT_INDEX_KERN_CODE_64, code_flags | GDT_RING_0);
    set_entry_64(GDT_INDEX_KERN_DATA_64, data_flags | GDT_RING_0);
    set_entry_64(GDT_INDEX_USER_CODE_64, code_flags | GDT_RING_3);
    set_entry_64(GDT_INDEX_USER_DATA_64, data_flags | GDT_RING_3);

    gdt_ptr.limit = (uint16_t)(sizeof(gdt) - 1);
    gdt_ptr.base = (uintptr_t)(&gdt[0]);

    asm volatile(__string_va(
        lgdtq %0;
    )::"m"(gdt_ptr));

    /* Shake up data segments */
    asm volatile(__string_va(
        movw %%ax, %%ds;
        movw %%ax, %%es;
        movw %%ax, %%fs;
        movw %%ax, %%gs;
        movw %%ax, %%ss;
    )::"a"(GDT_SELECTOR(GDT_INDEX_KERN_DATA_64, 0, 0)));

    /* Far jump to the new segment */
    asm volatile(__string_va(
        pushq %0;
        pushq $1f;
        lretq;
        1:;
    )::"i"(GDT_SELECTOR(GDT_INDEX_KERN_CODE_64, 0, 0)));

    kprintf(KP_MACHINE, "gdt: limit=%hu, base=%p", gdt_ptr.limit, (void *)gdt_ptr.base);

    return 0;
}
initcall_tier_0(gdt, init_gdt);
