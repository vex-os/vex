#include <vex.amd64/gdt.h>
#include <vex/errno.h>
#include <vex/hart.h>
#include <vex/initcall.h>
#include <vex/kmalloc.h>
#include <vex/kprintf.h>
#include <vex/panic.h>
#include <vex/spinlock.h>
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

#define PREDEF_CODE_FLAGS (GDT_READWRITE | GDT_NONSYSTEM | GDT_EXECUTABLE)
#define PREDEF_DATA_FLAGS (GDT_READWRITE | GDT_NONSYSTEM)

struct gdt_descriptor {
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

struct gdt_table {
    struct gdt_descriptor null;
    struct gdt_descriptor kern_code_16;
    struct gdt_descriptor kern_data_16;
    struct gdt_descriptor kern_code_32;
    struct gdt_descriptor kern_data_32;
    struct gdt_descriptor kern_code_64;
    struct gdt_descriptor kern_data_64;
    struct gdt_descriptor user_data_64;
    struct gdt_descriptor user_code_64;
    struct tss_descriptor tss;
} __packed;

struct gdt_register {
    uint16_t size;
    uint64_t offset;
} __packed;

static struct gdt_table gdt;
static struct gdt_register gdtr;
static spinlock_type gdt_lock = SPINLOCK_INIT;

static void write_entry_16(struct gdt_descriptor* restrict entry, uint8_t id, uint32_t base, uint16_t limit, uint8_t flags)
{
    entry->limit_0 = limit;
    entry->base_0 = base & 0xFFFF;
    entry->base_1 = (base >> 16) & 0xFF;
    entry->base_2 = (base >> 24) & 0xFF;
    entry->flags_0 = flags | GDT_PRESENT;
}

static void write_entry_32(struct gdt_descriptor* restrict entry, uint8_t id, uint32_t base, uint32_t limit, uint8_t flags)
{
    entry->limit_0 = limit & 0xFFFF;
    entry->limit_1 = (limit >> 16) & 0x0F;
    entry->base_0 = base & 0xFFFF;
    entry->base_1 = (base >> 16) & 0xFF;
    entry->base_2 = (base >> 24) & 0xFF;
    entry->flags_0 = flags | GDT_PRESENT;
    entry->flags_1 = GDT_4KIB_UNITS | GDT_32BIT;
}

static void write_entry_64(struct gdt_descriptor* restrict entry, uint8_t id, uint8_t flags)
{
    entry->flags_0 = flags | GDT_PRESENT;
    entry->flags_1 = GDT_64BIT;
}

static void reload_gdt(void)
{
    asm volatile("lgdtq %0" : : "m"(gdtr) : "memory");

    asm volatile("pushq %0" : : "i"(gdt_selector(GDT_KERN_CODE_64, 0, 0)) : "memory");
    asm volatile("leaq 1f(%%rip), %%rax" : : : "rax");
    asm volatile("pushq %%rax" : : : "memory");
    asm volatile("lretq\n\t1:" : : : "memory");

    asm volatile("movl %0, %%eax" : : "i"(gdt_selector(GDT_KERN_DATA_64, 0, 0)) : "eax");
    asm volatile("movw %%ax, %%ds" : : : "memory");
    asm volatile("movw %%ax, %%es" : : : "memory");
    asm volatile("movw %%ax, %%ss" : : : "memory");
}

static void arch_init_gdt_bsp(void)
{
    memset(&gdt, 0, sizeof(gdt));
    write_entry_16(&gdt.kern_code_16, GDT_KERN_CODE_16, 0x00000000, 0xFFFF, PREDEF_CODE_FLAGS | GDT_RING_0);
    write_entry_16(&gdt.kern_data_16, GDT_KERN_DATA_16, 0x00000000, 0xFFFF, PREDEF_DATA_FLAGS | GDT_RING_0);
    write_entry_32(&gdt.kern_code_32, GDT_KERN_CODE_32, 0x00000000, 0xFFFFF, PREDEF_CODE_FLAGS | GDT_RING_0);
    write_entry_32(&gdt.kern_data_32, GDT_KERN_DATA_32, 0x00000000, 0xFFFFF, PREDEF_DATA_FLAGS | GDT_RING_0);
    write_entry_64(&gdt.kern_code_64, GDT_KERN_CODE_64, PREDEF_CODE_FLAGS | GDT_RING_0);
    write_entry_64(&gdt.kern_data_64, GDT_KERN_DATA_64, PREDEF_DATA_FLAGS | GDT_RING_0);
    write_entry_64(&gdt.user_data_64, GDT_USER_DATA_64, PREDEF_DATA_FLAGS | GDT_RING_3);
    write_entry_64(&gdt.user_code_64, GDT_USER_CODE_64, PREDEF_CODE_FLAGS | GDT_RING_3);

    memset(&gdtr, 0, sizeof(gdtr));
    gdtr.size = sizeof(gdt) - 1;
    gdtr.offset = (uintptr_t)(&gdt);

    reload_gdt();
}

initcall_bsp_arch(arch_init_gdt_bsp, INIT_EARLY);

static void arch_init_gdt_smp(void)
{
    struct hart_local* hart = this_hart();

    spinlock_acquire(&gdt_lock);

    reload_gdt();

    uintptr_t tss_base = (uintptr_t)(&hart->arch.tss);

    gdt.tss.limit_0 = sizeof(hart->arch.tss) - 1;
    gdt.tss.base_0 = (tss_base & 0xFFFF);
    gdt.tss.base_1 = (tss_base >> 16) & 0xFF;
    gdt.tss.base_2 = (tss_base >> 24) & 0xFF;
    gdt.tss.base_3 = (tss_base >> 32) & 0xFFFFFFFF;
    gdt.tss.flags = 0x89;

    asm volatile("ltr %0" : : "r"(gdt_selector(GDT_TSS, 0, 0)) : "memory");

    spinlock_release(&gdt_lock);
}

initcall_smp_arch(arch_init_gdt_smp, INIT_EARLY);
