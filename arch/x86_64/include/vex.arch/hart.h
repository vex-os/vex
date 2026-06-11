#ifndef B15F19F4_C7E1_479A_A8B7_3E8843A71BA6
#define B15F19F4_C7E1_479A_A8B7_3E8843A71BA6
#pragma once

#include <vex.x86/tss.h>
#include <vex/compiler.h>
#include <vex/types.h>

#define AMD64_IST_COUNT 7

struct x86_gdt;
struct x86_idt;

struct hart_local {
    struct hart_local* self;

    unsigned short index;

    uint32_t x86_lapic_id;
    uint32_t x86_lapic_reload;

    struct x86_gdt* x86_gdtp;
    struct x86_idt* x86_idtp;
    struct x86_tss* x86_tssp;

    uint64_t x86_ist_stacks[AMD64_IST_COUNT];
};

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

void broadcast_panic_ipi(void);

#endif /* B15F19F4_C7E1_479A_A8B7_3E8843A71BA6 */
