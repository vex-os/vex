/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#include <stdint.h>
#include <string.h>
#include <sys/interrupt.h>
#include <x86_64/gdt.h>
#include <x86_64/idt.h>

#define IDT_TRAP    (0x0F << 0)
#define IDT_INTR    (0x0E << 0)
#define IDT_RING_0  (0x00 << 5)
#define IDT_RING_3  (0x03 << 5)
#define IDT_PRESENT (0x01 << 7)

typedef struct idt_entry_s {
    uint16_t i_offset_0;
    uint16_t i_selector;
    uint8_t i_ist_off;
    uint8_t i_flags;
    uint16_t i_offset_1;
    uint32_t i_offset_2;
    uint32_t i_reserved;
} __packed idt_entry_t;

typedef struct idt_register_s {
    uint16_t i_size;
    uintptr_t i_offset;
} __packed idt_register_t;

static idt_entry_t idt[X86_IDT_SIZE] = { 0 };
static idt_register_t idtr = { 0 };

// Generated $(TEMP_DIR)/isr_stubs.S
extern const uint64_t isr_stubs[X86_IDT_SIZE];

void __used isr_handler(cpu_ctx_t *restrict ctx, uint64_t vector)
{
    trigger_interrupt((long)(vector % X86_IDT_SIZE), ctx);
}

static void init_idt(void)
{
    size_t i;
    idt_entry_t *entry;

    for(i = 0; i < MAX_INTERRUPTS; i++) {
        entry = &idt[i];

        memset(entry, 0, sizeof(idt_entry_t));

        entry->i_offset_0 = (isr_stubs[i] & 0x000000000000FFFF);
        entry->i_offset_1 = (isr_stubs[i] & 0x00000000FFFF0000) >> 16;
        entry->i_offset_2 = (isr_stubs[i] & 0xFFFFFFFF00000000) >> 32;
        entry->i_selector = GDT_SELECTOR(GDT_KERN_CODE_64, 0, 0);

        // Interrupt vectors 0x00 to 0x1F are X86 exceptions
        entry->i_flags |= ((i < 0x20) ? IDT_TRAP : IDT_INTR);

        entry->i_flags |= IDT_RING_0;
        entry->i_flags |= IDT_PRESENT;
    }

    idtr.i_size = (uint16_t)(sizeof(idt) - 1);
    idtr.i_offset = (uintptr_t)(&idt[0]);

    asm volatile("lidtq %0"::"m"(idtr));
}
early_initcall(idt, init_idt);
initcall_dependency(idt, gdt);
