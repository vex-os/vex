#include <core/amd64.idt.h>
#include <core/amd64.pmio.h>
#include <core/arch.interrupts.h>
#include <core/kprintf.h>
#include <init/initfn.h>

#define PIC1_COMMAND 0x0020
#define PIC1_DATA    0x0021
#define PIC2_COMMAND 0x00A0
#define PIC2_DATA    0x00A1

extern void amd64_spur_20(void);
extern void amd64_spur_21(void);
extern void amd64_spur_22(void);
extern void amd64_spur_23(void);
extern void amd64_spur_24(void);
extern void amd64_spur_25(void);
extern void amd64_spur_26(void);
extern void amd64_spur_27(void);
extern void amd64_spur_28(void);
extern void amd64_spur_29(void);
extern void amd64_spur_2A(void);
extern void amd64_spur_2B(void);
extern void amd64_spur_2C(void);
extern void amd64_spur_2D(void);
extern void amd64_spur_2E(void);
extern void amd64_spur_2F(void);

void __used amd64_spur_handler(struct interrupt_frame* restrict frame, uint64_t intvec)
{
    kprintf(KP_WARNING, "picstub: spurious interrupt 0x%02zX", (size_t)intvec);
}

static int init_amd64_picstub(void)
{
    unsigned int vector;

    disable_interrupts();

    // Generally speaking, masking everything and
    // sending sixteen end-of-interrupt messages should
    // be enough to properly disable both legacy PICs, though
    // in cases BIOS is written by crackheads, PIC may still send an interrupt
    // so we have to remap both controllers to 0x20..0x2F and never touch
    // these vectors again, the system might probably reboot but whatever

    // ICW1: edge-triggered mode, cascade mode, require ICW4
    amd64_pmwrite8(PIC1_COMMAND, 0x11);
    amd64_pmwrite8(PIC2_COMMAND, 0x11);

    // ICW2: remap to 0x20..0x2F
    amd64_pmwrite8(PIC1_DATA, 0x20);
    amd64_pmwrite8(PIC2_DATA, 0x28);

    // ICW3: cascade mode, PIC2 at IRQ2
    amd64_pmwrite8(PIC1_DATA, 0x04);
    amd64_pmwrite8(PIC2_DATA, 0x02);

    // ICW4: 8086 mode
    amd64_pmwrite8(PIC1_DATA, 0x01);
    amd64_pmwrite8(PIC2_DATA, 0x01);

    // Mask all IRQs
    amd64_pmwrite8(PIC1_DATA, 0xFF);
    amd64_pmwrite8(PIC2_DATA, 0xFF);

    // Send sixteen EOI messages
    for(vector = 0x00; vector <= 0x10; vector += 1U) {
        if(vector >= 0x08)
            amd64_pmwrite8(PIC2_COMMAND, 0x20);
        amd64_pmwrite8(PIC1_COMMAND, 0x20);
    }

    amd64_set_idt_entry_kern(0x20, 0, &amd64_spur_20);
    amd64_set_idt_entry_kern(0x21, 0, &amd64_spur_21);
    amd64_set_idt_entry_kern(0x22, 0, &amd64_spur_22);
    amd64_set_idt_entry_kern(0x23, 0, &amd64_spur_23);
    amd64_set_idt_entry_kern(0x24, 0, &amd64_spur_24);
    amd64_set_idt_entry_kern(0x25, 0, &amd64_spur_25);
    amd64_set_idt_entry_kern(0x26, 0, &amd64_spur_26);
    amd64_set_idt_entry_kern(0x27, 0, &amd64_spur_27);
    amd64_set_idt_entry_kern(0x28, 0, &amd64_spur_28);
    amd64_set_idt_entry_kern(0x29, 0, &amd64_spur_29);
    amd64_set_idt_entry_kern(0x2A, 0, &amd64_spur_2A);
    amd64_set_idt_entry_kern(0x2B, 0, &amd64_spur_2B);
    amd64_set_idt_entry_kern(0x2C, 0, &amd64_spur_2C);
    amd64_set_idt_entry_kern(0x2D, 0, &amd64_spur_2D);
    amd64_set_idt_entry_kern(0x2E, 0, &amd64_spur_2E);
    amd64_set_idt_entry_kern(0x2F, 0, &amd64_spur_2F);

    return 0;
}

DEFINE_INITFN(init_amd64_picstub, INITFN_CAT_ARCH, INITFN_ORDER_MIDDLE);
