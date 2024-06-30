// SPDX-License-Identifier: Zlib
#include <arch/8259.h>
#include <arch/intreq.h>
#include <arch/limits.h>
#include <arch/pmio.h>
#include <kern/printf.h>

#define PIC1_COMMAND    0x0020
#define PIC1_DATA       0x0021
#define PIC2_COMMAND    0x00A0
#define PIC2_DATA       0x00A1

void init_8259(void)
{
    unsigned int vector;

    disable_interrupts();

    // We're just making sure that both
    // controllers are disabled in favor
    // of IOAPIC as an external interrupt source
    pmio_write8(PIC1_DATA, 0xFF);
    pmio_write8(PIC2_DATA, 0xFF);

    // Properly disabling both controllers requires
    // sending sixteen end-of-interrupt messages.
    //  Because it's a shit chip (c) streaksu
    for(vector = 0x00; vector <= 0x10; ++vector) {
        if(vector >= 0x08)
            pmio_write8(PIC2_COMMAND, 0x20);
        pmio_write8(PIC1_COMMAND, 0x20);
    }
}
