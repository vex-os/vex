#ifndef A43893CD_BE1B_41AC_9965_12F522628EF4
#define A43893CD_BE1B_41AC_9965_12F522628EF4
#pragma once

#include <sys/cdefs.h>
#include <sys/types.h>

extern void amd64_thread_trampoline(void);

static __always_inline __nodiscard uintptr_t* setup_thread_kstack(uintptr_t* restrict stack_top, void* restrict entry,
    void* restrict ctxdata)
{
    stack_top[-1] = (uintptr_t)(&amd64_thread_trampoline); // ret --> trampoline
    stack_top[-2] = 0;                                     // rbp
    stack_top[-3] = 0;                                     // rbx
    stack_top[-4] = (uintptr_t)(entry);                    // r12 = entry
    stack_top[-5] = (uintptr_t)(ctxdata);                  // r13 = ctxdata
    stack_top[-6] = 0;                                     // r14
    stack_top[-7] = 0;                                     // r15  <-- thread->kernel_stack points here
    return stack_top - 7;
}

#endif /* A43893CD_BE1B_41AC_9965_12F522628EF4 */
