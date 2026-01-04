#ifndef _ARCH_HALT_H
#define _ARCH_HALT_H 1

#include <vex/compiler.h>

static __always_inline inline void halt_cpu(void)
{
    asm volatile("hlt");
}

#endif
