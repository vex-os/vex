/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#include <kaneru/initcall.h>
#include <kaneru/interrupt.h>
#include <kaneru/kprintf.h>
#include <stdint.h>
#include <string.h>
#include <x86_64/gdt.h>
#include <x86_64/idt.h>

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

struct idt_pointer {
    uint16_t limit;
    uintptr_t base;
} __packed;

static struct idt_entry idt[X86_IDT_SIZE] = { 0 };
static struct idt_pointer idt_ptr = { 0 };
static intvec_t interrupt_map[X86_IDT_SIZE] = { 0 };

/* defined in sys/interrupts.S */
/* generated via gen.x86_64.interrupts.sh */
extern const uintptr_t __x86_interrupts[X86_IDT_SIZE];

/* called from sys/x86_64/idt.S */
void __used __x86_c_interrupt_handler(struct x86_interrupt_frame *restrict frame)
{
    frame->vector %= X86_IDT_SIZE;
    if(interrupt_map[frame->vector] < 0)
        return;
    trigger_interrupt(interrupt_map[frame->vector], frame);
}

void x86_enable_interrupts(void)
{
    asm volatile("sti");
}

void x86_disable_interrupts(void)
{
    asm volatile("cli");
}

bool x86_map_interrupt(intvec_t intvec, unsigned int vector, bool user)
{
    struct idt_entry *entry;

    if(intvec < 0 || intvec >= MAX_INTERRUPTS)
        return false;

    if(vector >= X86_IDT_SIZE)
        return false;

    if(interrupt_map[vector] != INTVEC_NULL)
        return false;

    entry = &idt[vector];
    entry->flags &= ~IDT_RING_0;
    entry->flags &= ~IDT_RING_3;
    entry->flags |= user ? IDT_RING_3 : IDT_RING_0;

    interrupt_map[vector] = intvec;

    return true;
}

static int init_x86_idt(void)
{
    unsigned int vector;
    struct idt_entry *entry;

    memset(idt, 0, sizeof(idt));

    for(vector = 0; vector < X86_IDT_SIZE; vector++) {
        entry = &idt[vector];

        entry->offset_0 = __x86_interrupts[vector] & 0xFFFF;
        entry->offset_1 = (__x86_interrupts[vector] >> 16) & 0xFFFF;
        entry->offset_2 = (__x86_interrupts[vector] >> 32) & 0xFFFFFFFF;
        entry->selector = GDT_SEL(GDT_KERN_CODE_64, 0, 0);
        entry->flags = IDT_PRESENT;
        if(vector == 0 || vector == 1 || (vector >= 3 && vector <= 31))
            entry->flags |= IDT_TRAP;
        else
            entry->flags |= IDT_INTR;
        entry->flags |= IDT_RING_0;
    }

    idt_ptr.limit = (uint16_t)(sizeof(idt) - 1);
    idt_ptr.base = (uintptr_t)(&idt[0]);

    asm volatile("lidtq %0"::"m"(idt_ptr));

    kprintf(KP_MACHINE, "idt: base=%p, limit=%hu", (void *)idt_ptr.base, idt_ptr.limit);

    return 0;
}
initcall_tier_0(x86_idt, init_x86_idt);
initcall_depend(x86_idt, interrupt);
initcall_depend(x86_idt, x86_gdt);
