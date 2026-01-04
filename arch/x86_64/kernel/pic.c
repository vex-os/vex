#include <arch/ireq.h>
#include <arch/pic.h>
#include <arch/pmio.h>

#define PIC1_COMMAND 0x0020
#define PIC1_DATA    0x0021
#define PIC2_COMMAND 0x00A0
#define PIC2_DATA    0x00A1

void init_pic(void)
{
    unsigned int vector;

    disable_interrupts();

    // We're just making sure that both
    // controllers are disabled in favor
    // of IOAPIC as an external interrupt source
    pmwrite8(PIC1_DATA, 0xFF);
    pmwrite8(PIC2_DATA, 0xFF);

    // Properly disabling both controllers requires
    // sending sixteen end-of-interrupt messages.
    // "Because it's a shit chip" (c) streaksu
    for(vector = 0x00; vector <= 0x10; vector += 1U) {
        if(vector >= 0x08)
            pmwrite8(PIC2_COMMAND, 0x20);
        pmwrite8(PIC1_COMMAND, 0x20);
    }
}
