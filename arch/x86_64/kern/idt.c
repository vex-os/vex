/* SPDX-License-Identifier: Zlib */
#include <arch/gdt.h>
#include <arch/idt.h>
#include <iserix/panic.h>
#include <iserix/printf.h>
#include <iserix/vmm.h>
#include <stddef.h>
#include <string.h>

#define IDT_SIZE 256

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
} PACKED;

struct idt_register {
    uint16_t size;
    uintptr_t offset;
} PACKED;

static struct idt_entry idt[IDT_SIZE] = { 0 };
static struct idt_register idtr = { 0 };

/* $(TEMP_DIR)/x86_64.isr_stubs.S */
extern const uint64_t isr_stubs[IDT_SIZE];

void USED isr_handler(struct x86_frame *restrict frame, uint64_t vector)
{
    panic("idt: isr_handler(%p, %02jX)", (void *)(frame), (uintmax_t)(vector));
    UNREACHABLE();
}

static void init_idt(void)
{
    size_t i;
    struct idt_entry *entry;

    for(i = 0; i < IDT_SIZE; i++) {
        entry = &idt[i];

        memset(entry, 0, sizeof(struct idt_entry));

        entry->offset_0 = (isr_stubs[i] & 0x000000000000FFFF);
        entry->offset_1 = (isr_stubs[i] & 0x00000000FFFF0000) >> 16;
        entry->offset_2 = (isr_stubs[i] & 0xFFFFFFFF00000000) >> 32;
        entry->selector = GDT_SELECTOR(GDT_KERN_CODE_64, 0, 0);

        /* Interrupt vectors 0x00 to 0x1F are x86 exceptions */
        entry->flags |= ((i < 0x20) ? IDT_TRAP : IDT_INTR);

        entry->flags |= IDT_RING_0;
        entry->flags |= IDT_PRESENT;
    }

    idtr.size = (uint16_t)(sizeof(idt) - 1);
    idtr.offset = (uintptr_t)(&idt[0]);

    asm volatile("lidtq %0"::"m"(idtr));

    kprintf(KP_DEBUG, "idt: idtr.size=%zu", (size_t)(idtr.size));
    kprintf(KP_DEBUG, "idt: idtr.offset=%p", (void *)(idtr.offset));
}
CORE_INITCALL(x86_idt, init_idt);
INITCALL_DEPENDENCY(x86_idt, x86_gdt);
INITCALL_DEPENDENCY(x86_idt, vmm);
