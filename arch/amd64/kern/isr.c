#include <vex.amd64/idt.h>
#include <vex.arch/hart.h>
#include <vex.arch/intr.h>
#include <vex.arch/regs.h>
#include <vex/compiler.h>
#include <vex/initcall.h>
#include <vex/panic.h>
#include <vex/process.h>
#include <vex/signal.h>
#include <vex/string.h>
#include <vex/thread.h>
#include <vex/vmm.h>

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

static const int32_t isr_signals[0x20] = {
    [0x00] = SIGFPE,  // divide error
    [0x01] = SIGTRAP, // debug
    [0x02] = SIGKILL, // non-maskable interrupt
    [0x03] = SIGTRAP, // breakpoint
    [0x04] = SIGSEGV, // overflow (INTO)
    [0x05] = SIGSEGV, // bound range isreeded
    [0x06] = SIGILL,  // invalid opcode
    [0x07] = SIGILL,  // device not available (FPU)
    [0x08] = SIGKILL, // double fault
    [0x09] = SIGBUS,  // coprocessor segment overrun
    [0x0A] = SIGKILL, // invalid tss
    [0x0B] = SIGKILL, // segment not present
    [0x0C] = SIGBUS,  // stack segment fault
    [0x0D] = SIGSEGV, // general protection
    [0x0E] = SIGSEGV, // page fault (handled separately)
    [0x0F] = SIGKILL, // reserved
    [0x10] = SIGFPE,  // x87 floating point
    [0x11] = SIGBUS,  // alignment check
    [0x12] = SIGKILL, // machine check
    [0x13] = SIGFPE,  // SIMD floating point
    [0x14] = SIGKILL, // virtualization
    [0x15] = SIGKILL, // control protection
    [0x16] = SIGKILL, // reserved
    [0x17] = SIGKILL, // reserved
    [0x18] = SIGKILL, // reserved
    [0x19] = SIGKILL, // reserved
    [0x1A] = SIGKILL, // reserved
    [0x1B] = SIGKILL, // reserved
    [0x1C] = SIGKILL, // hypervisor injection
    [0x1D] = SIGKILL, // vmm communication
    [0x1E] = SIGKILL, // security isreption
    [0x1F] = SIGKILL, // reserved
};

static const uint32_t isr_sigcodes[0x20] = {
    [0x00] = FPE_INTDIV,  // divide error
    [0x01] = TRAP_TRACE,  // debug
    [0x03] = TRAP_BRKPT,  // breakpoint
    [0x05] = SEGV_MAPERR, // bound range isreeded
    [0x06] = ILL_ILLOPC,  // invalid opcode
    [0x07] = ILL_COPROC,  // device not available (FPU)
    [0x09] = BUS_OBJERR,  // coprocessor segment overrun
    [0x0C] = BUS_ADRALN,  // stack segment fault
    [0x0D] = SEGV_ACCERR, // general protection
    [0x10] = FPE_FLTINV,  // x87 floating point
    [0x11] = BUS_ADRALN,  // alignment check
    [0x13] = FPE_FLTINV,  // SIMD floating point
};

static void page_fault_handler(struct thread* restrict thrd, struct regfile* restrict regs)
{
    struct vmm_fault_info info;
    memset(&info, 0, sizeof(info));

    info.present = (bool)(regs->amd64_err & 0x1);

    if(regs->amd64_err & 0x2) {
        info.vprot = VPROT_WRITE;
    }
    else if(regs->amd64_err & 0x10) {
        info.vprot = VPROT_EXEC;
    }
    else {
        info.vprot = VPROT_READ;
    }

    if(regs->amd64_err & 0x4) {
        info.vprot |= VPROT_USER;
    }

    asm volatile("movq %%cr2, %0" : "=r"(info.fault));

    struct pagemap* old_vm = thrd->owner ? thrd->owner->vm : &sys_pagemap;
    struct pagemap* new_vm = vmm_handle_fault(old_vm, &info);

    if(new_vm != nullptr) {
        if(thrd->owner)
            thrd->owner->vm = new_vm;
        return;
    }

    if(regs->amd64_cs & 3) {
        struct k_siginfo siginfo;
        memset(&siginfo, 0, sizeof(siginfo));

        siginfo.si_signo = SIGSEGV;
        siginfo.si_code = info.present ? SEGV_ACCERR : SEGV_MAPERR;
        siginfo.si_addr = (void*)(info.fault);

        signal_deliver_sync(thrd, regs, &siginfo);
    }
    else {
        panic("kernel page fault: fault=%p vprot=%02X present=%d", (void*)(info.fault), info.vprot, info.present);
    }
}

void __used amd64_isr_handler(struct regfile* restrict regs, uint64_t intvec)
{
    disable_interrupts();

    vmm_switch(&sys_pagemap);

    struct hart_local* hart = this_hart();
    struct thread* thrd = hart->curr_thread;

    if(intvec == 0x0E) {
        page_fault_handler(thrd, regs);
    }
    else if(regs->amd64_cs & 3) {
        struct k_siginfo siginfo;
        memset(&siginfo, 0, sizeof(siginfo));

        if(intvec < 0x20) {
            siginfo.si_signo = isr_signals[intvec];
            siginfo.si_code = isr_sigcodes[intvec];
        }
        else {
            siginfo.si_signo = SIGSEGV;
            siginfo.si_code = SI_USER;
        }

        signal_deliver_sync(thrd, regs, &siginfo);
    }
    else {
        panic("idt: unhandled exception 0x%02zX", (size_t)(intvec));
    }

    if((regs->amd64_cs & 3) && thrd->owner) {
        vmm_switch(thrd->owner->vm);
    }
}

static void arch_init_isr(void)
{
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
}

initcall_smp_arch(arch_init_isr, INIT_MIDDLE);
