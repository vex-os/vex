// SPDX-License-Identifier: BSD-3-Clause

#include <amd64/gdt.h>
#include <amd64/idt.h>
#include <arch/interrupts.h>
#include <core/assert.h>
#include <core/kprintf.h>
#include <core/panic.h>
#include <init/initfn.h>
#include <lib/string.h>

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
    uintptr_t offset;
} __packed;

static struct idt_entry idt[IDT_SIZE];
static struct idt_register idtr;

extern void amd64_isr_00(void);
extern void amd64_isr_01(void);
extern void amd64_isr_02(void);
extern void amd64_isr_03(void);
extern void amd64_isr_04(void);
extern void amd64_isr_05(void);
extern void amd64_isr_06(void);
extern void amd64_isr_07(void);
extern void amd64_isr_08(void);
extern void amd64_isr_09(void);
extern void amd64_isr_0A(void);
extern void amd64_isr_0B(void);
extern void amd64_isr_0C(void);
extern void amd64_isr_0D(void);
extern void amd64_isr_0E(void);
extern void amd64_isr_0F(void);
extern void amd64_isr_10(void);
extern void amd64_isr_11(void);
extern void amd64_isr_12(void);
extern void amd64_isr_13(void);
extern void amd64_isr_14(void);
extern void amd64_isr_15(void);
extern void amd64_isr_16(void);
extern void amd64_isr_17(void);
extern void amd64_isr_18(void);
extern void amd64_isr_19(void);
extern void amd64_isr_1A(void);
extern void amd64_isr_1B(void);
extern void amd64_isr_1C(void);
extern void amd64_isr_1D(void);
extern void amd64_isr_1E(void);
extern void amd64_isr_1F(void);

void __used amd64_isr_handler(struct interrupt_frame* restrict frame, uint64_t intvec)
{
    disable_interrupts();
    panic("idt: isr_handler %02zX", (size_t)intvec);
    unreachable();
}

void set_idt_entry_kern(unsigned int vector, bool trap, const void* restrict pfn)
{
    struct idt_entry* entry;

    kassert(pfn && "invalid IDT handler function pointer");

    if(vector < IDT_SIZE) {
        entry = &idt[vector];

        memset(entry, 0, sizeof(struct idt_entry));

        entry->offset_0 = ((uintptr_t)pfn & 0x000000000000FFFF);
        entry->offset_1 = ((uintptr_t)pfn & 0x00000000FFFF0000) >> 16;
        entry->offset_2 = ((uintptr_t)pfn & 0xFFFFFFFF00000000) >> 32;
        entry->selector = gdt_selector(GDT_KERN_CODE_64, 0, 0);

        entry->flags = trap ? IDT_TRAP : IDT_INTR;

        entry->flags |= IDT_RING_0;
        entry->flags |= IDT_PRESENT;
    }
}

void set_idt_entry_user(unsigned int vector, bool trap, const void* restrict pfn)
{
    struct idt_entry* entry;

    kassert(pfn && "invalid IDT handler function pointer");

    if(vector < IDT_SIZE) {
        entry = &idt[vector];

        memset(entry, 0, sizeof(struct idt_entry));

        entry->offset_0 = ((uintptr_t)pfn & 0x000000000000FFFF);
        entry->offset_1 = ((uintptr_t)pfn & 0x00000000FFFF0000) >> 16;
        entry->offset_2 = ((uintptr_t)pfn & 0xFFFFFFFF00000000) >> 32;
        entry->selector = gdt_selector(GDT_KERN_CODE_64, 3, 0);

        entry->flags = trap ? IDT_TRAP : IDT_INTR;

        entry->flags |= IDT_RING_3;
        entry->flags |= IDT_PRESENT;
    }
}

void unset_idt_entry(unsigned int vector)
{
    if(vector >= IDT_SIZE)
        return;
    memset(&idt[vector], 0, sizeof(struct idt_entry));
}

static int AMD64_init_idt(void)
{
    memset(idt, 0, sizeof(idt));

    set_idt_entry_kern(0x00, true, &amd64_isr_00);
    set_idt_entry_kern(0x01, true, &amd64_isr_01);
    set_idt_entry_kern(0x02, true, &amd64_isr_02);
    set_idt_entry_kern(0x03, true, &amd64_isr_03);
    set_idt_entry_kern(0x04, true, &amd64_isr_04);
    set_idt_entry_kern(0x05, true, &amd64_isr_05);
    set_idt_entry_kern(0x06, true, &amd64_isr_06);
    set_idt_entry_kern(0x07, true, &amd64_isr_07);
    set_idt_entry_kern(0x08, true, &amd64_isr_08);
    set_idt_entry_kern(0x09, true, &amd64_isr_09);
    set_idt_entry_kern(0x0A, true, &amd64_isr_0A);
    set_idt_entry_kern(0x0B, true, &amd64_isr_0B);
    set_idt_entry_kern(0x0C, true, &amd64_isr_0C);
    set_idt_entry_kern(0x0D, true, &amd64_isr_0D);
    set_idt_entry_kern(0x0E, true, &amd64_isr_0E);
    set_idt_entry_kern(0x0F, true, &amd64_isr_0F);
    set_idt_entry_kern(0x10, true, &amd64_isr_10);
    set_idt_entry_kern(0x11, true, &amd64_isr_11);
    set_idt_entry_kern(0x12, true, &amd64_isr_12);
    set_idt_entry_kern(0x13, true, &amd64_isr_13);
    set_idt_entry_kern(0x14, true, &amd64_isr_14);
    set_idt_entry_kern(0x15, true, &amd64_isr_15);
    set_idt_entry_kern(0x16, true, &amd64_isr_16);
    set_idt_entry_kern(0x17, true, &amd64_isr_17);
    set_idt_entry_kern(0x18, true, &amd64_isr_18);
    set_idt_entry_kern(0x19, true, &amd64_isr_19);
    set_idt_entry_kern(0x1A, true, &amd64_isr_1A);
    set_idt_entry_kern(0x1B, true, &amd64_isr_1B);
    set_idt_entry_kern(0x1C, true, &amd64_isr_1C);
    set_idt_entry_kern(0x1D, true, &amd64_isr_1D);
    set_idt_entry_kern(0x1E, true, &amd64_isr_1E);
    set_idt_entry_kern(0x1F, true, &amd64_isr_1F);

    idtr.size = (uint16_t)(sizeof(idt) - 1);
    idtr.offset = (uintptr_t)(&idt[0]);

    asm volatile("lidtq %0" ::"m"(idtr));

    kprintf(KP_INFORM, "idt: idtr.size=%zu idtr.offset=%p", (size_t)(idtr.size), (const void*)(idtr.offset));

    return 0;
}

DEFINE_INITFN(AMD64_init_idt, INITFN_CAT_ARCH, INITFN_ORDER_EARLY);
