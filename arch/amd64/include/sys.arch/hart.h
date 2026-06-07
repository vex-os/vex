#ifndef B15F19F4_C7E1_479A_A8B7_3E8843A71BA6
#define B15F19F4_C7E1_479A_A8B7_3E8843A71BA6
#pragma once

#include <sys/cdefs.h>
#include <sys/types.h>

#define AMD64_IST_COUNT 7

struct amd64_gdt;
struct amd64_idt;
struct amd64_tss;

struct sched_task;

struct hart_local {
    struct hart_local* self;
    struct sched_task* task;
    struct sched_task* rq;
    unsigned index;

    struct {
        unsigned lapic_id;
        struct amd64_gdt* gdt;
        struct amd64_idt* idt;
        struct amd64_tss* tss;
        uintptr_t ist_stacks[AMD64_IST_COUNT];
    } amd64;
};

static __always_inline __nodiscard struct hart_local* this_hart(void)
{
    struct hart_local* hart;
    asm volatile("movq %%gs:0, %0" : "=r"(hart));
    return hart;
}

static __always_inline void yield_hart(void)
{
    asm volatile("pause");
}

static __always_inline void stop_hart(void)
{
    asm volatile("hlt");
}

void broadcast_panic_ipi(void);

#endif /* B15F19F4_C7E1_479A_A8B7_3E8843A71BA6 */
