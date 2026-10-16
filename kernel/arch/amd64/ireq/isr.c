#include <arch/amd64.idt.h>
#include <vex/compiler.h>
#include <vex/hart.h>
#include <vex/initcall.h>
#include <vex/kprintf.h>
#include <vex/kstack.h>
#include <vex/panic.h>
#include <vex/pmap.h>
#include <vex/process.h>
#include <vex/sched.h>
#include <vex/string.h>
#include <vex/thread.h>
#include <vex/vms.h>

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

static void page_fault_handler(struct hart_local* restrict hart, struct hart_context* restrict ctx)
{
    uintptr_t fault;
    asm volatile("movq %%cr2, %0" : "=r"(fault));

    unsigned access;

    if(ctx->amd64_err & 0x2) {
        access = VPROT_WRITE;
    }
    else if(ctx->amd64_err & 0x10) {
        access = VPROT_EXEC;
    }
    else {
        access = VPROT_READ;
    }

    if(3 != (ctx->amd64_cs & 3)) {
        panic("kernel page fault: hart=%hu fault=%p rip=%p access=%02X present=%d", hart->index, (void*)(fault), (void*)(ctx->amd64_rip),
            access, (int)(ctx->amd64_err & 0x1));
    }

    struct thread* thrd = hart->curr_thread;
    struct process* proc = thrd->proc;

    if(proc == nullptr) {
        sched_terminate();
        unreachable();
    }

    // A fault from ring 3 holds
    // no kernel locks, so resolving
    // or exiting may sleep
    enable_interrupts();

    if(0 == vms_fault(proc->vms, fault, access)) {
        // The way back to ring 3 swaps GS
        // and an interrupt landing after
        // that would take the user GS for the kernel GS
        disable_interrupts();
        return;
    }

    process_exit(proc, -1);
    sched_terminate();
    unreachable();
}

void __used amd64_isr_handler(struct hart_context* restrict ctx, uint64_t intvec)
{
    disable_interrupts();

    struct hart_local* hart = this_hart();

    // Harts only get set up at the multicore stage
    // so anything happening before that quite literally
    // means it's joever and we're all gonna die
    if(hart == nullptr) {
        uintptr_t fault;
        asm volatile("movq %%cr2, %0" : "=r"(fault));
        panic("early exception 0x%02zX: rip=%p cr2=%p err=0x%zX", (size_t)(intvec), (void*)(ctx->amd64_rip), (void*)(fault),
            (size_t)(ctx->amd64_err));
    }

    if(intvec == 0x0E) {
        page_fault_handler(hart, ctx);
        return;
    }

    if(intvec == 0x08) {
        uintptr_t fault;
        asm volatile("movq %%cr2, %0" : "=r"(fault));

        if(kstack_is_guard(fault)) {
            panic("kernel stack overflow: hart=%hu fault=%p rip=%p", hart->index, (void*)(fault), (void*)(ctx->amd64_rip));
        }

        panic("double fault: hart=%hu rip=%p", hart->index, (void*)(ctx->amd64_rip));
    }

    if(3 == (ctx->amd64_cs & 3)) {
        struct thread* thrd = hart->curr_thread;
        struct process* proc = thrd->proc;

        if(proc) {
            enable_interrupts();
            process_exit(proc, -1);
        }

        sched_terminate();
        unreachable();
    }

    panic("idt: unhandled exception 0x%02zX", (size_t)(intvec));
}

static void arch_init_isr(void)
{
    set_idt_entry_kern(0x00, false, &amd64_isr_00);
    set_idt_entry_kern(0x01, false, &amd64_isr_01);
    set_idt_entry_ist(0x02, IDT_IST_NMI, &amd64_isr_02);
    set_idt_entry_kern(0x03, false, &amd64_isr_03);
    set_idt_entry_kern(0x04, false, &amd64_isr_04);
    set_idt_entry_kern(0x05, false, &amd64_isr_05);
    set_idt_entry_kern(0x06, false, &amd64_isr_06);
    set_idt_entry_kern(0x07, false, &amd64_isr_07);
    set_idt_entry_ist(0x08, IDT_IST_DOUBLE_FAULT, &amd64_isr_08);
    set_idt_entry_kern(0x09, false, &amd64_isr_09);
    set_idt_entry_kern(0x0A, false, &amd64_isr_0A);
    set_idt_entry_kern(0x0B, false, &amd64_isr_0B);
    set_idt_entry_kern(0x0C, false, &amd64_isr_0C);
    set_idt_entry_kern(0x0D, false, &amd64_isr_0D);
    set_idt_entry_kern(0x0E, false, &amd64_isr_0E);
    set_idt_entry_kern(0x0F, false, &amd64_isr_0F);
    set_idt_entry_kern(0x10, false, &amd64_isr_10);
    set_idt_entry_kern(0x11, false, &amd64_isr_11);
    set_idt_entry_ist(0x12, IDT_IST_MACHINE_CHECK, &amd64_isr_12);
    set_idt_entry_kern(0x13, false, &amd64_isr_13);
    set_idt_entry_kern(0x14, false, &amd64_isr_14);
    set_idt_entry_kern(0x15, false, &amd64_isr_15);
    set_idt_entry_kern(0x16, false, &amd64_isr_16);
    set_idt_entry_kern(0x17, false, &amd64_isr_17);
    set_idt_entry_kern(0x18, false, &amd64_isr_18);
    set_idt_entry_kern(0x19, false, &amd64_isr_19);
    set_idt_entry_kern(0x1A, false, &amd64_isr_1A);
    set_idt_entry_kern(0x1B, false, &amd64_isr_1B);
    set_idt_entry_kern(0x1C, false, &amd64_isr_1C);
    set_idt_entry_kern(0x1D, false, &amd64_isr_1D);
    set_idt_entry_kern(0x1E, false, &amd64_isr_1E);
    set_idt_entry_kern(0x1F, false, &amd64_isr_1F);
}

initcall_bsp_arch(arch_init_isr, INIT_MIDDLE);
