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

    // Mask all the interrupt lines
    pmio_write8(PIC1_DATA, 0xFF);
    pmio_write8(PIC2_DATA, 0xFF);

    //      und: Just mask and forget?
    // streaksu: Mask all the PIC interrupts
    // streaksu: EOI all 16 PIC vectors because it's a shit chip
    for(vector = 0x00; vector <= 0x10; ++vector) {
        if(vector >= 0x08)
            pmio_write8(PIC2_COMMAND, 0x20);
        pmio_write8(PIC1_COMMAND, 0x20);
    }
}
