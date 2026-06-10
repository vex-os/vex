#include <vex.arch/hart.h>
#include <vex.x86/gdt.h>
#include <vex.x86/idt.h>
#include <vex/initcall.h>
#include <vex/malloc.h>
#include <vex/panic.h>
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

struct x86_idt {
    struct idt_entry entries[IDT_SIZE];
    struct {
        uint16_t size;
        uintptr_t offset;
    } __packed idtr;
};

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
        struct hart_local* hart = this_hart();
        struct x86_idt* idt = hart->x86_idtp;
        set_entry(&idt->entries[vector], trap, gdt_selector(GDT_KERN_CODE_64, 0, 0), pfn);
        idt->entries[vector].flags |= IDT_RING_0;
    }
}

void set_idt_entry_user(unsigned vector, bool trap, const void* restrict pfn)
{
    if(vector < IDT_SIZE) {
        struct hart_local* hart = this_hart();
        struct x86_idt* idt = hart->x86_idtp;
        set_entry(&idt->entries[vector], trap, gdt_selector(GDT_KERN_CODE_64, 0, 0), pfn);
        idt->entries[vector].flags |= IDT_RING_3;
    }
}

void unset_idt_entry(unsigned vector)
{
    if(vector < IDT_SIZE) {
        struct hart_local* hart = this_hart();
        struct x86_idt* idt = hart->x86_idtp;
        memset(&idt->entries[vector], 0, sizeof(struct idt_entry));
    }
}

static void arch_init_idt(void)
{
    struct x86_idt* idt = kmalloc(sizeof(struct x86_idt));

    if(idt == nullptr) {
        panic("out of memory");
    }

    struct hart_local* hart = this_hart();
    hart->x86_idtp = idt;

    idt->idtr.size = (uint16_t)(sizeof(idt->entries) - 1);
    idt->idtr.offset = (uintptr_t)(&idt->entries[0]);

    asm volatile("lidtq %0" ::"m"(idt->idtr));
}

initcall_smp_arch(arch_init_idt, INIT_EARLY);
