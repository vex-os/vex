#include <vex.arch/hart.h>
#include <vex.x86/gdt.h>
#include <vex.x86/idt.h>
#include <vex/initcall.h>
#include <vex/kmalloc.h>
#include <vex/panic.h>
#include <vex/spinlock.h>
#include <vex/string.h>

#define IDT_TRAP    (0x0F << 0)
#define IDT_INTR    (0x0E << 0)
#define IDT_RING_0  (0x00 << 5)
#define IDT_RING_3  (0x03 << 5)
#define IDT_PRESENT (0x01 << 7)

struct idt_entry {
    uint16_t offset_0;
    uint16_t selector;
    uint8_t ist_off;
    uint8_t flags;
    uint16_t offset_1;
    uint32_t offset_2;
    uint32_t reserved;
} __packed;

struct idt_register {
    uint16_t size;
    uint64_t offset;
} __packed;

static struct idt_entry idt[IDT_SIZE];
static struct idt_register idtr;
static spinlock_type idt_lock = SPINLOCK_INIT;

static void set_entry(struct idt_entry* restrict entry, bool trap, uint16_t selector, const void* restrict pfn)
{
    memset(entry, 0, sizeof(*entry));

    entry->offset_0 = ((uintptr_t)pfn & 0x000000000000FFFF);
    entry->offset_1 = ((uintptr_t)pfn & 0x00000000FFFF0000) >> 16;
    entry->offset_2 = ((uintptr_t)pfn & 0xFFFFFFFF00000000) >> 32;
    entry->selector = selector;
    entry->flags = IDT_PRESENT;

    if(trap) {
        entry->flags |= IDT_TRAP;
    }
    else {
        entry->flags |= IDT_INTR;
    }
}

void set_idt_entry_kern(unsigned vector, bool trap, const void* restrict pfn)
{
    if(vector < IDT_SIZE) {
        spinlock_acquire(&idt_lock);
        set_entry(&idt[vector], trap, gdt_selector(GDT_KERN_CODE_64, 0, 0), pfn);
        idt[vector].flags |= IDT_RING_0;
        spinlock_release(&idt_lock);
    }
}

void set_idt_entry_user(unsigned vector, bool trap, const void* restrict pfn)
{
    if(vector < IDT_SIZE) {
        spinlock_acquire(&idt_lock);
        set_entry(&idt[vector], trap, gdt_selector(GDT_KERN_CODE_64, 0, 0), pfn);
        idt[vector].flags |= IDT_RING_3;
        spinlock_release(&idt_lock);
    }
}

void unset_idt_entry(unsigned vector)
{
    if(vector < IDT_SIZE) {
        spinlock_acquire(&idt_lock);
        memset(&idt[vector], 0, sizeof(struct idt_entry));
        spinlock_release(&idt_lock);
    }
}

static void arch_init_idt_bsp(void)
{
    memset(idt, 0, sizeof(idt));
    memset(&idtr, 0, sizeof(idtr));

    idtr.size = (uint16_t)(sizeof(idt) - 1);
    idtr.offset = (uintptr_t)(&idt[0]);
}

initcall_bsp_arch(arch_init_idt_bsp, INIT_EARLY);

static void arch_init_idt_smp(void)
{
    asm volatile("lidtq %0" ::"m"(idtr));
}

initcall_smp_arch(arch_init_idt_smp, INIT_EARLY);
