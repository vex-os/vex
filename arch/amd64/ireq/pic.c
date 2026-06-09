#include <vex.amd64/idt.h>
#include <vex.amd64/pmio.h>
#include <vex.arch/intr.h>
#include <vex/initcall.h>
#include <vex/kprintf.h>

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

void __used amd64_spur_handler(struct regfile* restrict regs, uint64_t intvec)
{
    // <<  Because it's a shit chip >>
    //                  - streaksu
    pr_verbose("pic: spurious interrupt: intvec=0x%02zX", (size_t)(intvec));
}

static void arch_init_pic_bsp(void)
{
    disable_interrupts();

    // Generally speaking, masking everything and
    // sending sixteen end-of-interrupt messages should
    // be enough to properly disable both legacy PICs, though
    // in cases BIOS is written by crackheads, PIC may still send an interrupt
    // so we have to remap both controllers to 0x20..0x2F and never touch
    // these vectors again, the system might probably reboot but whatever

    pmwrite8(PIC1_COMMAND, 0x11);
    pmwrite8(PIC2_COMMAND, 0x11);

    pmwrite8(PIC1_DATA, 0x20);
    pmwrite8(PIC2_DATA, 0x28);

    pmwrite8(PIC1_DATA, 0x04);
    pmwrite8(PIC2_DATA, 0x02);

    pmwrite8(PIC1_DATA, 0x01);
    pmwrite8(PIC2_DATA, 0x01);

    pmwrite8(PIC1_DATA, 0xFF);
    pmwrite8(PIC2_DATA, 0xFF);

    // <<  Because it's a shit chip >>
    //                  - streaksu
    for(unsigned vector = 0x00; vector <= 0x10; vector += 1U) {
        if(vector >= 0x08)
            pmwrite8(PIC2_COMMAND, 0x20);
        pmwrite8(PIC1_COMMAND, 0x20);
    }
}

initcall_bsp_arch(arch_init_pic_bsp, INIT_MIDDLE);

static void arch_init_pic_smp(void)
{
    set_idt_entry_kern(0x20, false, &amd64_spur_20);
    set_idt_entry_kern(0x21, false, &amd64_spur_21);
    set_idt_entry_kern(0x22, false, &amd64_spur_22);
    set_idt_entry_kern(0x23, false, &amd64_spur_23);
    set_idt_entry_kern(0x24, false, &amd64_spur_24);
    set_idt_entry_kern(0x25, false, &amd64_spur_25);
    set_idt_entry_kern(0x26, false, &amd64_spur_26);
    set_idt_entry_kern(0x27, false, &amd64_spur_27);
    set_idt_entry_kern(0x28, false, &amd64_spur_28);
    set_idt_entry_kern(0x29, false, &amd64_spur_29);
    set_idt_entry_kern(0x2A, false, &amd64_spur_2A);
    set_idt_entry_kern(0x2B, false, &amd64_spur_2B);
    set_idt_entry_kern(0x2C, false, &amd64_spur_2C);
    set_idt_entry_kern(0x2D, false, &amd64_spur_2D);
    set_idt_entry_kern(0x2E, false, &amd64_spur_2E);
    set_idt_entry_kern(0x2F, false, &amd64_spur_2F);
}

initcall_smp_ireq(arch_init_pic_smp, INIT_EARLY);
