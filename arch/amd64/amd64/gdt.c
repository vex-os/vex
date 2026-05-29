// SPDX-License-Identifier: BSD-3-Clause

#include <amd64/gdt.h>
#include <arch/hart.h>
#include <core/assert.h>
#include <core/errno.h>
#include <core/kprintf.h>
#include <init/initcall.h>
#include <lib/string.h>
#include <mm/malloc.h>

#define GDT_READWRITE  (1 << 1)
#define GDT_CONFORMING (1 << 2)
#define GDT_EXECUTABLE (1 << 3)
#define GDT_NONSYSTEM  (1 << 4)
#define GDT_RING_0     (0 << 5)
#define GDT_RING_3     (3 << 5)
#define GDT_PRESENT    (1 << 7)
#define GDT_4KIB_UNITS (1 << 3)
#define GDT_32BIT      (1 << 2)
#define GDT_64BIT      (1 << 1)

#define GDT_ENTRY_COUNT 16

struct gdt_entry {
    uint16_t limit_0;
    uint16_t base_0;
    uint8_t base_1;
    uint8_t flags_0;
    uint8_t limit_1 : 4;
    uint8_t flags_1 : 4;
    uint8_t base_2;
} __packed;

struct tss_descriptor {
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

struct amd64_gdt {
    struct gdt_entry entries[GDT_ENTRY_COUNT];
    struct {
        uint16_t size;
        uintptr_t offset;
    } __packed gdtr;
};

static void set_entry_16(struct amd64_gdt* restrict gdt, uint8_t id, uint32_t base, uint16_t limit, uint8_t flags)
{
    struct gdt_entry entry = { 0 };
    entry.limit_0 = limit;
    entry.base_0 = base & 0xFFFF;
    entry.base_1 = (base >> 16) & 0xFF;
    entry.base_2 = (base >> 24) & 0xFF;
    entry.flags_0 = flags | GDT_PRESENT;

    memcpy(gdt->entries + id, &entry, sizeof(entry));
}

static void set_entry_32(struct amd64_gdt* restrict gdt, uint8_t id, uint32_t base, uint32_t limit, uint8_t flags)
{
    struct gdt_entry entry = { 0 };
    entry.limit_0 = limit & 0xFFFF;
    entry.limit_1 = (limit >> 16) & 0x0F;
    entry.base_0 = base & 0xFFFF;
    entry.base_1 = (base >> 16) & 0xFF;
    entry.base_2 = (base >> 24) & 0xFF;
    entry.flags_0 = flags | GDT_PRESENT;
    entry.flags_1 = GDT_4KIB_UNITS | GDT_32BIT;

    memcpy(gdt->entries + id, &entry, sizeof(entry));
}

static void set_entry_64(struct amd64_gdt* restrict gdt, uint8_t id, uint8_t flags)
{
    struct gdt_entry entry = { 0 };
    entry.flags_0 = flags | GDT_PRESENT;
    entry.flags_1 = GDT_64BIT;

    memcpy(gdt->entries + id, &entry, sizeof(entry));
}

void gdt_install_tss(struct amd64_gdt* restrict gdt, uintptr_t tss_base, size_t tss_size)
{
    struct tss_descriptor descriptor = { 0 };

    descriptor.limit_0 = (tss_size - 1) & 0xFFFF;
    descriptor.limit_1 = ((tss_size - 1) >> 16) & 0x0F;
    descriptor.base_0 = tss_base & 0xFFFF;
    descriptor.base_1 = (tss_base >> 16) & 0xFF;
    descriptor.base_2 = (tss_base >> 24) & 0xFF;
    descriptor.base_3 = (tss_base >> 32) & 0xFFFFFFFF;
    descriptor.flags = 0x09 | (1 << 7);

    memcpy(gdt->entries + GDT_TSS, &descriptor, sizeof(descriptor));
}

define_initcall(amd64_gdt, INITCALL_STAGE_SMP_ARCH, INITCALL_ORDER_EARLY)
{
    uint8_t code_flags = GDT_READWRITE | GDT_NONSYSTEM | GDT_EXECUTABLE;
    uint8_t data_flags = GDT_READWRITE | GDT_NONSYSTEM;
    struct hart_local* hart = this_hart();
    struct amd64_gdt* gdt;

    gdt = kmalloc(sizeof(struct amd64_gdt));
    strict_kassert_text(gdt, "out of memory");

    memset(gdt, 0, sizeof(struct amd64_gdt));

    hart->amd64.gdt = gdt;

    set_entry_16(gdt, GDT_KERN_CODE_16, 0x00000000, 0xFFFF, code_flags | GDT_RING_0);
    set_entry_16(gdt, GDT_KERN_DATA_16, 0x00000000, 0xFFFF, data_flags | GDT_RING_0);
    set_entry_32(gdt, GDT_KERN_CODE_32, 0x00000000, 0xFFFFF, code_flags | GDT_RING_0);
    set_entry_32(gdt, GDT_KERN_DATA_32, 0x00000000, 0xFFFFF, data_flags | GDT_RING_0);
    set_entry_64(gdt, GDT_KERN_CODE_64, code_flags | GDT_RING_0);
    set_entry_64(gdt, GDT_KERN_DATA_64, data_flags | GDT_RING_0);
    set_entry_64(gdt, GDT_USER_CODE_64, code_flags | GDT_RING_3);
    set_entry_64(gdt, GDT_USER_DATA_64, data_flags | GDT_RING_3);

    gdt->gdtr.size = (uint16_t)(sizeof(gdt->entries) - 1);
    gdt->gdtr.offset = (uintptr_t)(&gdt->entries[0]);

    asm volatile("lgdtq %0" ::"m"(gdt->gdtr));

    pr_inform("gdt: hart=%u gdtr.size=%zu gdtr.offset=%p", hart->index, (size_t)(gdt->gdtr.size), (const void*)(gdt->gdtr.offset));
}
