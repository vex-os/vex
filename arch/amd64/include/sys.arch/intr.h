#ifndef FD177992_9E0F_4161_81C4_364624A1E1F6
#define FD177992_9E0F_4161_81C4_364624A1E1F6
#pragma once

#include <sys/cdefs.h>
#include <sys/types.h>

struct regfile;

static __always_inline bool disable_interrupts(void)
{
    uint64_t rflags;
    asm volatile("pushfq");
    asm volatile("pop %0" : "=r"(rflags));
    asm volatile("cli");
    return rflags & UINT64_C(0x0000000000000200);
}

static __always_inline bool enable_interrupts(void)
{
    uint64_t rflags;
    asm volatile("pushfq");
    asm volatile("pop %0" : "=r"(rflags));
    asm volatile("sti");
    return rflags & UINT64_C(0x0000000000000200);
}

static __always_inline void restore_interrupts(bool state)
{
    if(state) {
        asm volatile("sti");
    }
    else {
        asm volatile("cli");
    }
}

#endif /* FD177992_9E0F_4161_81C4_364624A1E1F6 */
