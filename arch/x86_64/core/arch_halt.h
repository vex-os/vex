#ifndef E0C73D54_5C74_4392_A51F_1A50004B3CEE
#define E0C73D54_5C74_4392_A51F_1A50004B3CEE

#include <core/cdefs.h>

static __always_inline inline void arch_halt_cpu(void)
{
    asm volatile("hlt");
}

#endif /* E0C73D54_5C74_4392_A51F_1A50004B3CEE */
