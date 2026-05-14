#include <core/arch.halt.h>
#include <core/arch.interrupts.h>
#include <init/kmain.h>

void __noreturn kmain(void)
{
    // TODO: implement things

    for(;;) {
        disable_interrupts();
        halt_cpu();
    }

    unreachable();
}
