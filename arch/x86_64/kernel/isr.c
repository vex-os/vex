#include <vex.x86/idt.h>
#include <vex/compiler.h>
#include <vex/hart.h>
#include <vex/initcall.h>
#include <vex/kprintf.h>
#include <vex/panic.h>
#include <vex/string.h>
#include <vex/vmm.h>

extern void x86_isr_00(void);
extern void x86_isr_01(void);
extern void x86_isr_02(void);
extern void x86_isr_03(void);
extern void x86_isr_04(void);
extern void x86_isr_05(void);
extern void x86_isr_06(void);
extern void x86_isr_07(void);
extern void x86_isr_08(void);
extern void x86_isr_09(void);
extern void x86_isr_0A(void);
extern void x86_isr_0B(void);
extern void x86_isr_0C(void);
extern void x86_isr_0D(void);
extern void x86_isr_0E(void);
extern void x86_isr_0F(void);
extern void x86_isr_10(void);
extern void x86_isr_11(void);
extern void x86_isr_12(void);
extern void x86_isr_13(void);
extern void x86_isr_14(void);
extern void x86_isr_15(void);
extern void x86_isr_16(void);
extern void x86_isr_17(void);
extern void x86_isr_18(void);
extern void x86_isr_19(void);
extern void x86_isr_1A(void);
extern void x86_isr_1B(void);
extern void x86_isr_1C(void);
extern void x86_isr_1D(void);
extern void x86_isr_1E(void);
extern void x86_isr_1F(void);

static void page_fault_handler(struct hart_local* restrict hart, struct hart_context* restrict ctx)
{
    struct vmm_fault_info info;
    memset(&info, 0, sizeof(info));

    info.present = ctx->x86_err & 0x1;

    if(ctx->x86_err & 0x2) {
        info.vprot = VPROT_WRITE;
    }
    else if(ctx->x86_err & 0x10) {
        info.vprot = VPROT_EXEC;
    }
    else {
        info.vprot = VPROT_READ;
    }

    if(ctx->x86_err & 0x4) {
        info.vprot |= VPROT_USER;
    }

    asm volatile("movq %%cr2, %0" : "=r"(info.fault));

    struct pagemap* old_vm = &sys_pagemap;
    struct pagemap* new_vm = vmm_handle_fault(old_vm, &info);

    if(new_vm == nullptr) {
        panic("kernel page fault: hart=%hu fault=%p vprot=%02X present=%d", hart->index, (void*)(info.fault), info.vprot, info.present);
        unreachable();
    }
}

void __used x86_isr_handler(struct hart_context* restrict ctx, uint64_t intvec)
{
    disable_interrupts();

    vmm_switch(&sys_pagemap);

    struct hart_local* hart = this_hart();

    if(intvec == 0x0E) {
        page_fault_handler(hart, ctx);
    }
    else {
        panic("idt: unhandled exception 0x%02zX", (size_t)(intvec));
    }
}

static void arch_init_isr(void)
{
    set_idt_entry_kern(0x00, true, &x86_isr_00);
    set_idt_entry_kern(0x01, true, &x86_isr_01);
    set_idt_entry_kern(0x02, true, &x86_isr_02);
    set_idt_entry_kern(0x03, true, &x86_isr_03);
    set_idt_entry_kern(0x04, true, &x86_isr_04);
    set_idt_entry_kern(0x05, true, &x86_isr_05);
    set_idt_entry_kern(0x06, true, &x86_isr_06);
    set_idt_entry_kern(0x07, true, &x86_isr_07);
    set_idt_entry_kern(0x08, true, &x86_isr_08);
    set_idt_entry_kern(0x09, true, &x86_isr_09);
    set_idt_entry_kern(0x0A, true, &x86_isr_0A);
    set_idt_entry_kern(0x0B, true, &x86_isr_0B);
    set_idt_entry_kern(0x0C, true, &x86_isr_0C);
    set_idt_entry_kern(0x0D, true, &x86_isr_0D);
    set_idt_entry_kern(0x0E, true, &x86_isr_0E);
    set_idt_entry_kern(0x0F, true, &x86_isr_0F);
    set_idt_entry_kern(0x10, true, &x86_isr_10);
    set_idt_entry_kern(0x11, true, &x86_isr_11);
    set_idt_entry_kern(0x12, true, &x86_isr_12);
    set_idt_entry_kern(0x13, true, &x86_isr_13);
    set_idt_entry_kern(0x14, true, &x86_isr_14);
    set_idt_entry_kern(0x15, true, &x86_isr_15);
    set_idt_entry_kern(0x16, true, &x86_isr_16);
    set_idt_entry_kern(0x17, true, &x86_isr_17);
    set_idt_entry_kern(0x18, true, &x86_isr_18);
    set_idt_entry_kern(0x19, true, &x86_isr_19);
    set_idt_entry_kern(0x1A, true, &x86_isr_1A);
    set_idt_entry_kern(0x1B, true, &x86_isr_1B);
    set_idt_entry_kern(0x1C, true, &x86_isr_1C);
    set_idt_entry_kern(0x1D, true, &x86_isr_1D);
    set_idt_entry_kern(0x1E, true, &x86_isr_1E);
    set_idt_entry_kern(0x1F, true, &x86_isr_1F);
}

initcall_smp_arch(arch_init_isr, INIT_MIDDLE);
