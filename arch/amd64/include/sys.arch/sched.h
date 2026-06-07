#ifndef D718BA46_38D6_497E_BAA7_5BE2DEA1BC9D
#define D718BA46_38D6_497E_BAA7_5BE2DEA1BC9D
#pragma once

#include <sys/cdefs.h>
#include <sys/types.h>

void amd64_task_trampoline(void);

static __always_inline __nodiscard uintptr_t* setup_task_stack(uintptr_t* restrict stack_ptr, void* restrict entry_addr)
{
    stack_ptr[-1] = (uintptr_t)(&amd64_task_trampoline); // ret --> trampoline (does sti; jmp rbp)
    stack_ptr[-2] = (uintptr_t)(entry_addr);             // rbp = entry_addr
    stack_ptr[-3] = 0;                                   // rbx
    stack_ptr[-4] = 0;                                   // r12
    stack_ptr[-5] = 0;                                   // r13
    stack_ptr[-6] = 0;                                   // r14
    stack_ptr[-7] = 0;                                   // r15  <-- task->kstack points here
    return stack_ptr - 7;
}

#endif /* D718BA46_38D6_497E_BAA7_5BE2DEA1BC9D */
