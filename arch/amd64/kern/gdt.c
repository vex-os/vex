#include <vex.amd64/gdt.h>
#include <vex.arch/hart.h>
#include <vex/errno.h>
#include <vex/initcall.h>
#include <vex/kprintf.h>
#include <vex/malloc.h>
#include <vex/panic.h>
#include <vex/string.h>

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

#define PREDEF_CODE_FLAGS (GDT_READWRITE | GDT_NONSYSTEM | GDT_EXECUTABLE)
#define PREDEF_DATA_FLAGS (GDT_READWRITE | GDT_NONSYSTEM)

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

void install_tss(struct amd64_gdt* restrict gdt, uintptr_t tss_base, size_t tss_size)
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

static void arch_init_gdt(void)
{
    struct amd64_gdt* gdt = kmalloc(sizeof(struct amd64_gdt));

    if(gdt == nullptr) {
        panic("out of memory");
    }

    struct hart_local* hart = this_hart();
    hart->amd64_gdtp = gdt;

    set_entry_16(gdt, GDT_KERN_CODE_16, 0x00000000, 0xFFFF, PREDEF_CODE_FLAGS | GDT_RING_0);
    set_entry_16(gdt, GDT_KERN_DATA_16, 0x00000000, 0xFFFF, PREDEF_DATA_FLAGS | GDT_RING_0);
    set_entry_32(gdt, GDT_KERN_CODE_32, 0x00000000, 0xFFFFF, PREDEF_CODE_FLAGS | GDT_RING_0);
    set_entry_32(gdt, GDT_KERN_DATA_32, 0x00000000, 0xFFFFF, PREDEF_DATA_FLAGS | GDT_RING_0);
    set_entry_64(gdt, GDT_KERN_CODE_64, PREDEF_CODE_FLAGS | GDT_RING_0);
    set_entry_64(gdt, GDT_KERN_DATA_64, PREDEF_DATA_FLAGS | GDT_RING_0);
    set_entry_64(gdt, GDT_USER_DATA_64, PREDEF_DATA_FLAGS | GDT_RING_3);
    set_entry_64(gdt, GDT_USER_CODE_64, PREDEF_CODE_FLAGS | GDT_RING_3);

    gdt->gdtr.size = (uint16_t)(sizeof(gdt->entries) - 1);
    gdt->gdtr.offset = (uintptr_t)(&gdt->entries[0]);

    asm volatile("lgdtq %0" ::"m"(gdt->gdtr));
}

initcall_smp_arch(arch_init_gdt, INIT_EARLY);
