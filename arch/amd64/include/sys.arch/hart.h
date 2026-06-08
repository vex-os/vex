#ifndef B15F19F4_C7E1_479A_A8B7_3E8843A71BA6
#define B15F19F4_C7E1_479A_A8B7_3E8843A71BA6
#pragma once

#include <sys.amd64/tss.h>
#include <sys/cdefs.h>
#include <sys/thread.h>
#include <sys/types.h>

#define AMD64_IST_COUNT 7

struct amd64_gdt;
struct amd64_idt;

struct thread;

struct hart_local {
    struct hart_local* self;
    uint64_t amd64_syscall_ksp;
    uint64_t amd64_syscall_usp;

    struct thread* curr_thread;
    struct thread* idle_thread;
    struct thread* dead_head; // Captain Blazkowicz. Help me make a choice.
    struct thread* runq_head;

    unsigned short index;

    uint32_t amd64_lapic_id;
    uint32_t amd64_lapic_reload;

    struct amd64_gdt* amd64_gdtp;
    struct amd64_idt* amd64_idtp;
    struct amd64_tss* amd64_tssp;

    uint64_t amd64_ist_stacks[AMD64_IST_COUNT];
};

// Static assertions as per arch/amd64/kern/syscall.S
static_assert(8 == offsetof(struct hart_local, amd64_syscall_ksp));
static_assert(16 == offsetof(struct hart_local, amd64_syscall_usp));

static __always_inline __nodiscard struct hart_local* this_hart(void)
{
    struct hart_local* hart;
    asm volatile("movq %%gs:0, %0" : "=r"(hart));
    return hart;
}

static __always_inline void pause_hart(void)
{
    asm volatile("pause");
}

static __always_inline void stop_hart(void)
{
    asm volatile("hlt");
}

static __always_inline void thread_switch_hook(struct hart_local* restrict hart, struct thread* restrict next)
{
    hart->amd64_syscall_ksp = next->kernel_stack_top;
    // hardware interrupts from ring 3 use TSS.rsp[0] for stack switch
    hart->amd64_tssp->rsp[0] = next->kernel_stack_top;
}

void broadcast_panic_ipi(void);

#endif /* B15F19F4_C7E1_479A_A8B7_3E8843A71BA6 */
