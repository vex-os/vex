// SPDX-License-Identifier: BSD-3-Clause

#ifndef B15F19F4_C7E1_479A_A8B7_3E8843A71BA6
#define B15F19F4_C7E1_479A_A8B7_3E8843A71BA6
#pragma once

#include <core/cdefs.h>
#include <stdint.h>

#define AMD64_IST_COUNT 7

struct amd64_gdt;
struct amd64_idt;
struct amd64_tss;

struct hart_local {
    struct hart_local* self;
    unsigned int index;
    struct {
        unsigned int lapic_id;
        struct amd64_gdt* gdt;
        struct amd64_idt* idt;
        struct amd64_tss* tss;
        uintptr_t ist_stacks[AMD64_IST_COUNT];
    } amd64;
};

static __always_inline __nodiscard inline struct hart_local* this_hart(void)
{
    struct hart_local* hart;
    asm volatile("movq %%gs:0, %0" : "=r"(hart));
    return hart;
}

static __always_inline inline void yield_hart(void)
{
    asm volatile("pause");
}

static __always_inline inline void stop_hart(void)
{
    asm volatile("hlt");
}

extern void halt_other_harts(void);

#endif /* B15F19F4_C7E1_479A_A8B7_3E8843A71BA6 */
