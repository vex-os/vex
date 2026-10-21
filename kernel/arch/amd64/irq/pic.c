#include <arch/amd64.idt.h>
#include <arch/io.h>
#include <vex/hart.h>
#include <vex/initcall.h>
#include <vex/kprintf.h>

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

#define PIC1_COMMAND 0x0020
#define PIC1_DATA    0x0021
#define PIC2_COMMAND 0x00A0
#define PIC2_DATA    0x00A1

void __used amd64_spur_handler(hart_context_t* restrict ctx, uint64_t intvec)
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

    port_write8(PIC1_COMMAND, 0x11);
    port_write8(PIC2_COMMAND, 0x11);

    port_write8(PIC1_DATA, 0x20);
    port_write8(PIC2_DATA, 0x28);

    port_write8(PIC1_DATA, 0x04);
    port_write8(PIC2_DATA, 0x02);

    port_write8(PIC1_DATA, 0x01);
    port_write8(PIC2_DATA, 0x01);

    port_write8(PIC1_DATA, 0xFF);
    port_write8(PIC2_DATA, 0xFF);

    // <<  Because it's a shit chip >>
    //                  - streaksu
    for(unsigned vector = 0x00; vector <= 0x10; vector += 1U) {
        if(vector >= 0x08)
            port_write8(PIC2_COMMAND, 0x20);
        port_write8(PIC1_COMMAND, 0x20);
    }
}

initcall_bsp_arch(arch_init_pic_bsp, INIT_MIDDLE);

static void arch_init_pic_smp(void)
{
    idt_set_kernel_gate(0x20, false, &amd64_spur_20);
    idt_set_kernel_gate(0x21, false, &amd64_spur_21);
    idt_set_kernel_gate(0x22, false, &amd64_spur_22);
    idt_set_kernel_gate(0x23, false, &amd64_spur_23);
    idt_set_kernel_gate(0x24, false, &amd64_spur_24);
    idt_set_kernel_gate(0x25, false, &amd64_spur_25);
    idt_set_kernel_gate(0x26, false, &amd64_spur_26);
    idt_set_kernel_gate(0x27, false, &amd64_spur_27);
    idt_set_kernel_gate(0x28, false, &amd64_spur_28);
    idt_set_kernel_gate(0x29, false, &amd64_spur_29);
    idt_set_kernel_gate(0x2A, false, &amd64_spur_2A);
    idt_set_kernel_gate(0x2B, false, &amd64_spur_2B);
    idt_set_kernel_gate(0x2C, false, &amd64_spur_2C);
    idt_set_kernel_gate(0x2D, false, &amd64_spur_2D);
    idt_set_kernel_gate(0x2E, false, &amd64_spur_2E);
    idt_set_kernel_gate(0x2F, false, &amd64_spur_2F);
}

initcall_smp_irq(arch_init_pic_smp, INIT_EARLY);
