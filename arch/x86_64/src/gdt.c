/* SPDX-License-Identifier: Zlib */
#include <arch/gdt.h>
#include <core/printf.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

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

struct gdt_register {
    uint16_t size;
    uintptr_t offset;
} __packed;

static struct gdt_entry gdt[16] = { 0 };
static struct gdt_register gdtr = { 0 };

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

void init_gdt(void)
{
    uint8_t code_flags = GDT_READWRITE | GDT_NONSYSTEM | GDT_EXECUTABLE;
    uint8_t data_flags = GDT_READWRITE | GDT_NONSYSTEM;

    memset(gdt, 0, sizeof(gdt));

    set_entry_16(GDT_KERN_CODE_16, 0x00000000, 0xFFFF, code_flags | GDT_RING_0);
    set_entry_16(GDT_KERN_DATA_16, 0x00000000, 0xFFFF, data_flags | GDT_RING_0);
    set_entry_32(GDT_KERN_CODE_32, 0x00000000, 0xFFFFF, code_flags | GDT_RING_0);
    set_entry_32(GDT_KERN_DATA_32, 0x00000000, 0xFFFFF, data_flags | GDT_RING_0);
    set_entry_64(GDT_KERN_CODE_64, code_flags | GDT_RING_0);
    set_entry_64(GDT_KERN_DATA_64, data_flags | GDT_RING_0);
    set_entry_64(GDT_USER_CODE_64, code_flags | GDT_RING_3);
    set_entry_64(GDT_USER_DATA_64, data_flags | GDT_RING_3);

    gdtr.size = (uint16_t)(sizeof(gdt) - 1);
    gdtr.offset = (uintptr_t)(&gdt[0]);

    asm volatile("lgdtq %0"::"m"(gdtr));

    kprintf(KP_DEBUG, "gdt: gdtr.size=%zu", (size_t)(gdtr.size));
    kprintf(KP_DEBUG, "gdt: gdtr.offset=%p", (void *)(gdtr.offset));
}
