/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <kan/errno.h>
#include <kan/initcall.h>
#include <kan/interrupt.h>
#include <kan/kprintf.h>
#include <kan/symbol.h>
#include <stdint.h>
#include <string.h>
#include <x86/interrupt.h>
#include <x86/segment.h>

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

/* Defined in temp/x86_isrs.S */
extern const uintptr_t __x86_isrs[X86_MAX_INTERRUPTS];

/* Called from assembly (src/x86/interrupt.S) */
void __used __x86_common_isr(interrupt_frame_t *restrict frame)
{
    frame->vector %= X86_MAX_INTERRUPTS;
    trigger_interrupt(frame->vector, frame);
}

static int init_x86_interrupt(void)
{
    size_t i;
    idt_entry_t *entry;

    for(i = 0; i < MAX_INTERRUPTS; i++) {
        entry = &idt[i];

        memset(entry, 0, sizeof(idt_entry_t));

        entry->offset_0 = (__x86_isrs[i] & 0x000000000000FFFF);
        entry->offset_1 = (__x86_isrs[i] & 0x00000000FFFF0000) >> 16;
        entry->offset_2 = (__x86_isrs[i] & 0xFFFFFFFF00000000) >> 32;
        entry->selector = SEGMENT_SELECTOR(SEGMENT_KERN_CODE_64, 0, 0);

        /* Interrupt vectors 0x00 to 0x1F are x86 exceptions */
        entry->flags |= ((i < 0x20) ? IDT_TRAP : IDT_INTR);

        entry->flags |= IDT_RING_0;
        entry->flags |= IDT_PRESENT;
    }

    idtr.size = (uint16_t)(sizeof(idt) - 1);
    idtr.offset = (uintptr_t)(&idt[0]);

    asm volatile("lidtq %0"::"m"(idtr));

    pr_inform("x86_interrupt: idtr.size=%zu", (size_t)(idtr.size));
    pr_inform("x86_interrupt: idtr.offset=%p", (void *)(idtr.offset));

    return EOK;
}
initcall_tier_0(x86_interrupt, init_x86_interrupt);
initcall_depend(x86_interrupt, x86_segment);
