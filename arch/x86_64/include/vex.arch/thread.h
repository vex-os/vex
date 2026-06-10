#ifndef A43893CD_BE1B_41AC_9965_12F522628EF4
#define A43893CD_BE1B_41AC_9965_12F522628EF4
#pragma once

#include <vex.x86/gdt.h>
#include <vex/compiler.h>
#include <vex/types.h>

extern void x86_thread_trampoline_kern(void);
extern void x86_thread_trampoline_user(void);

static __always_inline __nodiscard uintptr_t* setup_thread_kstack(uintptr_t* restrict kernel_stack_top, void* restrict entry,
    void* restrict ctxdata)
{
    kernel_stack_top[-1] = (uintptr_t)(&x86_thread_trampoline_kern); // ret --> trampoline
    kernel_stack_top[-2] = 0;                                        // rbp
    kernel_stack_top[-3] = 0;                                        // rbx
    kernel_stack_top[-4] = (uintptr_t)(entry);                       // r12 = entry
    kernel_stack_top[-5] = (uintptr_t)(ctxdata);                     // r13 = ctxdata
    kernel_stack_top[-6] = 0;                                        // r14
    kernel_stack_top[-7] = 0;                                        // r15  <-- thrd->kernel_stack points here
    return kernel_stack_top - 7;
}

static __always_inline __nodiscard uintptr_t* setup_thread_ustack(uintptr_t* restrict kernel_stack_top, uintptr_t user_entry,
    uintptr_t user_stack_top)
{
    kernel_stack_top[-1] = gdt_selector(GDT_USER_DATA_64, 0, 3);     // ss
    kernel_stack_top[-2] = user_stack_top;                           // rsp
    kernel_stack_top[-3] = 0x202;                                    // rflags: IF=1
    kernel_stack_top[-4] = gdt_selector(GDT_USER_CODE_64, 0, 3);     // cs
    kernel_stack_top[-5] = user_entry;                               // rip
    kernel_stack_top[-6] = (uintptr_t)(&x86_thread_trampoline_user); // ret --> trampoline
    kernel_stack_top[-7] = 0;                                        // rbp
    kernel_stack_top[-8] = 0;                                        // rbx
    kernel_stack_top[-9] = 0;                                        // r12
    kernel_stack_top[-10] = 0;                                       // r13
    kernel_stack_top[-11] = 0;                                       // r14
    kernel_stack_top[-12] = 0;                                       // r15  <-- thrd->kernel_stack
    return kernel_stack_top - 12;
}

#endif /* A43893CD_BE1B_41AC_9965_12F522628EF4 */
