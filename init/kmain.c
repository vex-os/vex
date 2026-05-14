#include <core/arch.halt.h>
#include <core/arch.interrupts.h>
#include <init/kmain.h>

void __noreturn kmain(void)
{
    // TODO: implement things

    for(;;) {
        arch_disable_interrupts();
        arch_halt_cpu();
    }

    unreachable();
}
