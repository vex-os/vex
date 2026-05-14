#include <core/amd64.picstub.h>
#include <core/arch.interrupts.h>
#include <core/arch.pmio.h>

#define PIC1_COMMAND 0x0020
#define PIC1_DATA    0x0021
#define PIC2_COMMAND 0x00A0
#define PIC2_DATA    0x00A1

void amd64_init_picstub(void)
{
    unsigned int vector;

    arch_disable_interrupts();

    // We're just making sure that both
    // controllers are disabled in favor
    // of IOAPIC as an external interrupt source
    arch_pmwrite8(PIC1_DATA, 0xFF);
    arch_pmwrite8(PIC2_DATA, 0xFF);

    // Properly disabling both controllers requires
    // sending sixteen end-of-interrupt messages.
    // "Because it's a shit chip" (c) streaksu
    for(vector = 0x00; vector <= 0x10; vector += 1U) {
        if(vector >= 0x08)
            arch_pmwrite8(PIC2_COMMAND, 0x20);
        arch_pmwrite8(PIC1_COMMAND, 0x20);
    }
}
