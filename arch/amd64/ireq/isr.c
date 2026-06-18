#include <arch/amd64.idt.h>
#include <vex/compiler.h>
#include <vex/hart.h>
#include <vex/initcall.h>
#include <vex/kprintf.h>
#include <vex/mmu.h>
#include <vex/panic.h>
#include <vex/process.h>
#include <vex/sched.h>
#include <vex/string.h>
#include <vex/thread.h>
#include <vex/vmmap.h>

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
    struct page_fault_info info;
    memset(&info, 0, sizeof(info));

    info.present = ctx->amd64_err & 0x1;

    if(ctx->amd64_err & 0x2) {
        info.vprot = VPROT_WRITE;
    }
    else if(ctx->amd64_err & 0x10) {
        info.vprot = VPROT_EXEC;
    }
    else {
        info.vprot = VPROT_READ;
    }

    if(ctx->amd64_err & 0x4) {
        info.vprot |= VPROT_USER;
    }

    asm volatile("movq %%cr2, %0" : "=r"(info.fault));

    struct thread* thrd = hart->curr_thread;
    struct process* proc = thrd->proc;

    if(proc == nullptr && 3 == (ctx->amd64_cs & 3)) {
        sched_terminate();
        unreachable();
    }

    struct mmu_ctx* old_ctx;
    struct mmu_ctx* new_ctx;

    if(3 == (ctx->amd64_cs & 3)) {
        old_ctx = proc->map->ctx;
        new_ctx = mmu_page_fault(old_ctx, &info);

        if(new_ctx != nullptr) {
            proc->map->ctx = new_ctx;
            return;
        }

        process_exit(proc, -1);
        sched_terminate();
        unreachable();
    }
    else {
        old_ctx = &sys_mmu_ctx;
        new_ctx = mmu_page_fault(old_ctx, &info);

        if(new_ctx) {
            mmu_switch_ctx(new_ctx);
            return;
        }

        panic("kernel page fault: hart=%hu fault=%p vprot=%02X present=%d", hart->index, (void*)(info.fault), info.vprot, info.present);
    }
}

void __used amd64_isr_handler(struct hart_context* restrict ctx, uint64_t intvec)
{
    disable_interrupts();

    mmu_switch_ctx(&sys_mmu_ctx);

    struct hart_local* hart = this_hart();

    if(intvec == 0x0E) {
        page_fault_handler(hart, ctx);
        return;
    }

    if(3 == (ctx->amd64_cs & 3)) {
        struct thread* thrd = hart->curr_thread;
        struct process* proc = thrd->proc;

        if(proc) {
            process_exit(proc, -1);
        }

        sched_terminate();
        unreachable();
    }

    panic("idt: unhandled exception 0x%02zX", (size_t)(intvec));
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
