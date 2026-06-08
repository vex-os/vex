#ifndef A43893CD_BE1B_41AC_9965_12F522628EF4
#define A43893CD_BE1B_41AC_9965_12F522628EF4
#pragma once

#include <sys.amd64/gdt.h>
#include <sys/cdefs.h>
#include <sys/types.h>

extern void amd64_thread_trampoline_kern(void);
extern void amd64_thread_trampoline_user(void);

static __always_inline __nodiscard uintptr_t* setup_thread_kstack(uintptr_t* restrict kstack_top, void* restrict entry,
    void* restrict ctxdata)
{
    kstack_top[-1] = (uintptr_t)(&amd64_thread_trampoline_kern); // ret --> trampoline
    kstack_top[-2] = 0;                                          // rbp
    kstack_top[-3] = 0;                                          // rbx
    kstack_top[-4] = (uintptr_t)(entry);                         // r12 = entry
    kstack_top[-5] = (uintptr_t)(ctxdata);                       // r13 = ctxdata
    kstack_top[-6] = 0;                                          // r14
    kstack_top[-7] = 0;                                          // r15  <-- thread->kernel_stack points here
    return kstack_top - 7;
}

static __always_inline __nodiscard uintptr_t* setup_thread_ustack(uintptr_t* restrict kstack_top, uintptr_t user_entry,
    uintptr_t ustack_top)
{
    kstack_top[-1] = gdt_selector(GDT_USER_DATA_64, 0, 3);       // ss
    kstack_top[-2] = ustack_top;                                 // rsp
    kstack_top[-3] = 0x202;                                      // rflags: IF=1
    kstack_top[-4] = gdt_selector(GDT_USER_CODE_64, 0, 3);       // cs
    kstack_top[-5] = user_entry;                                 // rip
    kstack_top[-6] = (uintptr_t)(&amd64_thread_trampoline_user); // ret --> trampoline
    kstack_top[-7] = 0;                                          // rbp
    kstack_top[-8] = 0;                                          // rbx
    kstack_top[-9] = 0;                                          // r12
    kstack_top[-10] = 0;                                         // r13
    kstack_top[-11] = 0;                                         // r14
    kstack_top[-12] = 0;                                         // r15  <-- thread->kernel_stack
    return kstack_top - 12;
}

#endif /* A43893CD_BE1B_41AC_9965_12F522628EF4 */
