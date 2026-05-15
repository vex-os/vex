#include <core/amd64.gdt.h>
#include <core/amd64.idt.h>
#include <core/arch.interrupts.h>
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

static struct idt_entry idt[X86_IDT_SIZE];
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

void amd64_set_idt_entry_kern(unsigned int vector, int is_trap, const void* restrict pfn)
{
    struct idt_entry* entry;

    if(vector < X86_IDT_SIZE) {
        entry = &idt[vector];

        memset(entry, 0, sizeof(struct idt_entry));

        entry->offset_0 = ((uintptr_t)pfn & 0x000000000000FFFF);
        entry->offset_1 = ((uintptr_t)pfn & 0x00000000FFFF0000) >> 16;
        entry->offset_2 = ((uintptr_t)pfn & 0xFFFFFFFF00000000) >> 32;
        entry->selector = amd64_gdt_selector(X86_GDT_KERN_CODE_64, 0, 0);

        entry->flags = is_trap ? IDT_TRAP : IDT_INTR;

        entry->flags |= IDT_RING_0;
        entry->flags |= IDT_PRESENT;
    }
}

void amd64_set_idt_entry_user(unsigned int vector, int is_trap, const void* restrict pfn)
{
    struct idt_entry* entry;

    if(vector < X86_IDT_SIZE) {
        entry = &idt[vector];

        memset(entry, 0, sizeof(struct idt_entry));

        entry->offset_0 = ((uintptr_t)pfn & 0x000000000000FFFF);
        entry->offset_1 = ((uintptr_t)pfn & 0x00000000FFFF0000) >> 16;
        entry->offset_2 = ((uintptr_t)pfn & 0xFFFFFFFF00000000) >> 32;
        entry->selector = amd64_gdt_selector(X86_GDT_KERN_CODE_64, 3, 0);

        entry->flags = is_trap ? IDT_TRAP : IDT_INTR;

        entry->flags |= IDT_RING_3;
        entry->flags |= IDT_PRESENT;
    }
}

void amd64_unset_idt_entry(unsigned int vector)
{
    if(vector >= X86_IDT_SIZE)
        return;
    memset(&idt[vector], 0, sizeof(struct idt_entry));
}

static int init_amd64_idt(void)
{
    memset(idt, 0, sizeof(idt));

    amd64_set_idt_entry_kern(0x00, 1, &amd64_isr_00);
    amd64_set_idt_entry_kern(0x01, 1, &amd64_isr_01);
    amd64_set_idt_entry_kern(0x02, 1, &amd64_isr_02);
    amd64_set_idt_entry_kern(0x03, 1, &amd64_isr_03);
    amd64_set_idt_entry_kern(0x04, 1, &amd64_isr_04);
    amd64_set_idt_entry_kern(0x05, 1, &amd64_isr_05);
    amd64_set_idt_entry_kern(0x06, 1, &amd64_isr_06);
    amd64_set_idt_entry_kern(0x07, 1, &amd64_isr_07);
    amd64_set_idt_entry_kern(0x08, 1, &amd64_isr_08);
    amd64_set_idt_entry_kern(0x09, 1, &amd64_isr_09);
    amd64_set_idt_entry_kern(0x0A, 1, &amd64_isr_0A);
    amd64_set_idt_entry_kern(0x0B, 1, &amd64_isr_0B);
    amd64_set_idt_entry_kern(0x0C, 1, &amd64_isr_0C);
    amd64_set_idt_entry_kern(0x0D, 1, &amd64_isr_0D);
    amd64_set_idt_entry_kern(0x0E, 1, &amd64_isr_0E);
    amd64_set_idt_entry_kern(0x0F, 1, &amd64_isr_0F);
    amd64_set_idt_entry_kern(0x10, 1, &amd64_isr_10);
    amd64_set_idt_entry_kern(0x11, 1, &amd64_isr_11);
    amd64_set_idt_entry_kern(0x12, 1, &amd64_isr_12);
    amd64_set_idt_entry_kern(0x13, 1, &amd64_isr_13);
    amd64_set_idt_entry_kern(0x14, 1, &amd64_isr_14);
    amd64_set_idt_entry_kern(0x15, 1, &amd64_isr_15);
    amd64_set_idt_entry_kern(0x16, 1, &amd64_isr_16);
    amd64_set_idt_entry_kern(0x17, 1, &amd64_isr_17);
    amd64_set_idt_entry_kern(0x18, 1, &amd64_isr_18);
    amd64_set_idt_entry_kern(0x19, 1, &amd64_isr_19);
    amd64_set_idt_entry_kern(0x1A, 1, &amd64_isr_1A);
    amd64_set_idt_entry_kern(0x1B, 1, &amd64_isr_1B);
    amd64_set_idt_entry_kern(0x1C, 1, &amd64_isr_1C);
    amd64_set_idt_entry_kern(0x1D, 1, &amd64_isr_1D);
    amd64_set_idt_entry_kern(0x1E, 1, &amd64_isr_1E);
    amd64_set_idt_entry_kern(0x1F, 1, &amd64_isr_1F);

    idtr.size = (uint16_t)(sizeof(idt) - 1);
    idtr.offset = (uintptr_t)(&idt[0]);

    asm volatile("lidtq %0" ::"m"(idtr));

    kprintf(KP_INFORM, "idt: idtr.size=%zu idtr.offset=%p", (size_t)(idtr.size), (const void*)(idtr.offset));

    return 0;
}

DEFINE_INITFN(init_amd64_idt, INITFN_CAT_ARCH, INITFN_ORDER_EARLY);
