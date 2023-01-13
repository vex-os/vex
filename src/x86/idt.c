/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <kan/errno.h>
#include <kan/initcall.h>
#include <kan/interrupt.h>
#include <kan/kprintf.h>
#include <kan/symbol.h>
#include <stdint.h>
#include <string.h>
#include <x86/gdt.h>
#include <x86/idt.h>

#define IDT_TRAP    (0x0F << 0)
#define IDT_INTR    (0x0E << 0)
#define IDT_RING_0  (0x00 << 5)
#define IDT_RING_3  (0x03 << 5)
#define IDT_PRESENT (0x01 << 7)

typedef struct idt_entry_s {
    uint16_t offset_0;
    uint16_t selector;
    uint8_t ist_off;
    uint8_t flags;
    uint16_t offset_1;
    uint32_t offset_2;
    uint32_t reserved;
} __packed idt_entry_t;

typedef struct idt_register_s {
    uint16_t size;
    uintptr_t offset;
} __packed idt_register_t;

static idt_entry_t idt[X86_MAX_INTERRUPTS] = { 0 };
static idt_register_t idtr = { 0 };
static long intmap[X86_MAX_INTERRUPTS] = { 0 };

/* generated as build/x86_interrupts.c */
extern const uintptr_t x86_interrupts[X86_MAX_INTERRUPTS];

/* called from assembly (idt.S) */
void __used x86_interrupt_handler(interrupt_frame_t *restrict frame)
{
    frame->vector %= X86_MAX_INTERRUPTS;
    if(intmap[frame->vector] < 0)
        return;
    trigger_interrupt(intmap[frame->vector], frame);
}

int x86_map_interrupt(long vector, unsigned short x86_vector, bool user)
{
    idt_entry_t *entry;

    if(vector < 0 || vector >= MAX_INTERRUPTS) {
        pr_warning("x86_idt: interrupt vector <%ld> out of range", vector);
        return ERANGE;
    }

    if(x86_vector >= X86_MAX_INTERRUPTS) {
        pr_warning("x86_idt: x86 vector <%hu> out of range", x86_vector);
        return ERANGE;
    }

    if(intmap[x86_vector] != NULL_INTERRUPT) {
        pr_warning("x86_idt: interrupt vector <%ld> is already mapped", vector);
        return EBUSY;
    }

    entry = &idt[x86_vector];
    entry->flags &= ~IDT_RING_0;
    entry->flags &= ~IDT_RING_3;
    entry->flags |= user ? IDT_RING_3 : IDT_RING_0;

    intmap[x86_vector] = vector;

    return EOK;
}
EXPORT_SYMBOL(x86_map_interrupt);

static int init_x86_idt(void)
{
    unsigned int vector;
    idt_entry_t *entry;

    memset(idt, 0, sizeof(idt));

    for(vector = 0; vector < X86_MAX_INTERRUPTS; vector++) {
        entry = &idt[vector];

        entry->offset_0 = x86_interrupts[vector] & 0xFFFF;
        entry->offset_1 = (x86_interrupts[vector] >> 16) & 0xFFFF;
        entry->offset_2 = (x86_interrupts[vector] >> 32) & 0xFFFFFFFF;
        entry->selector = GDT_SEL(GDT_KERN_CODE_64, 0, 0);
        entry->flags = IDT_PRESENT;
        if(vector == 0 || vector == 1 || (vector >= 3 && vector <= 31))
            entry->flags |= IDT_TRAP;
        else
            entry->flags |= IDT_INTR;
        entry->flags |= IDT_RING_0;
    }

    idtr.size = (uint16_t)(sizeof(idt) - 1);
    idtr.offset = (uintptr_t)(&idt[0]);

    asm volatile("lidtq %0"::"m"(idtr));

    pr_inform("x86_idt: idtr.size=%zu", (size_t)(idtr.size));
    pr_inform("x86_idt: idtr.offset=%p", (void *)(idtr.offset));

    return EOK;
}
initcall_tier_0(x86_idt, init_x86_idt);
initcall_depend(x86_idt, x86_gdt);
