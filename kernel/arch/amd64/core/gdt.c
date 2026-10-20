#include <arch/amd64.gdt.h>
#include <arch/amd64.idt.h>
#include <vex/errno.h>
#include <vex/hart.h>
#include <vex/initcall.h>
#include <vex/kmalloc.h>
#include <vex/kprintf.h>
#include <vex/kstack.h>
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

#define PREDEF_CODE_FLAGS (GDT_READWRITE | GDT_NONSYSTEM | GDT_EXECUTABLE)
#define PREDEF_DATA_FLAGS (GDT_READWRITE | GDT_NONSYSTEM)

struct gdt_register {
    uint16_t size;
    uint64_t offset;
} __packed;

// Carries the boot hart until the heap is up and its own GDT
// can be set up; exceptions before that still need IST stacks
static struct amd64_gdt boot_gdt;
static struct amd64_hart_tss boot_tss;
static uint8_t boot_ist_stacks[3][KSTACK_SIZE] __aligned(16);

static void write_entry_16(struct amd64_gdt_descriptor* restrict entry, uint32_t base, uint16_t limit, uint8_t flags)
{
    entry->limit_0 = limit;
    entry->base_0 = base & 0xFFFF;
    entry->base_1 = (base >> 16) & 0xFF;
    entry->base_2 = (base >> 24) & 0xFF;
    entry->flags_0 = flags | GDT_PRESENT;
}

static void write_entry_32(struct amd64_gdt_descriptor* restrict entry, uint32_t base, uint32_t limit, uint8_t flags)
{
    entry->limit_0 = limit & 0xFFFF;
    entry->limit_1 = (limit >> 16) & 0x0F;
    entry->base_0 = base & 0xFFFF;
    entry->base_1 = (base >> 16) & 0xFF;
    entry->base_2 = (base >> 24) & 0xFF;
    entry->flags_0 = flags | GDT_PRESENT;
    entry->flags_1 = GDT_4KIB_UNITS | GDT_32BIT;
}

static void write_entry_64(struct amd64_gdt_descriptor* restrict entry, uint8_t flags)
{
    entry->flags_0 = flags | GDT_PRESENT;
    entry->flags_1 = GDT_64BIT;
}

static void write_gdt(struct amd64_gdt* restrict gdt, const struct amd64_hart_tss* restrict tss)
{
    memset(gdt, 0, sizeof(struct amd64_gdt));

    write_entry_16(&gdt->kern_code_16, 0x00000000, 0xFFFF, PREDEF_CODE_FLAGS | GDT_RING_0);
    write_entry_16(&gdt->kern_data_16, 0x00000000, 0xFFFF, PREDEF_DATA_FLAGS | GDT_RING_0);
    write_entry_32(&gdt->kern_code_32, 0x00000000, 0xFFFFF, PREDEF_CODE_FLAGS | GDT_RING_0);
    write_entry_32(&gdt->kern_data_32, 0x00000000, 0xFFFFF, PREDEF_DATA_FLAGS | GDT_RING_0);
    write_entry_64(&gdt->kern_code_64, PREDEF_CODE_FLAGS | GDT_RING_0);
    write_entry_64(&gdt->kern_data_64, PREDEF_DATA_FLAGS | GDT_RING_0);
    write_entry_64(&gdt->user_data_64, PREDEF_DATA_FLAGS | GDT_RING_3);
    write_entry_64(&gdt->user_code_64, PREDEF_CODE_FLAGS | GDT_RING_3);

    uintptr_t tss_base = (uintptr_t)(tss);

    gdt->tss.limit_0 = sizeof(struct amd64_hart_tss) - 1;
    gdt->tss.base_0 = (tss_base & 0xFFFF);
    gdt->tss.base_1 = (tss_base >> 16) & 0xFF;
    gdt->tss.base_2 = (tss_base >> 24) & 0xFF;
    gdt->tss.base_3 = (tss_base >> 32) & 0xFFFFFFFF;
    gdt->tss.flags = 0x89; // Present, available 64-bit TSS
}

static void load_gdt(const struct amd64_gdt* restrict gdt)
{
    struct gdt_register gdtr;
    gdtr.size = sizeof(struct amd64_gdt) - 1;
    gdtr.offset = (uintptr_t)(gdt);

    asm volatile("lgdtq %[gdtr]\n\t"
                 "pushq %[code]\n\t"
                 "leaq 1f(%%rip), %%rax\n\t"
                 "pushq %%rax\n\t"
                 "lretq\n\t"
                 "1:\n\t"
                 "movw %w[data], %%ds\n\t"
                 "movw %w[data], %%es\n\t"
                 "movw %w[data], %%ss\n\t"
                 "ltr %w[tss]"
        :
        : [gdtr] "m"(gdtr), [code] "i"(gdt_selector(GDT_KERN_CODE_64, 0, 0)), [data] "r"(gdt_selector(GDT_KERN_DATA_64, 0, 0)),
        [tss] "r"(gdt_selector(GDT_TSS, 0, 0))
        : "rax", "memory");
}

static void arch_init_gdt_bsp(void)
{
    boot_tss.iopb = sizeof(struct amd64_hart_tss); // No I/O permission bitmap, ring 3 port access faults
    boot_tss.ist_1 = (uintptr_t)(&boot_ist_stacks[0][KSTACK_SIZE]); // IDT_IST_DOUBLE_FAULT
    boot_tss.ist_2 = (uintptr_t)(&boot_ist_stacks[1][KSTACK_SIZE]); // IDT_IST_NMI
    boot_tss.ist_3 = (uintptr_t)(&boot_ist_stacks[2][KSTACK_SIZE]); // IDT_IST_MACHINE_CHECK

    write_gdt(&boot_gdt, &boot_tss);
    load_gdt(&boot_gdt);
}

initcall_bsp_arch(arch_init_gdt_bsp, INIT_EARLY);

static uint64_t alloc_ist_stack(void)
{
    void* top = kstack_alloc();

    if(top == nullptr) {
        panic("gdt: out of memory");
    }

    return (uintptr_t)(top);
}

static void arch_init_gdt_smp(void)
{
    struct hart_local* hart = this_hart();

    memset(&hart->arch.tss, 0, sizeof(struct amd64_hart_tss));
    hart->arch.tss.iopb = sizeof(struct amd64_hart_tss); // No I/O permission bitmap, ring 3 port access faults

    hart->arch.tss.ist_1 = alloc_ist_stack(); // IDT_IST_DOUBLE_FAULT
    hart->arch.tss.ist_2 = alloc_ist_stack(); // IDT_IST_NMI
    hart->arch.tss.ist_3 = alloc_ist_stack(); // IDT_IST_MACHINE_CHECK

    write_gdt(&hart->arch.gdt, &hart->arch.tss);
    load_gdt(&hart->arch.gdt);
}

initcall_smp_arch(arch_init_gdt_smp, INIT_EARLY);
